// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

namespace policy {

namespace {

// Helper that forwards a job cancelation confirmation from the thread that the
// ExternalPolicyDataFetcherBackend runs on to the thread that the
// ExternalPolicyDataFetcher which canceled the job runs on.
void ForwardJobCanceled(scoped_refptr<base::SequencedTaskRunner> task_runner,
                        base::OnceClosure callback) {
  task_runner->PostTask(FROM_HERE, std::move(callback));
}

}  // namespace

class ExternalPolicyDataFetcher::Job
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  Job(base::WeakPtr<ExternalPolicyDataFetcher> fetcher,
      scoped_refptr<base::SequencedTaskRunner> frontend_task_runner,
      ExternalPolicyDataFetcher::FetchCallback callback);

  void Start(network::mojom::URLLoaderFactory* url_loader_factory,
             const GURL& url,
             int64_t max_size);
  void Cancel();
  void OnResponseStarted(const GURL& final_url,
                         const network::ResourceResponseHead& response_head);

  // network::SimpleURLLoaderStreamConsumer implementation
  void OnDataReceived(base::StringPiece string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

 private:
  void ReportFinished(Result result, std::unique_ptr<std::string> data);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<ExternalPolicyDataFetcher> fetcher_;
  scoped_refptr<base::SequencedTaskRunner> frontend_task_runner_;
  ExternalPolicyDataFetcher::FetchCallback callback_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::string response_body_;
  int64_t max_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

ExternalPolicyDataFetcher::Job::Job(
    base::WeakPtr<ExternalPolicyDataFetcher> fetcher,
    scoped_refptr<base::SequencedTaskRunner> frontend_task_runner,
    ExternalPolicyDataFetcher::FetchCallback callback)
    : fetcher_(std::move(fetcher)),
      frontend_task_runner_(std::move(frontend_task_runner)),
      callback_(std::move(callback)) {
  // A job is created on the frontend thread but receives callbacks on the
  // backend's sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ExternalPolicyDataFetcher::Job::Start(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& url,
    int64_t max_size) {
  DCHECK_GE(max_size, 0);
  max_size_ = max_size;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SEND_AUTH_DATA;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("external_policy_fetcher", R"(
        semantics {
          sender: "Cloud Policy"
          description:
            "Used to fetch policy for extensions, policy-controlled wallpaper, "
            "and custom terms of service."
          trigger:
            "Periodically loaded when a managed user is signed in to Chrome."
          data:
            "This request does not send any data. It loads external resources "
            "by a unique URL provided by the admin."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be controlled by Chrome settings, but users "
            "can sign out of Chrome to disable it."
          policy_exception_justification:
            "Not implemented, considered not useful. This request is part of "
            "the policy fetcher itself."
        })");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader_->SetOnResponseStartedCallback(
      base::BindOnce(&ExternalPolicyDataFetcher::Job::OnResponseStarted,
                     base::Unretained(this)));
  url_loader_->DownloadAsStream(url_loader_factory, this);

  // TODO(https://crbug.com/808498): Use ServiceURLLoader to flag data usage as
  // data_use_measurement::DataUseUserData::POLICY.
}

void ExternalPolicyDataFetcher::Job::Cancel() {
  url_loader_.reset();
}

void ExternalPolicyDataFetcher::Job::OnResponseStarted(
    const GURL& /* final_url */,
    const network::ResourceResponseHead& response_head) {
  if (response_head.content_length != -1 &&
      response_head.content_length > max_size_) {
    url_loader_.reset();
    ReportFinished(MAX_SIZE_EXCEEDED, nullptr);
    return;
  }
}

void ExternalPolicyDataFetcher::Job::OnDataReceived(
    base::StringPiece string_piece,
    base::OnceClosure resume) {
  if (response_body_.length() + string_piece.length() >
      static_cast<uint64_t>(max_size_)) {
    url_loader_.reset();
    ReportFinished(MAX_SIZE_EXCEEDED, nullptr);
    return;
  }

  response_body_.append(string_piece.data(), string_piece.length());
  std::move(resume).Run();
}

void ExternalPolicyDataFetcher::Job::OnComplete(bool /* success */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<network::SimpleURLLoader> url_loader = std::move(url_loader_);

  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();

  Result result;
  std::unique_ptr<std::string> data;

  if (url_loader->NetError() == net::ERR_CONNECTION_RESET ||
      url_loader->NetError() == net::ERR_TEMPORARILY_THROTTLED ||
      url_loader->NetError() == net::ERR_CONNECTION_CLOSED) {
    // The connection was interrupted.
    result = CONNECTION_INTERRUPTED;
  } else if (url_loader->NetError() == net::ERR_FAILED && response_code != 0 &&
             response_code != 200) {
    // net::ERR_FAILED may signal that a non-2xx HTTP response has been
    // received.
    if (response_code >= 500) {
      // Problem at the server.
      result = SERVER_ERROR;
    } else if (response_code >= 400) {
      // Client error.
      result = CLIENT_ERROR;
    } else {
      // Any other type of HTTP failure.
      result = HTTP_ERROR;
    }
  } else if (url_loader->NetError() != net::OK) {
    // Another network error occurred.
    result = NETWORK_ERROR;
  } else {
    result = SUCCESS;
    data = std::make_unique<std::string>(std::move(response_body_));
  }

  ReportFinished(result, std::move(data));
}

void ExternalPolicyDataFetcher::Job::OnRetry(base::OnceClosure start_retry) {
  response_body_.clear();
  std::move(start_retry).Run();
}

void ExternalPolicyDataFetcher::Job::ReportFinished(
    Result result,
    std::unique_ptr<std::string> data) {
  frontend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternalPolicyDataFetcher::OnJobFinished, fetcher_,
                     std::move(callback_), this, result, std::move(data)));
}

