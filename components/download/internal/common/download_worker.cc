// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/download_worker.h"

#include "base/bind.h"
#include "components/download/internal/common/resource_downloader.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/input_stream.h"
#include "components/download/public/common/url_download_handler_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace download {
namespace {

const int kWorkerVerboseLevel = 1;

bool IsURLSafe(int render_process_id, const GURL& url) {
  // The URL will be checked in the main request, parallel request will ignore
  // the security check here.
  return true;
}

class CompletedInputStream : public InputStream {
 public:
  CompletedInputStream(DownloadInterruptReason status) : status_(status) {}
  ~CompletedInputStream() override = default;

  // InputStream
  bool IsEmpty() override { return false; }
  InputStream::StreamState Read(scoped_refptr<net::IOBuffer>* data,
                                size_t* length) override {
    *length = 0;
    return InputStream::StreamState::COMPLETE;
  }

  DownloadInterruptReason GetCompletionStatus() override { return status_; }

 private:
  DownloadInterruptReason status_;
  DISALLOW_COPY_AND_ASSIGN(CompletedInputStream);
};

void CreateUrlDownloadHandler(
    std::unique_ptr<DownloadUrlParameters> params,
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    URLLoaderFactoryProvider* url_loader_factory_provider,
    const URLSecurityPolicy& url_security_policy,
    std::unique_ptr<service_manager::Connector> connector,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  auto downloader = UrlDownloadHandlerFactory::Create(
      std::move(params), delegate,
      url_loader_factory_provider
          ? url_loader_factory_provider->GetURLLoaderFactory()
          : nullptr,
      url_security_policy, std::move(connector), task_runner);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UrlDownloadHandler::Delegate::OnUrlDownloadHandlerCreated,
                     delegate, std::move(downloader)));
}

}  // namespace

DownloadWorker::DownloadWorker(DownloadWorker::Delegate* delegate,
                               int64_t offset)
    : delegate_(delegate),
      offset_(offset),
      is_paused_(false),
      is_canceled_(false),
      url_download_handler_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  DCHECK(delegate_);
}

DownloadWorker::~DownloadWorker() = default;

void DownloadWorker::SendRequest(
    std::unique_ptr<DownloadUrlParameters> params,
    URLLoaderFactoryProvider* url_loader_factory_provider,
    service_manager::Connector* connector) {
  GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&CreateUrlDownloadHandler, std::move(params),
                                weak_factory_.GetWeakPtr(),
                                // This is safe because URLLoaderFactoryProvider
                                // deleter is called on the same task sequence.
                                base::Unretained(url_loader_factory_provider),
                                base::BindRepeating(&IsURLSafe),
                                connector ? connector->Clone() : nullptr,
                                base::ThreadTaskRunnerHandle::Get()));
}

void DownloadWorker::Pause() {
  is_paused_ = true;
}

void DownloadWorker::Resume() {
  is_paused_ = false;
}

void DownloadWorker::Cancel(bool user_cancel) {
  is_canceled_ = true;
  url_download_handler_.reset();
}

void DownloadWorker::OnUrlDownloadStarted(
    std::unique_ptr<DownloadCreateInfo> create_info,
    std::unique_ptr<InputStream> input_stream,
    URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
        url_loader_factory_provider,
    UrlDownloadHandler* downloader,
    DownloadUrlParameters::OnStartedCallback callback) {
  // |callback| is not used in subsequent requests.
  DCHECK(callback.is_null());

  // Destroy the request if user canceled.
  if (is_canceled_) {
    VLOG(kWorkerVerboseLevel)
        << "Byte stream arrived after user cancel the request.";
    url_download_handler_.reset();
    return;
  }

  if (offset_ != create_info->offset)
    create_info->result = DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;

  if (create_info->result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    RecordParallelRequestCreationFailure(create_info->result);
    VLOG(kWorkerVerboseLevel)
        << "Parallel download sub-request failed. reason = "
        << create_info->result;
    input_stream.reset(new CompletedInputStream(create_info->result));
    url_download_handler_.reset();
  }

  // Pause the stream if user paused, still push the stream reader to the sink.
  if (is_paused_) {
    VLOG(kWorkerVerboseLevel)
        << "Byte stream arrived after user pause the request.";
    Pause();
  }

  delegate_->OnInputStreamReady(this, std::move(input_stream),
                                std::move(create_info));
}

void DownloadWorker::OnUrlDownloadStopped(UrlDownloadHandler* downloader) {
  // Release the |url_download_handler_|, the object will be deleted on IO
  // thread.
  url_download_handler_.reset();
}

void DownloadWorker::OnUrlDownloadHandlerCreated(
    UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) {
  url_download_handler_ = std::move(downloader);
}

}  // namespace download
