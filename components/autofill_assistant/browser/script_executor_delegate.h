// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/viewport_mode.h"
#include "url/gurl.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {

class Service;
class WebController;
struct ClientSettings;
class TriggerContext;
class WebsiteLoginManager;
class EventHandler;
class UserModel;

class ScriptExecutorDelegate {
 public:
  class NavigationListener : public base::CheckedObserver {
   public:
    // The values returned by IsNavigatingToNewDocument() or
    // HasNavigationError() might have changed.
    virtual void OnNavigationStateChanged() = 0;
  };

  class Listener : public base::CheckedObserver {
   public:
    // The execution flow is being stopped.
    virtual void OnPause(const std::string& message,
                         const std::string& button_label) = 0;
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
  virtual content::WebContents* GetWebContents() = 0;
  virtual std::string GetEmailAddressForAccessTokenAccount() = 0;
  virtual std::string GetLocale() = 0;

  // Enters the given state. Returns true if the state was changed.
  virtual bool EnterState(AutofillAssistantState state) = 0;

  virtual void SetOverlayBehavior(
      ConfigureUiStateProto::OverlayBehavior overlay_behavior) = 0;

  // Make the area of the screen that correspond to the given elements
  // touchable.
  virtual void SetTouchableElementArea(const ElementAreaProto& element) = 0;
  virtual void SetStatusMessage(const std::string& message) = 0;
  virtual std::string GetStatusMessage() const = 0;
  virtual void SetBubbleMessage(const std::string& message) = 0;
  virtual std::string GetBubbleMessage() const = 0;
  virtual void SetDetails(std::unique_ptr<Details> details) = 0;
  virtual void SetInfoBox(const InfoBox& info_box) = 0;
  virtual void ClearInfoBox() = 0;
  virtual void SetCollectUserDataOptions(
      CollectUserDataOptions* collect_user_data_options) = 0;
  virtual void SetLastSuccessfulUserDataOptions(
      std::unique_ptr<CollectUserDataOptions> collect_user_data_options) = 0;
  virtual const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const = 0;
  virtual void WriteUserData(
      base::OnceCallback<void(UserData*, UserData::FieldChange*)>
          write_callback) = 0;
  virtual void SetProgress(int progress) = 0;
  virtual bool SetProgressActiveStepIdentifier(
      const std::string& active_step_identifier) = 0;
  virtual void SetProgressActiveStep(int active_step) = 0;
  virtual void SetProgressVisible(bool visible) = 0;
  virtual void SetProgressBarErrorState(bool error) = 0;
  virtual void SetStepProgressBarConfiguration(
      const ShowProgressBarProto::StepProgressBarConfiguration&
          configuration) = 0;
  virtual void SetUserActions(
      std::unique_ptr<std::vector<UserAction>> user_action) = 0;
  virtual ViewportMode GetViewportMode() = 0;
  virtual void SetViewportMode(ViewportMode mode) = 0;
  virtual void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) = 0;
  virtual ConfigureBottomSheetProto::PeekMode GetPeekMode() = 0;
  virtual void ExpandBottomSheet() = 0;
  virtual void CollapseBottomSheet() = 0;
  virtual bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) = 0;
  virtual UserModel* GetUserModel() = 0;
  virtual EventHandler* GetEventHandler() = 0;

  // Makes no area of the screen touchable.
  void ClearTouchableElementArea() {
    SetTouchableElementArea(ElementAreaProto::default_instance());
  }

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

  // Add a listener that can be told about changes. Duplicate calls are ignored.
  virtual void AddListener(Listener* listener) = 0;

  // Removes a previously registered listener. Does nothing if no such listener
  // exists.
  virtual void RemoveListener(Listener* listener) = 0;

  // Set how the sheet should behave when entering a prompt state.
  virtual void SetExpandSheetForPromptAction(bool expand) = 0;

  // Set the domains whitelist for browse mode.
  virtual void SetBrowseDomainsWhitelist(std::vector<std::string> domains) = 0;

  // Sets the generic UI to show to the user.
  virtual void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)> end_action_callback,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) = 0;

  // Clears the generic UI.
  virtual void ClearGenericUi() = 0;

  // Sets whether browse mode should be invisible or not. Must be set before
  // calling |EnterState(BROWSE)| to take effect.
  virtual void SetBrowseModeInvisible(bool invisible) = 0;

 protected:
  virtual ~ScriptExecutorDelegate() {}
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_