ExternalPolicyDataFetcher::ExternalPolicyDataFetcher(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::WeakPtr<ExternalPolicyDataFetcherBackend>& backend)
    : task_runner_(std::move(task_runner)),
      backend_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      backend_(backend),
      weak_factory_(this) {}

ExternalPolicyDataFetcher::~ExternalPolicyDataFetcher() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (auto it = jobs_.begin(); it != jobs_.end(); ++it)
    CancelJob(*it);
}

ExternalPolicyDataFetcher::Job* ExternalPolicyDataFetcher::StartJob(
    const GURL& url,
    int64_t max_size,
    FetchCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Job* job =
      new Job(weak_factory_.GetWeakPtr(), task_runner_, std::move(callback));
  jobs_.insert(job);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ExternalPolicyDataFetcherBackend::StartJob,
                                backend_, url, max_size, job));
  return job;
}

void ExternalPolicyDataFetcher::CancelJob(Job* job) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(jobs_.find(job) != jobs_.end());
  jobs_.erase(job);
  // Post a task that will cancel the |job| in the |backend_|. The |job| is
  // removed from |jobs_| immediately to indicate that it has been canceled but
  // is not actually deleted until the cancelation has reached the |backend_|
  // and a confirmation has been posted back. This ensures that no new job can
  // be allocated at the same address while an OnJobFinished() callback may
  // still be pending for the canceled |job|.
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExternalPolicyDataFetcherBackend::CancelJob, backend_, job,
          base::BindOnce(&ForwardJobCanceled, task_runner_,
                         base::BindOnce(base::DoNothing::Once<Job*>(),
                                        base::Owned(job)))));
}

void ExternalPolicyDataFetcher::OnJobFinished(
    FetchCallback callback,
    Job* job,
    Result result,
    std::unique_ptr<std::string> data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = jobs_.find(job);
  if (it == jobs_.end()) {
    // The |job| has been canceled and removed from |jobs_| already. This can
    // happen because the |backend_| runs on a different thread and a |job| may
    // finish before the cancellation has reached that thread.
    return;
  }
  std::move(callback).Run(result, std::move(data));
  jobs_.erase(it);
  delete job;
}

ExternalPolicyDataFetcherBackend::ExternalPolicyDataFetcherBackend(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)), weak_factory_(this) {}

ExternalPolicyDataFetcherBackend::~ExternalPolicyDataFetcherBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<ExternalPolicyDataFetcher>
ExternalPolicyDataFetcherBackend::CreateFrontend(
    scoped_refptr<base::SequencedTaskRunner> frontend_task_runner) {
  return std::make_unique<ExternalPolicyDataFetcher>(
      std::move(frontend_task_runner), weak_factory_.GetWeakPtr());
}

void ExternalPolicyDataFetcherBackend::StartJob(
    const GURL& url,
    int64_t max_size,
    ExternalPolicyDataFetcher::Job* job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  job->Start(url_loader_factory_.get(), url, max_size);
}

void ExternalPolicyDataFetcherBackend::CancelJob(
    ExternalPolicyDataFetcher::Job* job,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  job->Cancel();
  std::move(callback).Run();
}

}  // namespace policy
