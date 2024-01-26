// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/in_memory_download_driver.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/download/internal/background_service/test/mock_download_driver_client.h"
#include "components/download/public/background_service/blob_context_getter_factory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace download {
namespace {

MATCHER_P(DriverEntryEqual, entry, "") {
  return entry.guid == arg.guid && entry.state == arg.state &&
         entry.bytes_downloaded == arg.bytes_downloaded;
}

// BlobContextGetterFactory implementation that does nothing.
class NoopBlobContextGetterFactory : public BlobContextGetterFactory {
 public:
  NoopBlobContextGetterFactory() = default;

  NoopBlobContextGetterFactory(const NoopBlobContextGetterFactory&) = delete;
  NoopBlobContextGetterFactory& operator=(const NoopBlobContextGetterFactory&) =
      delete;

  ~NoopBlobContextGetterFactory() override = default;

 private:
  void RetrieveBlobContextGetter(BlobContextGetterCallback callback) override {}
};

// Test in memory download that doesn't do complex IO.
class TestInMemoryDownload : public InMemoryDownload {
 public:
  TestInMemoryDownload(const std::string& guid,
                       InMemoryDownload::Delegate* delegate)
      : InMemoryDownload(guid), delegate_(delegate) {
    DCHECK(delegate_) << "Delegate can't be nullptr.";
  }

  TestInMemoryDownload(const TestInMemoryDownload&) = delete;
  TestInMemoryDownload& operator=(const TestInMemoryDownload&) = delete;

  void SimulateDownloadStarted() {
    state_ = InMemoryDownload::State::IN_PROGRESS;
    delegate_->OnDownloadStarted(this);
  }

  void SimulateDownloadProgress() {
    state_ = InMemoryDownload::State::IN_PROGRESS;
    delegate_->OnDownloadProgress(this);
  }

  void SimulateDownloadComplete(bool success) {
    state_ = success ? InMemoryDownload::State::COMPLETE
                     : InMemoryDownload::State::FAILED;
    delegate_->OnDownloadComplete(this);
  }

  // InMemoryDownload implementation.
  void Start() override {}
  void Pause() override {}
  void Resume() override {}
  std::unique_ptr<storage::BlobDataHandle> ResultAsBlob() const override {
    return nullptr;
  }
  size_t EstimateMemoryUsage() const override { return 0u; }

 private:
  raw_ptr<InMemoryDownload::Delegate> delegate_;
};

// Factory that injects to InMemoryDownloadDriver and only creates fake objects.
class TestInMemoryDownloadFactory : public InMemoryDownload::Factory {
 public:
  TestInMemoryDownloadFactory() = default;

  TestInMemoryDownloadFactory(const TestInMemoryDownloadFactory&) = delete;
  TestInMemoryDownloadFactory& operator=(const TestInMemoryDownloadFactory&) =
      delete;

  ~TestInMemoryDownloadFactory() override = default;

  // InMemoryDownload::Factory implementation.
  std::unique_ptr<InMemoryDownload> Create(
      const std::string& guid,
      const RequestParams& request_params,
      scoped_refptr<network::ResourceRequestBody> request_body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      InMemoryDownload::Delegate* delegate) override {
    auto download = std::make_unique<TestInMemoryDownload>(guid, delegate);
    download_ = download.get();
    return download;
  }

  // Returns the last created download, if the driver remove the object, this
  // can be invalid memory, so need to use with caution.
  TestInMemoryDownload* last_created_download() { return download_; }

 private:
  raw_ptr<TestInMemoryDownload, DanglingUntriaged> download_ = nullptr;
};

class InMemoryDownloadDriverTest : public testing::Test {
 public:
  InMemoryDownloadDriverTest() = default;

  InMemoryDownloadDriverTest(const InMemoryDownloadDriverTest&) = delete;
  InMemoryDownloadDriverTest& operator=(const InMemoryDownloadDriverTest&) =
      delete;

  ~InMemoryDownloadDriverTest() override = default;

  // Helper method to call public method on |driver_|.
  DownloadDriver* driver() {
    return static_cast<DownloadDriver*>(driver_.get());
  }

