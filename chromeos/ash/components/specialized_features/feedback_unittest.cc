// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/specialized_features/feedback.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/proto/extension.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace specialized_features {
namespace {

using ::base::test::InvokeFuture;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::WithArg;
using ::testing::WithoutArgs;

constexpr int kFakeProductId = 1;

class MockFeedbackUploader : public feedback::FeedbackUploader {
 public:
  MockFeedbackUploader(
      const base::FilePath& state_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : FeedbackUploader(/*is_off_the_record=*/false,
                         state_path,
                         std::move(url_loader_factory)) {}

  // feedback::FeedbackUploader:
  MOCK_METHOD(void,
              QueueReport,
              (std::unique_ptr<std::string> data,
               bool has_email,
               int product_id),
              (override));

  base::WeakPtr<FeedbackUploader> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockFeedbackUploader> weak_ptr_factory_{this};
};

// A `base::ScopedTempDir` which automatically creates itself upon construction.
// Required to use `MockFeedbackUploader` in `SpecializedFeaturesFeedbackTest`.
class ScopedAutocreatingTempDir {
 public:
  ScopedAutocreatingTempDir() { CHECK(scoped_temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& GetPath() const LIFETIME_BOUND {
    return scoped_temp_dir_.GetPath();
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
};

class SpecializedFeaturesFeedbackTest : public testing::Test {
 public:
  SpecializedFeaturesFeedbackTest()
      : uploader_(
            scoped_autocreating_temp_dir_.GetPath(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)) {}

  MockFeedbackUploader& uploader() LIFETIME_BOUND { return uploader_; }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  ScopedAutocreatingTempDir scoped_autocreating_temp_dir_;
  MockFeedbackUploader uploader_;
};

TEST_F(SpecializedFeaturesFeedbackTest, SendFeedbackUsesProductId) {
  base::test::TestFuture<void> report_future;
  EXPECT_CALL(uploader(), QueueReport(_, _, kFakeProductId))
      .WillOnce(WithoutArgs(InvokeFuture(report_future)));

  SendFeedback(uploader(), kFakeProductId, "test description");
  EXPECT_TRUE(report_future.Wait());
}

TEST_F(SpecializedFeaturesFeedbackTest, SendFeedbackSendsDescription) {
  base::test::TestFuture<std::unique_ptr<std::string>> feedback_string_future;
  EXPECT_CALL(uploader(), QueueReport)
      .WillOnce(WithArg<0>(InvokeFuture(feedback_string_future)));

  SendFeedback(uploader(), kFakeProductId, "test description");

  std::unique_ptr<std::string> feedback_string = feedback_string_future.Take();
  ASSERT_TRUE(feedback_string);
  userfeedback::ExtensionSubmit feedback_data;
  feedback_data.ParseFromString(*feedback_string);
  EXPECT_EQ(feedback_data.common_data().description(), "test description");
}

TEST_F(SpecializedFeaturesFeedbackTest, SendFeedbackRedactsDescriptionStart) {
  base::test::TestFuture<std::unique_ptr<std::string>> feedback_string_future;
  EXPECT_CALL(uploader(), QueueReport)
      .WillOnce(WithArg<0>(InvokeFuture(feedback_string_future)));

  // This test fails without two leading spaces in the description due to a
  // limitation in `feedback::RedactionTool`.
  // TODO: b/367882164 - Fix this test by either fixing
  // `feedback::RedactionTool`, or working around it.
  SendFeedback(uploader(), kFakeProductId,
               "  4242 4242 4242 4242 is my credit card number");

  std::unique_ptr<std::string> feedback_string = feedback_string_future.Take();
  ASSERT_TRUE(feedback_string);
  userfeedback::ExtensionSubmit feedback_data;
  feedback_data.ParseFromString(*feedback_string);
  EXPECT_THAT(feedback_data.common_data().description(),
              Not(HasSubstr("4242 4242 4242 4242")));
}

TEST_F(SpecializedFeaturesFeedbackTest, SendFeedbackRedactsDescriptionEnd) {
  base::test::TestFuture<std::unique_ptr<std::string>> feedback_string_future;
  EXPECT_CALL(uploader(), QueueReport)
      .WillOnce(WithArg<0>(InvokeFuture(feedback_string_future)));

  // This test fails without a trailing new line in the description due to a
  // limitation in `feedback::RedactionTool`.
  // TODO: b/367882164 - Fix this test by either fixing
  // `feedback::RedactionTool`, or working around it.
  SendFeedback(uploader(), kFakeProductId,
               "My credit card number is 4242 4242 4242 4242\n");

  std::unique_ptr<std::string> feedback_string = feedback_string_future.Take();
  ASSERT_TRUE(feedback_string);
  userfeedback::ExtensionSubmit feedback_data;
  feedback_data.ParseFromString(*feedback_string);
  EXPECT_THAT(feedback_data.common_data().description(),
              Not(HasSubstr("4242 4242 4242 4242")));
}

TEST_F(SpecializedFeaturesFeedbackTest,
       SendFeedbackDoesNotIncludeScreenshotByDefault) {
  base::test::TestFuture<std::unique_ptr<std::string>> feedback_string_future;
  EXPECT_CALL(uploader(), QueueReport)
      .WillOnce(WithArg<0>(InvokeFuture(feedback_string_future)));

  SendFeedback(uploader(), kFakeProductId, "test description");

  std::unique_ptr<std::string> feedback_string = feedback_string_future.Take();
  ASSERT_TRUE(feedback_string);
  userfeedback::ExtensionSubmit feedback_data;
  feedback_data.ParseFromString(*feedback_string);
  EXPECT_FALSE(feedback_data.screenshot().has_binary_content());
}

TEST_F(SpecializedFeaturesFeedbackTest, SendFeedbackSendsScreenshot) {
  base::test::TestFuture<std::unique_ptr<std::string>> feedback_string_future;
  EXPECT_CALL(uploader(), QueueReport)
      .WillOnce(WithArg<0>(InvokeFuture(feedback_string_future)));

  SendFeedback(uploader(), kFakeProductId, "test description",
               "screenshotbytes");

  std::unique_ptr<std::string> feedback_string = feedback_string_future.Take();
  ASSERT_TRUE(feedback_string);
  userfeedback::ExtensionSubmit feedback_data;
  feedback_data.ParseFromString(*feedback_string);
  EXPECT_EQ(feedback_data.screenshot().binary_content(), "screenshotbytes");
}

TEST_F(SpecializedFeaturesFeedbackTest,
       SendFeedbackUsesDefaultScreenshotMimeType) {
  base::test::TestFuture<std::unique_ptr<std::string>> feedback_string_future;
  EXPECT_CALL(uploader(), QueueReport)
      .WillOnce(WithArg<0>(InvokeFuture(feedback_string_future)));

  SendFeedback(uploader(), kFakeProductId, "test description",
               "screenshotbytes");

  std::unique_ptr<std::string> feedback_string = feedback_string_future.Take();
  ASSERT_TRUE(feedback_string);
  userfeedback::ExtensionSubmit feedback_data;
  feedback_data.ParseFromString(*feedback_string);
  EXPECT_EQ(feedback_data.screenshot().mime_type(), "image/png");
}

TEST_F(SpecializedFeaturesFeedbackTest, SendFeedbackSendsScreenshotMimeType) {
  base::test::TestFuture<std::unique_ptr<std::string>> feedback_string_future;
  EXPECT_CALL(uploader(), QueueReport)
      .WillOnce(WithArg<0>(InvokeFuture(feedback_string_future)));

  SendFeedback(uploader(), kFakeProductId, "test description",
               "screenshotbytes", "image/jpeg");

  std::unique_ptr<std::string> feedback_string = feedback_string_future.Take();
  ASSERT_TRUE(feedback_string);
  userfeedback::ExtensionSubmit feedback_data;
  feedback_data.ParseFromString(*feedback_string);
  EXPECT_EQ(feedback_data.screenshot().mime_type(), "image/jpeg");
}

}  // namespace
}  // namespace specialized_features
