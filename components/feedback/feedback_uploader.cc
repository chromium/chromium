// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/feedback/features.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_switches.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace feedback {

namespace {

constexpr char kReportSendingResultHistogramName[] =
    "Feedback.ReportSending.Result";
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FeedbackReportSendingResult {
  kSuccessAtFirstTry = 0,  // The report was uploaded successfully without retry
  kSuccessAfterRetry = 1,  // The report was uploaded successfully after retry
  kDropped = 2,            // The report is corrupt or invalid and was dropped
  kMaxValue = kDropped,
};

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
base::TimeDelta g_minimum_retry_delay = base::Minutes(60);

// If a new report is queued to be dispatched immediately while another is being
// dispatched, this is the time to wait for the on-going dispatching to finish.
base::TimeDelta g_dispatching_wait_delay = base::Seconds(4);

GURL GetFeedbackPostGURL() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return GURL(command_line.HasSwitch(switches::kFeedbackServer)
                  ? command_line.GetSwitchValueASCII(switches::kFeedbackServer)
                  : kFeedbackPostUrl);
}

// Creates a new SingleThreadTaskRunner that is used to run feedback blocking
// background work.
scoped_refptr<base::SingleThreadTaskRunner> CreateUploaderTaskRunner() {
  // Uses a BLOCK_SHUTDOWN file task runner to prevent losing reports or
  // corrupting report's files.
  return base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

}  // namespace

FeedbackUploader::FeedbackUploader(
    bool is_off_the_record,
    const base::FilePath& state_path,
    SharedURLLoaderFactoryGetter shared_url_loader_factory_getter)
    : FeedbackUploader(is_off_the_record,
                       state_path,
                       std::move(shared_url_loader_factory_getter),
                       nullptr) {}

FeedbackUploader::FeedbackUploader(
    bool is_off_the_record,
    const base::FilePath& state_path,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : FeedbackUploader(is_off_the_record,
                       state_path,
                       SharedURLLoaderFactoryGetter(),
                       shared_url_loader_factory) {}

FeedbackUploader::~FeedbackUploader() = default;

// static
void FeedbackUploader::SetMinimumRetryDelayForTesting(base::TimeDelta delay) {
  g_minimum_retry_delay = delay;
}

void FeedbackUploader::QueueReport(std::unique_ptr<std::string> data,
                                   bool has_email,
                                   int product_id) {
  reports_queue_.emplace(base::MakeRefCounted<FeedbackReport>(
      feedback_reports_path_, base::Time::Now(), std::move(data), task_runner_,
      has_email, product_id));
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
  if (retry_delay_ == g_minimum_retry_delay) {
    UMA_HISTOGRAM_ENUMERATION(kReportSendingResultHistogramName,
                              FeedbackReportSendingResult::kSuccessAtFirstTry);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kReportSendingResultHistogramName,
                              FeedbackReportSendingResult::kSuccessAfterRetry);
  }
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
    UMA_HISTOGRAM_ENUMERATION(kReportSendingResultHistogramName,
                              FeedbackReportSendingResult::kDropped);
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

FeedbackUploader::FeedbackUploader(
    bool is_off_the_record,
    const base::FilePath& state_path,
    SharedURLLoaderFactoryGetter url_loader_factory_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_getter_(std::move(url_loader_factory_getter)),
      url_loader_factory_(url_loader_factory),
      feedback_reports_path_(state_path.Append(kFeedbackReportPath)),
      task_runner_(CreateUploaderTaskRunner()),
      feedback_post_url_(GetFeedbackPostGURL()),
      retry_delay_(g_minimum_retry_delay),
      is_off_the_record_(is_off_the_record) {
  DCHECK(!!url_loader_factory_getter_ != !!url_loader_factory_);
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
            "ARC++ logs, etc.). The logs are redacted to remove any "
            "user-private data. The user can view the system information "
            "before sending, and choose to send the feedback report without "
            "system information and the logs (unchecking 'Send system "
            "information' prevents sending logs as well), the screenshot, or "
            "even his/her email address."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "cros-device-enablement@google.com"
            }
          }
          user_data {
            type: ARBITRARY_DATA
            type: EMAIL
            type: IMAGE
            type: USER_CONTENT
          }
          last_reviewed: "2023-08-14"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings and is only activated "
            "by direct user request."
          chrome_policy {
            UserFeedbackAllowed {
              UserFeedbackAllowed: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = feedback_post_url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  // Tell feedback server about the variation state of this install.
  if (report_being_dispatched_->should_include_variations()) {
    variations::AppendVariationsHeaderUnknownSignedIn(
        feedback_post_url_,
        is_off_the_record_ ? variations::InIncognito::kYes
                           : variations::InIncognito::kNo,
        resource_request.get());
  }

  if (report_being_dispatched_->has_email()) {
    AppendExtraHeadersToUploadRequest(resource_request.get());
  }

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  simple_url_loader->AttachStringForUpload(report_being_dispatched_->data(),
                                           kProtoBufMimeType);
  auto it = uploads_in_progress_.insert(uploads_in_progress_.begin(),
                                        std::move(simple_url_loader));

  if (!url_loader_factory_) {
    // Lazily create the URLLoaderFactory.
    url_loader_factory_ = std::move(url_loader_factory_getter_).Run();
    DCHECK(url_loader_factory_);
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

  // Don't send reports in Tast tests so that they don't spam Listnr.
  if (feedback::features::IsSkipSendingFeedbackReportInTastTestsEnabled()) {
    report->DeleteReportOnDisk();
    reports_queue_.pop();
    return;
  }

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
