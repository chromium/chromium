// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui_handler_impl.h"

#include <map>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui_handler_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace ash {

class FakeParentAccessUIHandlerDelegate : public ParentAccessUIHandlerDelegate {
 public:
  FakeParentAccessUIHandlerDelegate() = default;
  ~FakeParentAccessUIHandlerDelegate() = default;

  parent_access_ui::mojom::ParentAccessParamsPtr CloneParentAccessParams()
      override {
    return parent_access_ui::mojom::ParentAccessParams::New(
        parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
        parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
            parent_access_ui::mojom::WebApprovalsParams::New()));
  }

  MOCK_METHOD2(SetApproved, void(const std::string&, const base::Time&));
  MOCK_METHOD0(SetDeclined, void());
  MOCK_METHOD0(SetCanceled, void());
  MOCK_METHOD0(SetError, void());
};

class ParentAccessUIHandlerImplTest : public testing::Test {
 public:
  ParentAccessUIHandlerImplTest() = default;
  ~ParentAccessUIHandlerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakePrimaryAccountAvailable(
        "testuser@gmail.com", signin::ConsentLevel::kSync);

    parent_access_ui_handler_ = std::make_unique<ParentAccessUIHandlerImpl>(
        parent_access_ui_handler_remote_.BindNewPipeAndPassReceiver(),
        identity_test_env_->identity_manager(), &delegate_);
  }

  void TearDown() override { parent_access_ui_handler_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  mojo::Remote<parent_access_ui::mojom::ParentAccessUIHandler>
      parent_access_ui_handler_remote_;
  std::unique_ptr<ParentAccessUIHandlerImpl> parent_access_ui_handler_;
  FakeParentAccessUIHandlerDelegate delegate_;
};

