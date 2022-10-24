// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_H_

#include "base/callback.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/security_state/core/security_state.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {
class PasswordChangeSuccessTracker;
}

namespace autofill_assistant {

class MockClient : public Client {
 public:
  MockClient();
  ~MockClient() override;

  MOCK_CONST_METHOD0(GetChannel, version_info::Channel());
  MOCK_CONST_METHOD0(GetLocale, std::string());
  MOCK_CONST_METHOD1(IsXmlSigned, bool(const std::string& xml_string));
  MOCK_CONST_METHOD2(
      ExtractValuesFromSingleTagXml,
      const std::vector<std::string>(const std::string& xml_string,
                                     const std::vector<std::string>& keys));
  MOCK_CONST_METHOD0(GetLatestCountryCode, std::string());
  MOCK_CONST_METHOD0(GetStoredPermanentCountryCode, std::string());
  MOCK_CONST_METHOD0(GetDeviceContext, DeviceContext());
  MOCK_CONST_METHOD0(GetWindowSize, absl::optional<std::pair<int, int>>());
  MOCK_CONST_METHOD0(GetScreenOrientation,
                     ClientContextProto::ScreenOrientation());
  MOCK_CONST_METHOD0(IsAccessibilityEnabled, bool());
  MOCK_CONST_METHOD0(IsSpokenFeedbackAccessibilityServiceEnabled, bool());
  MOCK_CONST_METHOD0(GetEmailAddressForAccessTokenAccount, std::string());
  MOCK_CONST_METHOD0(GetSignedInEmail, std::string());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_CONST_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_CONST_METHOD0(GetWebsiteLoginManager, WebsiteLoginManager*());
  MOCK_CONST_METHOD0(GetPasswordChangeSuccessTracker,
                     password_manager::PasswordChangeSuccessTracker*());
  MOCK_METHOD0(GetAccessTokenFetcher, AccessTokenFetcher*());
  MOCK_METHOD1(Shutdown, void(Metrics::DropOutReason reason));
  MOCK_METHOD1(RecordDropOut, void(Metrics::DropOutReason reason));
  MOCK_METHOD0(AttachUI, void());
  MOCK_METHOD0(DestroyUI, void());
  MOCK_METHOD0(DestroyUISoon, void());
  MOCK_CONST_METHOD0(HasHadUI, bool());
  MOCK_CONST_METHOD0(IsFirstTimeTriggerScriptUser, bool());
  MOCK_METHOD1(FetchPaymentsClientToken,
               void(base::OnceCallback<void(const std::string&)>));
  MOCK_METHOD0(GetScriptExecutorUiDelegate, ScriptExecutorUiDelegate*());
  MOCK_CONST_METHOD0(MustUseBackendData, bool());
  MOCK_CONST_METHOD1(GetAnnotateDomModelVersion,
                     void(base::OnceCallback<void(absl::optional<int64_t>)>));
  MOCK_CONST_METHOD0(GetMakeSearchesAndBrowsingBetterEnabled, bool());
  MOCK_CONST_METHOD0(GetMetricsReportingEnabled, bool());
  MOCK_METHOD(security_state::SecurityLevel,
              GetSecurityLevel,
              (),
              (const override));

 private:
  std::unique_ptr<MockPersonalDataManager> mock_personal_data_manager_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_H_
