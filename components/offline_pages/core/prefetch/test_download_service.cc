// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_download_service.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/service_config.h"
#include "components/offline_pages/core/prefetch/test_download_client.h"

namespace offline_pages {

namespace {

// Implementation of ServiceConfig used for testing. This is never actually
// constructed.
class TestServiceConfig : public download::ServiceConfig {
 public:
  TestServiceConfig() = default;

  TestServiceConfig(const TestServiceConfig&) = delete;
  TestServiceConfig& operator=(const TestServiceConfig&) = delete;

  ~TestServiceConfig() override = default;

  // ServiceConfig implementation.
  uint32_t GetMaxScheduledDownloadsPerClient() const override { return 0; }
  uint32_t GetMaxConcurrentDownloads() const override { return 0; }
  const base::TimeDelta& GetFileKeepAliveTime() const override {
    return time_delta_;
  }

 private:
  base::TimeDelta time_delta_;
};

}  // namespace

TestDownloadService::TestDownloadService() = default;
TestDownloadService::~TestDownloadService() = default;

const download::ServiceConfig& TestDownloadService::GetConfig() {
  NOTIMPLEMENTED();
  static TestServiceConfig config;
  return config;
}

void TestDownloadService::StartDownload(
    download::DownloadParams download_params) {
  if (!download_dir_.IsValid())
    CHECK(download_dir_.CreateUniqueTempDir());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(download_params.callback), download_params.guid,
                     download::DownloadParams::ACCEPTED));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestDownloadService::FinishDownload,
                                base::Unretained(this), download_params.guid));
}

void TestDownloadService::FinishDownload(const std::string& guid) {
  base::FilePath path = download_dir_.GetPath().AppendASCII(
      base::StrCat({"dl_", base::NumberToString(next_file_id_++)}));
  CHECK(base::WriteFile(path, test_file_data_));
  client_->OnDownloadSucceeded(
      guid, download::CompletionInfo(path, test_file_data_.size(),
                                     std::vector<GURL>(), nullptr));
}

void TestDownloadService::SetTestFileData(const std::string& data) {
  test_file_data_ = data;
}

void TestDownloadService::OnStartScheduledTask(
    download::DownloadTaskType task_type,
    download::TaskFinishedCallback callback) {
  NOTIMPLEMENTED();
}
bool TestDownloadService::OnStopScheduledTask(
    download::DownloadTaskType task_type) {
  NOTIMPLEMENTED();
  return false;
}
download::BackgroundDownloadService::ServiceStatus
TestDownloadService::GetStatus() {
  NOTIMPLEMENTED();
  return BackgroundDownloadService::ServiceStatus();
}
void TestDownloadService::PauseDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void TestDownloadService::ResumeDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void TestDownloadService::CancelDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void TestDownloadService::ChangeDownloadCriteria(
    const std::string& guid,
    const download::SchedulingParams& params) {
  NOTIMPLEMENTED();
}
download::Logger* TestDownloadService::GetLogger() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace offline_pages