  MockDriverClient* driver_client() { return &driver_client_; }

  TestInMemoryDownloadFactory* factory() { return factory_; }

  void SetUp() override {
    auto factory = std::make_unique<TestInMemoryDownloadFactory>();
    factory_ = factory.get();
    auto blob_context_getter_factory =
        std::make_unique<NoopBlobContextGetterFactory>();
    driver_ = std::make_unique<InMemoryDownloadDriver>(
        std::move(factory), std::move(blob_context_getter_factory));
    driver()->Initialize(&driver_client_);
  }

  void TearDown() override {
    // Driver should be teared down before its owner.
    driver_.reset();
  }

  void Start(const std::string& guid) {
    RequestParams params;
    base::FilePath path;
    driver()->Start(params, guid, path, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

 private:
  testing::NiceMock<MockDriverClient> driver_client_;
  std::unique_ptr<InMemoryDownloadDriver> driver_;
  raw_ptr<TestInMemoryDownloadFactory, DanglingUntriaged> factory_;
};

// Verifies in memory download success and remove API.
TEST_F(InMemoryDownloadDriverTest, DownloadSuccessAndRemove) {
  // General states check.
  EXPECT_TRUE(driver()->IsReady());

  const std::string guid = "1234";
  EXPECT_CALL(*driver_client(), OnDownloadCreated(_)).Times(1);
  Start(guid);
  factory()->last_created_download()->SimulateDownloadStarted();

  // After starting a download, we should be able to find a record in the
  // driver.
  std::optional<DriverEntry> entry = driver()->Find(guid);
  EXPECT_TRUE(entry.has_value());
  EXPECT_EQ(guid, entry->guid);
  EXPECT_EQ(DriverEntry::State::IN_PROGRESS, entry->state);

  EXPECT_CALL(*driver_client(), OnDownloadUpdated(_));
  factory()->last_created_download()->SimulateDownloadProgress();

  DriverEntry match_entry;
  match_entry.state = DriverEntry::State::COMPLETE;
  match_entry.guid = guid;
  EXPECT_CALL(*driver_client(),
              OnDownloadSucceeded(DriverEntryEqual(match_entry)))
      .Times(1);
  EXPECT_CALL(*driver_client(), OnDownloadFailed(_, _)).Times(0);

  // Trigger download complete.
  factory()->last_created_download()->SimulateDownloadComplete(
      true /* success*/);
  EXPECT_TRUE(entry.has_value());
  entry = driver()->Find(guid);
  EXPECT_EQ(guid, entry->guid);
  EXPECT_EQ(DriverEntry::State::COMPLETE, entry->state);

  driver()->Remove(guid, false);
  entry = driver()->Find(guid);
  EXPECT_FALSE(entry.has_value());
}

// Verifies in memory download failure.
TEST_F(InMemoryDownloadDriverTest, DownloadFailure) {
  // General states check.
  EXPECT_TRUE(driver()->IsReady());

  EXPECT_CALL(*driver_client(), OnDownloadCreated(_)).Times(1);
  EXPECT_CALL(*driver_client(), OnDownloadUpdated(_)).Times(0);

  const std::string guid = "1234";
  Start(guid);
  factory()->last_created_download()->SimulateDownloadStarted();

  DriverEntry match_entry;
  match_entry.state = DriverEntry::State::INTERRUPTED;
  match_entry.guid = guid;
  EXPECT_CALL(*driver_client(),
              OnDownloadFailed(DriverEntryEqual(match_entry), _))
      .Times(1);
  EXPECT_CALL(*driver_client(), OnDownloadSucceeded(_)).Times(0);

  // Trigger download complete.
  factory()->last_created_download()->SimulateDownloadComplete(
      false /* success*/);
  std::optional<DriverEntry> entry = driver()->Find(guid);
  EXPECT_TRUE(entry.has_value());
  EXPECT_EQ(guid, entry->guid);
  EXPECT_EQ(DriverEntry::State::INTERRUPTED, entry->state);
}

}  // namespace
}  // namespace download