// Verifies that the webview URL is properly constructed
TEST_F(ParentAccessUIHandlerImplTest, GetParentAccessURL) {
  base::RunLoop run_loop;
  parent_access_ui_handler_->GetParentAccessURL(
      base::BindLambdaForTesting([&](const std::string& url) -> void {
        GURL webview_url(url);
        ASSERT_TRUE(webview_url.has_query());

        // Split the query string into a map of keys to values.
        std::string query_str = webview_url.query();
        url::Component query(0, query_str.length());
        url::Component key;
        url::Component value;
        std::map<std::string, std::string> query_parts;
        while (url::ExtractQueryKeyValue(query_str.c_str(), &query, &key,
                                         &value)) {
          query_parts[query_str.substr(key.begin, key.len)] =
              query_str.substr(value.begin, value.len);
        }

        // Validate the query parameters.
        EXPECT_EQ(query_parts.at("callerid"), "39454505");
        EXPECT_EQ(query_parts.at("cros-origin"), "chrome://parent-access");
        EXPECT_EQ(query_parts.at("platform_version"),
                  base::SysInfo::OperatingSystemVersion());
        EXPECT_EQ(query_parts.at("hl"), "en");
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verify that the access token is successfully fetched.
TEST_F(ParentAccessUIHandlerImplTest, GetOAuthTokenSuccess) {
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  base::RunLoop run_loop;
  parent_access_ui_handler_->GetOAuthToken(base::BindLambdaForTesting(
      [&](parent_access_ui::mojom::GetOAuthTokenStatus status,
          const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kSuccess,
                  status);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verifies that access token fetch errors are recorded.
TEST_F(ParentAccessUIHandlerImplTest, GetOAuthTokenError) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  parent_access_ui_handler_->GetOAuthToken(base::BindLambdaForTesting(
      [&](parent_access_ui::mojom::GetOAuthTokenStatus status,
          const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kError, status);
        run_loop.Quit();
      }));

  // Trigger failure to issue access token.
  identity_test_env_->SetAutomaticIssueOfAccessTokens(false);
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceError("FAKE SERVICE ERROR"));

  run_loop.Run();

  // Expect metric to be recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          absl::nullopt),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kOAuthError, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kOAuthError, 1);
}

// Verifies that only one access token fetch is possible at a time.
TEST_F(ParentAccessUIHandlerImplTest, GetOAuthTokenOnlyOneFetchAtATimeError) {
  identity_test_env_->SetAutomaticIssueOfAccessTokens(false);
  parent_access_ui_handler_->GetOAuthToken(base::DoNothing());

  base::RunLoop one_fetch_run_loop;
  parent_access_ui_handler_->GetOAuthToken(base::BindLambdaForTesting(
      [&](parent_access_ui::mojom::GetOAuthTokenStatus status,
          const std::string& token) -> void {
        EXPECT_EQ(
            parent_access_ui::mojom::GetOAuthTokenStatus::kOnlyOneFetchAtATime,
            status);
        one_fetch_run_loop.Quit();
      }));
  one_fetch_run_loop.Run();
}

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized = message.SerializeAsString();
  std::string actual_serialized = arg.SerializeAsString();
  return expected_serialized == actual_serialized;
}

// Verifies that the parent approvals sequence is handled correctly.
TEST_F(ParentAccessUIHandlerImplTest, OnParentVerifiedAndApproved) {
  base::HistogramTester histogram_tester;
  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  kids::platform::parentaccess::client::proto::OnParentVerified*
      on_parent_verified = parent_access_callback.mutable_on_parent_verified();
  kids::platform::parentaccess::client::proto::ParentAccessToken* pat =
      on_parent_verified->mutable_parent_access_token();
  pat->set_token("TEST_TOKEN");
  kids::platform::parentaccess::client::proto::Timestamp* expire_time =
      pat->mutable_expire_time();
  expire_time->set_seconds(123456);
  // Nanoseconds will be ignored.
  expire_time->set_nanos(567890);

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  EXPECT_CALL(delegate_, SetApproved(pat->token(), base::Time::FromDoubleT(
                                                       expire_time->seconds())))
      .Times(1);

  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindLambdaForTesting(
          [&](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify the Parent Verified callback is parsed.
            EXPECT_EQ(parent_access_ui::mojom::ParentAccessServerMessageType::
                          kParentVerified,
                      message->type);
            run_loop.Quit();
          }));

  run_loop.Run();

  // Verify the Parent Access Token was stored.
  EXPECT_THAT(
      *pat,
      EqualsProto(*(parent_access_ui_handler_->GetParentAccessTokenForTest())));

  // Send the approved result status.
  base::RunLoop parent_approved_run_loop;
  parent_access_ui_handler_->OnParentAccessDone(
      parent_access_ui::mojom::ParentAccessResult::kApproved,
      base::BindLambdaForTesting(
          [&]() -> void { parent_approved_run_loop.Quit(); }));

  parent_approved_run_loop.Run();

  // Reset handler to simulate dialog closing.
  parent_access_ui_handler_.reset();
  histogram_tester.ExpectUniqueSample(
      ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
          absl::nullopt),
      ParentAccessStateTracker::FlowResult::kAccessApproved, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessStateTracker::FlowResult::kAccessApproved, 1);
}

// Verifies that an unparsable parent access callback proto is handled
// properly.
TEST_F(ParentAccessUIHandlerImplTest, OnInvalidParentAccessCallback) {
  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode("INVALID_SERIALIZED_CALLBACK",
                     &encoded_parent_access_callback);

  EXPECT_CALL(delegate_, SetApproved(_, _)).Times(0);

  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindLambdaForTesting(
          [&](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify the Parent Verified callback is parsed.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kError,
                message->type);
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Verifies that non-base64 encoded data passed as a parent access callback is
// handled properly.
TEST_F(ParentAccessUIHandlerImplTest, OnNonBase64ParentAccessCallback) {
  EXPECT_CALL(delegate_, SetApproved(_, _)).Times(0);

  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessCallbackReceived(
      "**THIS_STRING_HAS_NON_BASE64_CHARACTERS**",
      base::BindLambdaForTesting(
          [&](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify the Parent Verified callback is parsed.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kError,
                message->type);
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Verifies that parent declining is handled correctly.
TEST_F(ParentAccessUIHandlerImplTest, OnParentDeclined) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(delegate_, SetDeclined()).Times(1);

  // Send the declined result status.
  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessDone(
      parent_access_ui::mojom::ParentAccessResult::kDeclined,
      base::BindLambdaForTesting([&]() -> void { run_loop.Quit(); }));

  run_loop.Run();

  // Reset handler to simulate dialog closing.
  parent_access_ui_handler_.reset();
  histogram_tester.ExpectUniqueSample(
      ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
          absl::nullopt),
      ParentAccessStateTracker::FlowResult::kAccessDeclined, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessStateTracker::FlowResult::kAccessDeclined, 1);
}

// Verifies canceling the UI is handled correctly.
TEST_F(ParentAccessUIHandlerImplTest, OnCanceled) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(delegate_, SetCanceled()).Times(1);

  // Send the declined result status.
  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessDone(
      parent_access_ui::mojom::ParentAccessResult::kCanceled,
      base::BindLambdaForTesting([&]() -> void { run_loop.Quit(); }));

  run_loop.Run();

  // Reset handler to simulate dialog closing.
  parent_access_ui_handler_.reset();
  histogram_tester.ExpectUniqueSample(
      ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
          absl::nullopt),
      ParentAccessStateTracker::FlowResult::kParentAuthentication, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessStateTracker::FlowResult::kParentAuthentication, 1);
}

// Verifies errors are handled correctly.
TEST_F(ParentAccessUIHandlerImplTest, OnError) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(delegate_, SetError()).Times(1);

  // Send the declined result status.
  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessDone(
      parent_access_ui::mojom::ParentAccessResult::kError,
      base::BindLambdaForTesting([&]() -> void { run_loop.Quit(); }));

  run_loop.Run();

  // Reset handler to simulate dialog closing.
  parent_access_ui_handler_.reset();
  histogram_tester.ExpectUniqueSample(
      ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
          absl::nullopt),
      ParentAccessStateTracker::FlowResult::kError, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessStateTracker::FlowResult::kError, 1);
}

