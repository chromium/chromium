// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/test/test_download_service.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/background_service/service_config.h"
#include "components/download/public/background_service/test/empty_logger.h"

namespace download {
namespace test {

namespace {

// Implementation of ServiceConfig used for testing.
class TestServiceConfig : public ServiceConfig {
 public:
  TestServiceConfig() = default;
  ~TestServiceConfig() override = default;

  // ServiceConfig implementation.
  uint32_t GetMaxScheduledDownloadsPerClient() const override { return 0; }
  uint32_t GetMaxConcurrentDownloads() const override { return 0; }
  const base::TimeDelta& GetFileKeepAliveTime() const override {
    return time_delta_;
  }

 private:
  base::TimeDelta time_delta_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceConfig);
};

}  // namespace

TestDownloadService::TestDownloadService()
    : service_config_(std::make_unique<TestServiceConfig>()),
      logger_(std::make_unique<EmptyLogger>()),
      is_ready_(false),
      fail_at_start_(false),
      file_size_(123456789u),
      client_(nullptr) {}

TestDownloadService::~TestDownloadService() = default;

const ServiceConfig& TestDownloadService::GetConfig() {
  return *service_config_;
}

void TestDownloadService::OnStartScheduledTask(DownloadTaskType task_type,
                                               TaskFinishedCallback callback) {}

bool TestDownloadService::OnStopScheduledTask(DownloadTaskType task_type) {
  return true;
}

DownloadService::ServiceStatus TestDownloadService::GetStatus() {
  return is_ready_ ? DownloadService::ServiceStatus::READY
                   : DownloadService::ServiceStatus::STARTING_UP;
}

void TestDownloadService::StartDownload(const DownloadParams& params) {
  if (!failed_download_id_.empty() && fail_at_start_) {
    params.callback.Run(params.guid,
                        DownloadParams::StartResult::UNEXPECTED_GUID);
    return;
  }

  // The download will be accepted and queued even if the service is not ready.
  params.callback.Run(params.guid, DownloadParams::StartResult::ACCEPTED);
  downloads_.push_back(params);

  if (!is_ready_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TestDownloadService::ProcessDownload,
                                base::Unretained(this)));
}

void TestDownloadService::PauseDownload(const std::string& guid) {}

void TestDownloadService::ResumeDownload(const std::string& guid) {}

void TestDownloadService::CancelDownload(const std::string& guid) {
  for (auto iter = downloads_.begin(); iter != downloads_.end(); ++iter) {
    if (iter->guid == guid) {
      downloads_.erase(iter);
      return;
    }
  }
}

void TestDownloadService::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {}

Logger* TestDownloadService::GetLogger() {
  return logger_.get();
}

base::Optional<DownloadParams> TestDownloadService::GetDownload(
    const std::string& guid) const {
  for (const auto& download : downloads_) {
    if (download.guid == guid)
      return base::Optional<DownloadParams>(download);
  }
  return base::Optional<DownloadParams>();
}

void TestDownloadService::SetFailedDownload(
    const std::string& failed_download_id,
    bool fail_at_start) {
  failed_download_id_ = failed_download_id;
  fail_at_start_ = fail_at_start;
}

void TestDownloadService::SetIsReady(bool is_ready) {
  is_ready_ = is_ready;
  if (is_ready_)
    ProcessDownload();
}

void TestDownloadService::SetHash256(const std::string& hash256) {
  hash256_ = hash256;
}

void TestDownloadService::ProcessDownload() {
  DCHECK(!fail_at_start_);
  if (!is_ready_ || downloads_.empty())
    return;

  DownloadParams params = downloads_.front();
  downloads_.pop_front();

  if (!failed_download_id_.empty() && params.guid == failed_download_id_) {
    CompletionInfo completion_info(base::FilePath(), 0u);
    OnDownloadFailed(params.guid, completion_info,
                     Client::FailureReason::ABORTED);
  } else {
    CompletionInfo completion_info(base::FilePath(), file_size_,
                                   {params.request_params.url}, nullptr);
    completion_info.hash256 = hash256_;
    OnDownloadSucceeded(params.guid, completion_info);
  }
}

void TestDownloadService::OnDownloadSucceeded(
    const std::string& guid,
    const CompletionInfo& completion_info) {
  if (client_)
    client_->OnDownloadSucceeded(guid, completion_info);
}

void TestDownloadService::OnDownloadFailed(
    const std::string& guid,
    const CompletionInfo& completion_info,
    Client::FailureReason failure_reason) {
  if (client_)
    client_->OnDownloadFailed(guid, completion_info, failure_reason);
}

}  // namespace test
}  // namespace download
