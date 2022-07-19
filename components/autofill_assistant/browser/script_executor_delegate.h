// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/js_flow_devtools_wrapper.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/tts_button_state.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/viewport_mode.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace password_manager {
class PasswordChangeSuccessTracker;
}  // namespace password_manager

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {

class Service;
class TriggerContext;
class UserModel;
class WebController;
class WebsiteLoginManager;
struct ClientSettings;

class ScriptExecutorDelegate {
 public:
  class NavigationListener : public base::CheckedObserver {
   public:
    // The values returned by IsNavigatingToNewDocument() or
    // HasNavigationError() might have changed.
    virtual void OnNavigationStateChanged() = 0;
  };

  virtual const ClientSettings& GetSettings() = 0;
  virtual const GURL& GetCurrentURL() = 0;
  virtual const GURL& GetDeeplinkURL() = 0;
  virtual const GURL& GetScriptURL() = 0;
  virtual Service* GetService() = 0;
  virtual WebController* GetWebController() = 0;
  virtual const TriggerContext* GetTriggerContext() = 0;
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;
  virtual WebsiteLoginManager* GetWebsiteLoginManager() = 0;
  virtual password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() = 0;
  virtual content::WebContents* GetWebContents() = 0;
  virtual const std::string GetLocale() = 0;

  virtual void SetJsFlowLibrary(const std::string& js_flow_library) = 0;
  virtual JsFlowDevtoolsWrapper* GetJsFlowDevtoolsWrapper() = 0;

  virtual std::string GetEmailAddressForAccessTokenAccount() = 0;
  virtual ukm::UkmRecorder* GetUkmRecorder() = 0;

  // Enters the given state. Returns true if the state was changed.
  virtual bool EnterState(AutofillAssistantState state) = 0;

  // Make the area of the screen that correspond to the given elements
  // touchable.
  virtual void SetTouchableElementArea(const ElementAreaProto& element) = 0;

  // Returns the current state.
  virtual AutofillAssistantState GetState() const = 0;

  virtual void SetOverlayBehavior(
      ConfigureUiStateProto::OverlayBehavior overlay_behavior) = 0;

  virtual void WriteUserData(
      base::OnceCallback<void(UserData*, UserDataFieldChange*)>
          write_callback) = 0;
  virtual ViewportMode GetViewportMode() = 0;
  virtual void SetViewportMode(ViewportMode mode) = 0;
  virtual void SetClientSettings(
      const ClientSettingsProto& client_settings) = 0;
  virtual UserModel* GetUserModel() = 0;
  virtual UserData* GetUserData() = 0;

  // The next navigation is expected and will not cause an error.
  virtual void ExpectNavigation() = 0;

  // Returns true if a new document is being fetched for the main frame.
  //
  // Navigation ends once a response, with its associated URL has been
  // committed, whether the response is successful or not.
  //
  // Navigation of frames other than the main frame, loading of resource or
  // navigation to the same document aren't reported.
  //
  // Changes to this value is reported to Listener::OnNavigationStateChanged()
  virtual bool IsNavigatingToNewDocument() = 0;

  // Returns true if Chrome failed to fetch the response for its main document
  // during its last attempt.
  //
  // This is cleared once a page for the main document has been successfully
  // navigated to a new document.
  //
  // Navigation of frames other than the main frame, loading of resource or
  // navigation to the same document aren't taken into account for this value.
  //
  // Changes to this value is reported to Listener::OnNavigationStateChanged()
  virtual bool HasNavigationError() = 0;

  // Makes no area of the screen touchable.
  void ClearTouchableElementArea() {
    SetTouchableElementArea(ElementAreaProto::default_instance());
  }

  // Force showing the UI, if necessary. This is useful when executing a direct
  // action which realizes it needs to interact with the user. The UI stays up
  // until the end of the flow.
  virtual void RequireUI() = 0;

  // Register a navigation listener that can be told about navigation state
  // changes. Duplicate calls are ignored.
  virtual void AddNavigationListener(NavigationListener* listener) = 0;

  // Removes a previously registered navigation listener. Does nothing if no
  // such listener exists.
  virtual void RemoveNavigationListener(NavigationListener* listener) = 0;

  // Set the domains allowlist for browse mode.
  virtual void SetBrowseDomainsAllowlist(std::vector<std::string> domains) = 0;

  // Sets whether browse mode should be invisible or not. Must be set before
  // calling |EnterState(BROWSE)| to take effect.
  virtual void SetBrowseModeInvisible(bool invisible) = 0;

  // Whether the slow connection or website warning should be shown. Depends on
  // the state at the moment of the invocation.
  virtual bool ShouldShowWarning() = 0;

  // Get modifiable log information gathered while executing the action. This
  // gets attached to the action's response if non empty.
  virtual ProcessedActionStatusDetailsProto& GetLogInfo() = 0;

  // Returns whether or not this instance of Autofill Assistant must use a
  // backend endpoint to query data.
  virtual bool MustUseBackendData() const = 0;

  // Called when a new action response has been received. Used for metrics.
  virtual void OnActionsResponseReceived(
      const RoundtripNetworkStats& network_stats) = 0;

 protected:
  virtual ~ScriptExecutorDelegate() = default;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_
