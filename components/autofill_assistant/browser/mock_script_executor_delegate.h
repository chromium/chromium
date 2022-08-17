// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_DELEGATE_H_

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/viewport_mode.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordChangeSuccessTracker;
}

namespace autofill_assistant {

class MockScriptExecutorDelegate : public ScriptExecutorDelegate {
 public:
  MockScriptExecutorDelegate();
  ~MockScriptExecutorDelegate() override;

  MOCK_METHOD(const ClientSettings&, GetSettings, (), (override));
  MOCK_METHOD(const GURL&, GetCurrentURL, (), (override));
  MOCK_METHOD(const GURL&, GetDeeplinkURL, (), (override));
  MOCK_METHOD(const GURL&, GetScriptURL, (), (override));
  MOCK_METHOD(Service*, GetService, (), (override));
  MOCK_METHOD(WebController*, GetWebController, (), (override));
  MOCK_METHOD(const TriggerContext*, GetTriggerContext, (), (override));
  MOCK_METHOD(autofill::PersonalDataManager*,
              GetPersonalDataManager,
              (),
              (override));
  MOCK_METHOD(WebsiteLoginManager*, GetWebsiteLoginManager, (), (override));
  MOCK_METHOD(password_manager::PasswordChangeSuccessTracker*,
              GetPasswordChangeSuccessTracker,
              (),
              (override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (override));
  MOCK_METHOD(const std::string, GetLocale, (), (override));
  MOCK_METHOD(void,
              SetJsFlowLibrary,
              (const std::string& js_flow_library),
              (override));
  MOCK_METHOD(JsFlowDevtoolsWrapper*, GetJsFlowDevtoolsWrapper, (), (override));

  MOCK_METHOD(std::string,
              GetEmailAddressForAccessTokenAccount,
              (),
              (override));
  MOCK_METHOD(ukm::UkmRecorder*, GetUkmRecorder, (), (override));
  MOCK_METHOD(bool, EnterState, (AutofillAssistantState state), (override));
  MOCK_CONST_METHOD0(GetState, AutofillAssistantState());
  MOCK_METHOD(void,
              SetOverlayBehavior,
              (ConfigureUiStateProto::OverlayBehavior overlay_behavior),
              (override));
  MOCK_METHOD(void,
              SetTouchableElementArea,
              (const ElementAreaProto& element),
              (override));
  MOCK_METHOD(void,
              WriteUserData,
              (base::OnceCallback<void(UserData*, UserDataFieldChange*)>
                   write_callback),
              (override));
  MOCK_METHOD(ViewportMode, GetViewportMode, (), (override));
  MOCK_METHOD(void, SetViewportMode, (ViewportMode mode), (override));
  MOCK_METHOD(void,
              SetClientSettings,
              (const ClientSettingsProto& client_settings),
              (override));
  MOCK_METHOD(UserModel*, GetUserModel, (), (override));
  MOCK_METHOD(UserData*, GetUserData, (), (override));
  MOCK_METHOD(void, ExpectNavigation, (), (override));
  MOCK_METHOD(bool, IsNavigatingToNewDocument, (), (override));
  MOCK_METHOD(bool, HasNavigationError, (), (override));
  MOCK_METHOD(void, RequireUI, (), (override));
  MOCK_METHOD(void,
              AddNavigationListener,
              (NavigationListener * listener),
              (override));
  MOCK_METHOD(void,
              RemoveNavigationListener,
              (NavigationListener * listener),
              (override));
  MOCK_METHOD(void,
              SetBrowseDomainsAllowlist,
              (std::vector<std::string> domains),
              (override));
  MOCK_METHOD(void, SetBrowseModeInvisible, (bool invisible), (override));
  MOCK_METHOD(ProcessedActionStatusDetailsProto&, GetLogInfo, (), (override));
  MOCK_METHOD(bool, ShouldShowWarning, (), (override));
  MOCK_METHOD(bool, MustUseBackendData, (), (const override));
  MOCK_METHOD(void,
              OnActionsResponseReceived,
              (const RoundtripNetworkStats& network_stats),
              (override));
  MOCK_CONST_METHOD1(IsXmlSigned, bool(const std::string& xml_string));
  MOCK_CONST_METHOD2(
      ExtractValuesFromSingleTagXml,
      const std::vector<std::string>(const std::string& xml_string,
                                     const std::vector<std::string>& keys));

 private:
  ClientSettings client_settings_;
  ProcessedActionStatusDetailsProto log_info_;
  const GURL default_url_ = GURL("https://example.com/");
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SCRIPT_EXECUTOR_DELEGATE_H_
