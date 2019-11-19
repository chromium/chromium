// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace metrics {

class NetMetricsLogUploaderTest : public testing::Test {
 public:
  NetMetricsLogUploaderTest()
      : on_upload_complete_count_(0),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          upload_data_ = network::GetUploadData(request);
          headers_ = request.headers;
          loop_.Quit();
        }));
  }

  void CreateAndOnUploadCompleteReuseUploader() {
    ReportingInfo reporting_info;
    reporting_info.set_attempt_count(10);
    uploader_.reset(new NetMetricsLogUploader(
        test_shared_url_loader_factory_, GURL("https://dummy_server"),
        "dummy_mime", MetricsLogUploader::UMA,
        base::Bind(&NetMetricsLogUploaderTest::OnUploadCompleteReuseUploader,
                   base::Unretained(this))));
    uploader_->UploadLog("initial_dummy_data", "initial_dummy_hash",
                         "initial_dummy_signature", reporting_info);
  }

  void CreateUploaderAndUploadToSecureURL(const std::string& url) {
    ReportingInfo dummy_reporting_info;
    uploader_.reset(new NetMetricsLogUploader(
        test_shared_url_loader_factory_, GURL(url), "dummy_mime",
        MetricsLogUploader::UMA,
        base::Bind(&NetMetricsLogUploaderTest::DummyOnUploadComplete,
                   base::Unretained(this))));
    uploader_->UploadLog("dummy_data", "dummy_hash", "dummy_signature",
                         dummy_reporting_info);
  }

  void CreateUploaderAndUploadToInsecureURL() {
    ReportingInfo dummy_reporting_info;
    uploader_.reset(new NetMetricsLogUploader(
        test_shared_url_loader_factory_, GURL("http://dummy_insecure_server"),
        "dummy_mime", MetricsLogUploader::UMA,
        base::Bind(&NetMetricsLogUploaderTest::DummyOnUploadComplete,
                   base::Unretained(this))));
    std::string compressed_message;
    // Compress the data since the encryption code expects a compressed log,
    // and tries to decompress it before encrypting it.
    compression::GzipCompress("dummy_data", &compressed_message);
    uploader_->UploadLog(compressed_message, "dummy_hash", "dummy_signature",
                         dummy_reporting_info);
  }

  void DummyOnUploadComplete(int response_code,
                             int error_code,
                             bool was_https) {}

  void OnUploadCompleteReuseUploader(int response_code,
                                     int error_code,
                                     bool was_https) {
    ++on_upload_complete_count_;
    if (on_upload_complete_count_ == 1) {
      ReportingInfo reporting_info;
      reporting_info.set_attempt_count(20);
      uploader_->UploadLog("dummy_data", "dummy_hash", "dummy_signature",
                           reporting_info);
    }
  }

  int on_upload_complete_count() const {
    return on_upload_complete_count_;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  const net::HttpRequestHeaders& last_request_headers() { return headers_; }

  const std::string& last_upload_data() { return upload_data_; }

  void WaitForRequest() { loop_.Run(); }

 private:
  std::unique_ptr<NetMetricsLogUploader> uploader_;
  int on_upload_complete_count_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  base::test::TaskEnvironment task_environment_;

  base::RunLoop loop_;
  std::string upload_data_;
  net::HttpRequestHeaders headers_;

  DISALLOW_COPY_AND_ASSIGN(NetMetricsLogUploaderTest);
};

void CheckReportingInfoHeader(net::HttpRequestHeaders headers,
                              int expected_attempt_count) {
  std::string reporting_info_base64;
  EXPECT_TRUE(
      headers.GetHeader("X-Chrome-UMA-ReportingInfo", &reporting_info_base64));
  std::string reporting_info_string;
  EXPECT_TRUE(
      base::Base64Decode(reporting_info_base64, &reporting_info_string));
  ReportingInfo reporting_info;
  EXPECT_TRUE(reporting_info.ParseFromString(reporting_info_string));
  EXPECT_EQ(reporting_info.attempt_count(), expected_attempt_count);
}

TEST_F(NetMetricsLogUploaderTest, OnUploadCompleteReuseUploader) {
  CreateAndOnUploadCompleteReuseUploader();
  WaitForRequest();

  // Mimic the initial fetcher callback.
  CheckReportingInfoHeader(last_request_headers(), 10);
  auto* pending_request_0 = test_url_loader_factory()->GetPendingRequest(0);
  test_url_loader_factory()->SimulateResponseWithoutRemovingFromPendingList(
      pending_request_0, "");

  // Mimic the second fetcher callback.
  CheckReportingInfoHeader(last_request_headers(), 20);
  auto* pending_request_1 = test_url_loader_factory()->GetPendingRequest(1);
  test_url_loader_factory()->SimulateResponseWithoutRemovingFromPendingList(
      pending_request_1, "");

  EXPECT_EQ(on_upload_complete_count(), 2);
}

// Test that attempting to upload to an HTTP URL results in an encrypted
// message.
TEST_F(NetMetricsLogUploaderTest, MessageOverHTTPIsEncrypted) {
  CreateUploaderAndUploadToInsecureURL();
  WaitForRequest();
  encrypted_messages::EncryptedMessage message;
  EXPECT_TRUE(message.ParseFromString(last_upload_data()));
}

// Test that attempting to upload to an HTTPS URL results in an unencrypted
// message.
TEST_F(NetMetricsLogUploaderTest, MessageOverHTTPSIsNotEncrypted) {
  CreateUploaderAndUploadToSecureURL("https://dummy_secure_server");
  WaitForRequest();
  EXPECT_EQ(last_upload_data(), "dummy_data");
}

// Test that attempting to upload to localhost over http results in an
// unencrypted message.
TEST_F(NetMetricsLogUploaderTest, MessageOverHTTPLocalhostIsNotEncrypted) {
  CreateUploaderAndUploadToSecureURL("http://localhost");
  WaitForRequest();
  EXPECT_EQ(last_upload_data(), "dummy_data");
}

}  // namespace metrics
