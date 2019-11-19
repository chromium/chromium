// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_switches.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace feedback {

namespace {

constexpr base::FilePath::CharType kFeedbackReportPath[] =
    FILE_PATH_LITERAL("Feedback Reports");

constexpr char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";

constexpr char kProtoBufMimeType[] = "application/x-protobuf";

constexpr int kHttpPostSuccessNoContent = 204;
constexpr int kHttpPostFailNoConnection = -1;
constexpr int kHttpPostFailClientError = 400;
constexpr int kHttpPostFailServerError = 500;

// The minimum time to wait before uploading reports are retried. Exponential
// backoff delay is applied on successive failures.
// This value can be overriden by tests by calling
// FeedbackUploader::SetMinimumRetryDelayForTesting().
base::TimeDelta g_minimum_retry_delay = base::TimeDelta::FromMinutes(60);

// If a new report is queued to be dispatched immediately while another is being
// dispatched, this is the time to wait for the on-going dispatching to finish.
base::TimeDelta g_dispatching_wait_delay = base::TimeDelta::FromSeconds(4);

base::FilePath GetPathFromContext(content::BrowserContext* context) {
  return context->GetPath().Append(kFeedbackReportPath);
}

GURL GetFeedbackPostGURL() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return GURL(command_line.HasSwitch(switches::kFeedbackServer)
                  ? command_line.GetSwitchValueASCII(switches::kFeedbackServer)
                  : kFeedbackPostUrl);
}

}  // namespace

FeedbackUploader::FeedbackUploader(
    content::BrowserContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_(context),
      feedback_reports_path_(GetPathFromContext(context)),
      task_runner_(task_runner),
      feedback_post_url_(GetFeedbackPostGURL()),
      retry_delay_(g_minimum_retry_delay),
      is_dispatching_(false) {
  DCHECK(task_runner_);
  DCHECK(context_);
}

FeedbackUploader::~FeedbackUploader() {}

// static
void FeedbackUploader::SetMinimumRetryDelayForTesting(base::TimeDelta delay) {
  g_minimum_retry_delay = delay;
}

void FeedbackUploader::QueueReport(std::unique_ptr<std::string> data) {
  reports_queue_.emplace(base::MakeRefCounted<FeedbackReport>(
      feedback_reports_path_, base::Time::Now(), std::move(data),
      task_runner_));
  UpdateUploadTimer();
}

void FeedbackUploader::RequeueReport(scoped_refptr<FeedbackReport> report) {
  DCHECK_EQ(task_runner_, report->reports_task_runner());
  report->set_upload_at(base::Time::Now());
  reports_queue_.emplace(std::move(report));
  UpdateUploadTimer();
}

void FeedbackUploader::StartDispatchingReport() {
  DispatchReport();
}

void FeedbackUploader::OnReportUploadSuccess() {
  retry_delay_ = g_minimum_retry_delay;
  is_dispatching_ = false;
  // Explicitly release the successfully dispatched report.
  report_being_dispatched_->DeleteReportOnDisk();
  report_being_dispatched_ = nullptr;
  UpdateUploadTimer();
}

void FeedbackUploader::OnReportUploadFailure(bool should_retry) {
  if (should_retry) {
    // Implement a backoff delay by doubling the retry delay on each failure.
    retry_delay_ *= 2;
    report_being_dispatched_->set_upload_at(retry_delay_ + base::Time::Now());
    reports_queue_.emplace(report_being_dispatched_);
  } else {
    // The report won't be retried, hence explicitly delete its file on disk.
    report_being_dispatched_->DeleteReportOnDisk();
  }

  // The report dispatching failed, and should either be retried or not. In all
  // cases, we need to release |report_being_dispatched_|. If it was up for
  // retry, then it has already been re-enqueued and will be kept alive.
  // Otherwise we're done with it and it should destruct.
  report_being_dispatched_ = nullptr;
  is_dispatching_ = false;
  UpdateUploadTimer();
}

bool FeedbackUploader::ReportsUploadTimeComparator::operator()(
    const scoped_refptr<FeedbackReport>& a,
    const scoped_refptr<FeedbackReport>& b) const {
  return a->upload_at() > b->upload_at();
}

void FeedbackUploader::AppendExtraHeadersToUploadRequest(
    network::ResourceRequest* resource_request) {}

void FeedbackUploader::DispatchReport() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_feedback_report_app", R"(
        semantics {
          sender: "Chrome Feedback Report App"
          description:
            "Users can press Alt+Shift+i to report a bug or a feedback in "
            "general. Along with the free-form text they entered, system logs "
            "that helps in diagnosis of the issue are sent to Google. This "
            "service uploads the report to Google Feedback server."
          trigger:
            "When user chooses to send a feedback to Google."
          data:
            "The free-form text that user has entered and useful debugging "
            "logs (UI logs, Chrome logs, kernel logs, auto update engine logs, "
            "ARC++ logs, etc.). The logs are anonymized to remove any "
            "user-private data. The user can view the system information "
            "before sending, and choose to send the feedback report without "
            "system information and the logs (unchecking 'Send system "
            "information' prevents sending logs as well), the screenshot, or "
            "even his/her email address."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings and is only activated "
            "by direct user request."
          policy_exception_justification: "Not implemented."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = feedback_post_url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  // Tell feedback server about the variation state of this install.
  variations::AppendVariationsHeaderUnknownSignedIn(
      feedback_post_url_,
      context_->IsOffTheRecord() ? variations::InIncognito::kYes
                                 : variations::InIncognito::kNo,
      resource_request.get());

  AppendExtraHeadersToUploadRequest(resource_request.get());

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  simple_url_loader->AttachStringForUpload(report_being_dispatched_->data(),
                                           kProtoBufMimeType);
  auto it = uploads_in_progress_.insert(uploads_in_progress_.begin(),
                                        std::move(simple_url_loader));

  // Creating the StoragePartitionImpl is costly, so don't do it until
  // necessary (most importantly, avoid doing so during startup).
  if (!url_loader_factory_) {
    url_loader_factory_ =
        content::BrowserContext::GetDefaultStoragePartition(context_)
            ->GetURLLoaderFactoryForBrowserProcess();
  }

  simple_url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&FeedbackUploader::OnDispatchComplete,
                     base::Unretained(this), std::move(it)));
}

