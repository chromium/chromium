// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"

#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace policy {

class ExternalPolicyDataFetcher::Job
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  Job(std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      base::WeakPtr<ExternalPolicyDataFetcher> fetcher,
      scoped_refptr<base::SequencedTaskRunner> fetcher_task_runner,
      ExternalPolicyDataFetcher::FetchCallback callback);
  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  void Start(const GURL& url, int64_t max_size);
  void Cancel();
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  // network::SimpleURLLoaderStreamConsumer implementation
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

 private:
  void ReportFinished(Result result, std::unique_ptr<std::string> data);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  base::WeakPtr<ExternalPolicyDataFetcher> fetcher_;
  scoped_refptr<base::SequencedTaskRunner> fetcher_task_runner_;
  ExternalPolicyDataFetcher::FetchCallback callback_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::string response_body_;
  int64_t max_size_ = 0;
};

ExternalPolicyDataFetcher::Job::Job(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    base::WeakPtr<ExternalPolicyDataFetcher> fetcher,
    scoped_refptr<base::SequencedTaskRunner> fetcher_task_runner,
    ExternalPolicyDataFetcher::FetchCallback callback)
    : pending_url_loader_factory_(std::move(pending_url_loader_factory)),
      fetcher_(std::move(fetcher)),
      fetcher_task_runner_(std::move(fetcher_task_runner)),
      callback_(std::move(callback)) {
  // A job is created on the fetcher sequence but it then lives on the separate
  // job sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ExternalPolicyDataFetcher::Job::Start(
    const GURL& url,
    int64_t max_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_GE(max_size, 0);
  max_size_ = max_size;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

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
  url_loader_->DownloadAsStream(network::SharedURLLoaderFactory::Create(
                                    std::move(pending_url_loader_factory_))
                                    .get(),
                                this);
}

void ExternalPolicyDataFetcher::Job::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  url_loader_.reset();
}

void ExternalPolicyDataFetcher::Job::OnResponseStarted(
    const GURL& /* final_url */,
    const network::mojom::URLResponseHead& response_head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (response_head.content_length != -1 &&
      response_head.content_length > max_size_) {
    url_loader_.reset();
    ReportFinished(MAX_SIZE_EXCEEDED, nullptr);
    return;
  }
}

void ExternalPolicyDataFetcher::Job::OnDataReceived(
    std::string_view string_piece,
    base::OnceClosure resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  } else if (url_loader->NetError() == net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    // net::ERR_HTTP_RESPONSE_CODE_FAILURE signals that a non-2xx HTTP response
    // has been received.
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  response_body_.clear();
  std::move(start_retry).Run();
}

void ExternalPolicyDataFetcher::Job::ReportFinished(
    Result result,
    std::unique_ptr<std::string> data) {
  fetcher_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternalPolicyDataFetcher::OnJobFinished, fetcher_,
                     std::move(callback_), this, result, std::move(data)));
}

ExternalPolicyDataFetcher::ExternalPolicyDataFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      job_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  // |url_loader_factory| is null in some tests.
  if (url_loader_factory)
    pending_url_loader_factory_ = url_loader_factory->Clone();
}

ExternalPolicyDataFetcher::~ExternalPolicyDataFetcher() {
  // No RunsTasksInCurrentSequence() check to avoid unit tests failures.
  // In unit tests the browser process instance is deleted only after test ends
  // and test task scheduler is shutted down. Therefore we need to delete some
  // components of BrowserPolicyConnector (ResourceCache and
  // CloudExternalDataManagerBase::Backend) manually when task runner doesn't
  // accept new tasks (DeleteSoon in this case). This leads to the situation
  // when this destructor is called not on |task_runner|.

  for (auto it = jobs_.begin(); it != jobs_.end(); ++it)
    CancelJob(*it);
}

ExternalPolicyDataFetcher::Job* ExternalPolicyDataFetcher::StartJob(
    const GURL& url,
    int64_t max_size,
    FetchCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!cloned_url_loader_factory_) {
    cloned_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_url_loader_factory_));
  }
  Job* job =
      new Job(cloned_url_loader_factory_->Clone(), weak_factory_.GetWeakPtr(),
              task_runner_, std::move(callback));
  jobs_.insert(job);
  job_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Job::Start, base::Unretained(job), url, max_size));
  return job;
}

void ExternalPolicyDataFetcher::CancelJob(Job* job) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(jobs_.find(job) != jobs_.end());
  jobs_.erase(job);
  // Post a task that will cancel the |job| in the |job_task_runner_|. The |job|
  // is removed from |jobs_| immediately to indicate that it has been canceled
  // but is not actually deleted until the cancellation has reached the
  // |job_task_runner_| and a confirmation has been posted back. This ensures
  // that no new job can be allocated at the same address while an
  // OnJobFinished() callback may still be pending for the canceled |job|.
  job_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&Job::Cancel, base::Unretained(job)),
      base::DoNothingWithBoundArgs(base::Owned(job)));
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
    // happen because the jobs run on a different sequence and a |job| may
    // finish before the cancellation has reached that sequence.
    return;
  }
  std::move(callback).Run(result, std::move(data));
  jobs_.erase(it);
  delete job;
}

}  // namespace policy
