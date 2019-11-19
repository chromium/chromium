// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "chromecast/base/scoped_temp_file.h"
#include "chromecast/crash/cast_crashdump_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/breakpad/breakpad/src/common/linux/libcurl_wrapper.h"

namespace chromecast {

class MockLibcurlWrapper : public google_breakpad::LibcurlWrapper {
 public:
  MOCK_METHOD0(Init, bool());
  MOCK_METHOD2(SetProxy,
               bool(const std::string& proxy_host,
                    const std::string& proxy_userpwd));
  MOCK_METHOD2(AddFile,
               bool(const std::string& upload_file_path,
                    const std::string& basename));
  MOCK_METHOD5(SendRequest,
               bool(const std::string& url,
                    const std::map<std::string, std::string>& parameters,
                    int* http_status_code,
                    std::string* http_header_data,
                    std::string* http_response_data));
};

// Declared for the scope of this file to increase readability.
using testing::_;
using testing::Return;

TEST(CastCrashdumpUploaderTest, UploadFailsWhenInitFails) {
  auto m = std::make_unique<testing::StrictMock<MockLibcurlWrapper>>();
  EXPECT_CALL(*m, Init()).Times(1).WillOnce(Return(false));

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = "/tmp/foo.dmp";
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, std::move(m));

  ASSERT_FALSE(uploader.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadSucceedsWithValidParameters) {
  auto m = std::make_unique<testing::StrictMock<MockLibcurlWrapper>>();

  // Create a temporary file.
  ScopedTempFile minidump;

  EXPECT_CALL(*m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*m, AddFile(minidump.path().value(), _)).WillOnce(Return(true));
  EXPECT_CALL(*m, SendRequest("http://foo.com", _, _, _, _)).Times(1).WillOnce(
      Return(true));

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = minidump.path().value();
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, std::move(m));

  ASSERT_TRUE(uploader.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadFailsWithInvalidPathname) {
  auto m = std::make_unique<testing::StrictMock<MockLibcurlWrapper>>();
  EXPECT_CALL(*m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*m, SendRequest(_, _, _, _, _)).Times(0);

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = "/invalid/file/path";
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, std::move(m));

  ASSERT_FALSE(uploader.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadFailsWithoutAllRequiredParameters) {

  // Create a temporary file.
  ScopedTempFile minidump;

  // Has all the require fields for a crashdump.
  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = minidump.path().value();
  data.crash_server = "http://foo.com";

  // Test with empty product name.
  data.product = "";
  auto m = std::make_unique<testing::StrictMock<MockLibcurlWrapper>>();
  EXPECT_CALL(*m, Init()).Times(1).WillOnce(Return(true));
  CastCrashdumpUploader uploader_no_product(data, std::move(m));
  ASSERT_FALSE(uploader_no_product.Upload(nullptr));
  data.product = "foobar";

  // Test with empty product version.
  data.version = "";
  m = std::make_unique<testing::StrictMock<MockLibcurlWrapper>>();
  EXPECT_CALL(*m, Init()).Times(1).WillOnce(Return(true));
  CastCrashdumpUploader uploader_no_version(data, std::move(m));
  ASSERT_FALSE(uploader_no_version.Upload(nullptr));
  data.version = "1.0";

  // Test with empty client GUID.
  data.guid = "";
  m = std::make_unique<testing::StrictMock<MockLibcurlWrapper>>();
  EXPECT_CALL(*m, Init()).Times(1).WillOnce(Return(true));
  CastCrashdumpUploader uploader_no_guid(data, std::move(m));
  ASSERT_FALSE(uploader_no_guid.Upload(nullptr));
}

TEST(CastCrashdumpUploaderTest, UploadFailsWithInvalidAttachment) {
  auto m = std::make_unique<testing::StrictMock<MockLibcurlWrapper>>();

  // Create a temporary file.
  ScopedTempFile minidump;

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = minidump.path().value();
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, std::move(m));

  // Add a file that does not exist as an attachment.
  ASSERT_FALSE(uploader.AddAttachment("label", "/path/does/not/exist"));
}

TEST(CastCrashdumpUploaderTest, UploadSucceedsWithValidAttachment) {
  auto m = std::make_unique<testing::StrictMock<MockLibcurlWrapper>>();

  // Create a temporary file.
  ScopedTempFile minidump;

  // Create a valid attachment.
  ScopedTempFile attachment;

  EXPECT_CALL(*m, Init()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*m, AddFile(minidump.path().value(), _)).WillOnce(Return(true));
  EXPECT_CALL(*m, AddFile(attachment.path().value(), _)).WillOnce(Return(true));
  EXPECT_CALL(*m, SendRequest(_, _, _, _, _)).Times(1).WillOnce(Return(true));

  CastCrashdumpData data;
  data.product = "foobar";
  data.version = "1.0";
  data.guid = "AAA-BBB";
  data.email = "test@test.com";
  data.comments = "none";
  data.minidump_pathname = minidump.path().value();
  data.crash_server = "http://foo.com";
  CastCrashdumpUploader uploader(data, std::move(m));

  // Add a valid file as an attachment.
  uploader.AddAttachment("label", attachment.path().value());
  ASSERT_TRUE(uploader.Upload(nullptr));
}

}  // namespace chromecast