void FeedbackUploader::OnDispatchComplete(
    UrlLoaderList::iterator it,
    std::unique_ptr<std::string> response_body) {
  std::stringstream error_stream;
  network::SimpleURLLoader* simple_url_loader = it->get();
  int response_code = kHttpPostFailNoConnection;
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();
  }
  if (response_code == kHttpPostSuccessNoContent) {
    error_stream << "Success";
    OnReportUploadSuccess();
  } else {
    bool should_retry = true;
    // Process the error for debug output
    if (response_code == kHttpPostFailNoConnection) {
      error_stream << "No connection to server.";
    } else if ((response_code >= kHttpPostFailClientError) &&
               (response_code < kHttpPostFailServerError)) {
      // Client errors mean that the server failed to parse the proto that was
      // sent, or that some requirements weren't met by the server side
      // validation, and hence we should NOT retry sending this corrupt report
      // and give up.
      should_retry = false;

      error_stream << "Client error: HTTP response code " << response_code;
    } else if (response_code >= kHttpPostFailServerError) {
      error_stream << "Server error: HTTP response code " << response_code;
    } else {
      error_stream << "Unknown error: HTTP response code " << response_code;
    }

    OnReportUploadFailure(should_retry);
  }

  LOG(WARNING) << "FEEDBACK: Submission to feedback server ("
               << simple_url_loader->GetFinalURL()
               << ") status: " << error_stream.str();
  uploads_in_progress_.erase(it);
}

void FeedbackUploader::UpdateUploadTimer() {
  if (reports_queue_.empty())
    return;

  scoped_refptr<FeedbackReport> report = reports_queue_.top();
  const base::Time now = base::Time::Now();
  if (report->upload_at() <= now && !is_dispatching_) {
    reports_queue_.pop();
    is_dispatching_ = true;
    report_being_dispatched_ = report;
    StartDispatchingReport();
  } else {
    // Stop the old timer and start an updated one.
    const base::TimeDelta delay = (is_dispatching_ || now > report->upload_at())
                                      ? g_dispatching_wait_delay
                                      : report->upload_at() - now;
    upload_timer_.Stop();
    upload_timer_.Start(FROM_HERE, delay, this,
                        &FeedbackUploader::UpdateUploadTimer);
  }
}

}  // namespace feedback
