// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_download_service.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
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

TestDownloadService::TestDownloadService() = default;
TestDownloadService::~TestDownloadService() = default;

const download::ServiceConfig& TestDownloadService::GetConfig() {
  NOTIMPLEMENTED();
  static TestServiceConfig config;
  return config;
}

void TestDownloadService::StartDownload(
    const download::DownloadParams& download_params) {
  if (!download_dir_.IsValid())
    CHECK(download_dir_.CreateUniqueTempDir());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(download_params.callback, download_params.guid,
                          download::DownloadParams::ACCEPTED));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TestDownloadService::FinishDownload,
                                base::Unretained(this), download_params.guid));
}

void TestDownloadService::FinishDownload(const std::string& guid) {
  base::FilePath path = download_dir_.GetPath().AppendASCII(
      base::StrCat({"dl_", base::NumberToString(next_file_id_++)}));
  const int file_size = static_cast<int>(test_file_data_.size());
  CHECK_EQ(file_size, base::WriteFile(path, test_file_data_.data(), file_size));
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
download::DownloadService::ServiceStatus TestDownloadService::GetStatus() {
  NOTIMPLEMENTED();
  return DownloadService::ServiceStatus();
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
