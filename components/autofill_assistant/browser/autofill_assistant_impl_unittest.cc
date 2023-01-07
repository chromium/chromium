// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_assistant_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill_assistant/browser/mock_common_dependencies.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using autofill::FormSignature;
using base::test::RunOnceCallback;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAreArray;

const char kScriptServerUrl[] = "https://www.fake.backend.com/script_server";

class AutofillAssistantImplTest : public testing::Test {
 public:
  AutofillAssistantImplTest() {
    auto mock_request_sender =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_request_sender_ = mock_request_sender.get();

    auto mock_common_dependencies = std::make_unique<MockCommonDependencies>();
    mock_dependencies_ = mock_common_dependencies.get();
    ON_CALL(*mock_dependencies_, GetLatestCountryCode)
        .WillByDefault(Return("US"));
    ON_CALL(*mock_dependencies_, GetLocale).WillByDefault(Return("en-US"));
    ON_CALL(*mock_dependencies_, IsSupervisedUser).WillByDefault(Return(false));

    // As long as the `BrowserContext` is only passed as an argument during
    // `CommonDependencies` calls, we do not need to set up a test environment
    // for it.
    service_ = std::make_unique<AutofillAssistantImpl>(
        /* browser_context= */ nullptr, std::move(mock_request_sender),
        std::move(mock_common_dependencies), GURL(kScriptServerUrl));
  }
  ~AutofillAssistantImplTest() override = default;

 protected:
  base::MockCallback<AutofillAssistant::GetCapabilitiesResponseCallback>
      mock_response_callback_;
  raw_ptr<NiceMock<MockServiceRequestSender>> mock_request_sender_;
  raw_ptr<MockCommonDependencies> mock_dependencies_;
  std::unique_ptr<AutofillAssistantImpl> service_;
};

}  // namespace

bool operator==(const AutofillAssistant::BundleCapabilitiesInformation& lhs,
                const AutofillAssistant::BundleCapabilitiesInformation& rhs) {
  return (lhs.trigger_form_signatures == rhs.trigger_form_signatures &&
          lhs.supports_consentless_execution ==
              rhs.supports_consentless_execution);
}

bool operator==(const AutofillAssistant::CapabilitiesInfo& lhs,
                const AutofillAssistant::CapabilitiesInfo& rhs) {
  return std::tie(lhs.url, lhs.script_parameters,
                  lhs.bundle_capabilities_information) ==
         std::tie(rhs.url, rhs.script_parameters,
                  rhs.bundle_capabilities_information);
}

TEST_F(AutofillAssistantImplTest, GetCapabilitiesByHashPrefixEmptyRespose) {
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _,
                            RpcType::GET_CAPABILITIES_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK, std::vector<AutofillAssistant::CapabilitiesInfo>()));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

TEST_F(AutofillAssistantImplTest, BackendRequestFailed) {
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _,
                            RpcType::GET_CAPABILITIES_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_FORBIDDEN, "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_FORBIDDEN,
                  std::vector<AutofillAssistant::CapabilitiesInfo>()));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

TEST_F(AutofillAssistantImplTest, ParsingError) {
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _,
                            RpcType::GET_CAPABILITIES_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "invalid",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK, std::vector<AutofillAssistant::CapabilitiesInfo>()));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

TEST_F(AutofillAssistantImplTest, GetCapabilitiesByHashPrefix) {
  GetCapabilitiesByHashPrefixResponseProto proto;
  GetCapabilitiesByHashPrefixResponseProto::MatchInfoProto* match_info =
      proto.add_match_info();
  match_info->set_url_match("http://exampleA.com");
  ScriptParameterProto* script_parameter =
      match_info->add_script_parameters_override();
  script_parameter->set_name("EXPERIMENT_IDS");
  script_parameter->set_value("3345172");

  GetCapabilitiesByHashPrefixResponseProto::MatchInfoProto* match_info2 =
      proto.add_match_info();
  match_info2->set_url_match("http://exampleB.com");

  BundleCapabilitiesInformationProto* bundle_cap_info_proto =
      match_info2->mutable_bundle_capabilities_information();
  BundleCapabilitiesInformationProto::ChromeFastCheckoutProto*
      fast_checkout_proto =
          bundle_cap_info_proto->mutable_chrome_fast_checkout();

  fast_checkout_proto->add_trigger_form_signatures(123ull);
  fast_checkout_proto->add_trigger_form_signatures(18446744073709551615ull);
  bundle_cap_info_proto->set_supports_consentless_execution(true);

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  AutofillAssistant::BundleCapabilitiesInformation
      bundle_capabilities_information;
  bundle_capabilities_information.trigger_form_signatures =
      std::vector<FormSignature>{FormSignature(123ull),
                                 FormSignature(18446744073709551615ull)};
  bundle_capabilities_information.supports_consentless_execution = true;

  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kScriptServerUrl), _, _,
                            RpcType::GET_CAPABILITIES_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_proto,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK,
          UnorderedElementsAreArray(
              std::vector<AutofillAssistant::CapabilitiesInfo>{
                  {"http://exampleA.com", {{"EXPERIMENT_IDS", "3345172"}}},
                  {"http://exampleB.com",
                   {},
                   bundle_capabilities_information}})));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

TEST_F(AutofillAssistantImplTest,
       GetCapabilitiesByHashPrefixDoesNotExecuteForSupervisedUsers) {
  EXPECT_CALL(*mock_dependencies_, IsSupervisedUser).WillOnce(Return(true));

  EXPECT_CALL(*mock_request_sender_, OnSendRequest).Times(0);

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK, std::vector<AutofillAssistant::CapabilitiesInfo>()));

  service_->GetCapabilitiesByHashPrefix(16, {1339}, "DUMMY_INTENT",
                                        mock_response_callback_.Get());
}

}  // namespace autofill_assistant