// Verifies that the ConsentDeclined status is ignored.
TEST_F(ParentAccessUIHandlerImplTest, ConsentDeclinedParsed) {
  base::HistogramTester histogram_tester;
  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_consent_declined();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindLambdaForTesting(
          [&](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Expect metric to be recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          absl::nullopt),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kUnknownCallback, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kUnknownCallback, 1);
}

// Verifies that the OnPageSizeChanged status is ignored.
TEST_F(ParentAccessUIHandlerImplTest, OnPageSizeChangedIgnored) {
  base::HistogramTester histogram_tester;
  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_page_size_changed();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindLambdaForTesting(
          [&](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Expect metric to be recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          absl::nullopt),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kUnknownCallback, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kUnknownCallback, 1);
}

// Verifies that the OnCommunicationEstablished status is ignored.
TEST_F(ParentAccessUIHandlerImplTest, OnCommunicationEstablishedIgnored) {
  base::HistogramTester histogram_tester;

  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_communication_established();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  base::RunLoop run_loop;
  parent_access_ui_handler_->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindLambdaForTesting(
          [&](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Expect metric to be recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          absl::nullopt),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kUnknownCallback, 1);
}

// Verifies metric is recorded for no delegate error.
TEST_F(ParentAccessUIHandlerImplTest, NoDelegateErrorMetricRecorded) {
  base::HistogramTester histogram_tester;

  // Construct a ParentAccessUIHandler without a delegate.
  mojo::Remote<parent_access_ui::mojom::ParentAccessUIHandler> remote;
  auto parent_access_ui_handler_no_delegate =
      std::make_unique<ParentAccessUIHandlerImpl>(
          remote.BindNewPipeAndPassReceiver(),
          identity_test_env_->identity_manager(), nullptr);

  // Send a result status.
  base::RunLoop run_loop;
  parent_access_ui_handler_no_delegate->OnParentAccessDone(
      parent_access_ui::mojom::ParentAccessResult::kApproved,
      base::BindLambdaForTesting([&]() -> void { run_loop.Quit(); }));

  run_loop.Run();

  // Expect metric to be recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          absl::nullopt),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kDelegateNotAvailable,
      1);
}

// Verifies metric is recorded when received callback cannot be decoded.
TEST_F(ParentAccessUIHandlerImplTest, DecodingErrorMetricRecorded) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Receive non-decodable callback.
  parent_access_ui_handler_->OnParentAccessCallbackReceived(
      "not_a_callback",
      base::BindLambdaForTesting(
          [&](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void { run_loop.Quit(); }));
  run_loop.Run();

  // Expect metric to be recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          absl::nullopt),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kDecodingError, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kDecodingError, 1);
}

// Verifies metric is recorded when received callback cannot be parsed to proto.
TEST_F(ParentAccessUIHandlerImplTest, ParsingErrorMetricRecorded) {
  base::HistogramTester histogram_tester;

  // Receive non-parseable callback.
  base::RunLoop run_loop;
  std::string encoded_not_a_callback;
  base::Base64Encode("not_a_callback", &encoded_not_a_callback);
  parent_access_ui_handler_->OnParentAccessCallbackReceived(
      encoded_not_a_callback,
      base::BindLambdaForTesting(
          [&](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void { run_loop.Quit(); }));
  run_loop.Run();

  // Expect metric to be recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          absl::nullopt),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kParsingError, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessUIHandlerImpl::GetParentAccessWidgetErrorHistogramForFlowType(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kWebsiteAccess),
      ParentAccessUIHandlerImpl::ParentAccessWidgetError::kParsingError, 1);
}
}  // namespace ash
