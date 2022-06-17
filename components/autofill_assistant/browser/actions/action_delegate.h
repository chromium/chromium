// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/js_flow_devtools_wrapper.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "components/autofill_assistant/browser/public/external_script_controller.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/tts_button_state.h"
#include "components/autofill_assistant/browser/viewport_mode.h"

class GURL;

namespace autofill {
class ClientStatus;
class CreditCard;
struct FormData;
struct FormFieldData;
class PersonalDataManager;
}  // namespace autofill

namespace base {
class TimeDelta;
template <typename T>
class WeakPtr;
}  // namespace base

namespace password_manager {
class PasswordChangeSuccessTracker;
}

namespace content {
class WebContents;
}  // namespace content

namespace ukm {
class UkmRecorder;
}

namespace autofill_assistant {
class BatchElementChecker;
class ClientSettingsProto;
class ClientStatus;
class Details;
class ElementFinderResult;
struct Selector;
class WaitForDomObserver;
class ElementStore;
class GenericUserInterfaceProto;
class FormProto;
class FormProto_Result;
class InfoBox;
class UserAction;
class UserData;
class UserModel;
class WebController;
class WebsiteLoginManager;
struct ClientSettings;
struct CollectUserDataOptions;
class ShowProgressBarProto_StepProgressBarConfiguration;
class ProcessedActionStatusDetailsProto;
class GetUserDataResponseProto;
class ElementAreaProto;
class ExternalActionProto;

enum ConfigureBottomSheetProto_PeekMode : int;
enum ConfigureUiStateProto_OverlayBehavior : int;
enum DocumentReadyState : int;
enum class UserDataFieldChange;
enum class UserDataEventField;

// Action delegate called when processing actions.
class ActionDelegate {
 public:
  virtual ~ActionDelegate() = default;

  // Show status message on the bottom bar. Additionally, it overrides the TTS
  // message and stops any ongoing TTS.
  virtual void SetStatusMessage(const std::string& message) = 0;

  // Returns the current status message. Usually used to restore a message after
  // the action.
  virtual std::string GetStatusMessage() const = 0;

  // Show a bubble / tooltip on the bottom bar. Dismisses the bubble if
  // |message| is empty.
  virtual void SetBubbleMessage(const std::string& message) = 0;

  // Returns the current bubble / status message. Usually used to restore a
  // message after the action.
  virtual std::string GetBubbleMessage() const = 0;

  // Overrides the TTS message to be played when requested. The TTS message
  // defaults to the current status message but can be overridden (until the
  // next status message change) with this method. Stops any ongoing TTS.
  virtual void SetTtsMessage(const std::string& message) = 0;

  // Returns the current TTS button state.
  virtual TtsButtonState GetTtsButtonState() const = 0;

  // Play TTS message if TextToSpeech is enabled (via "ENABLE_TTS"
  // script param). Will also stop any ongoing TTS message.
  virtual void MaybePlayTtsMessage() = 0;

  // Checks one or more elements.
  virtual void RunElementChecks(BatchElementChecker* checker) = 0;

  // Wait for a short time for a given selector to appear.
  //
  // Most actions should call this before issuing a command on an element, to
  // account for timing issues with needed elements not showing up right away.
  //
  // Longer-time waiting should still be controlled explicitly, using
  // WaitForDom.
  //
  // TODO(crbug.com/806868): Consider embedding that wait right into
  // WebController and eliminate double-lookup.
  virtual void ShortWaitForElement(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
          callback) = 0;

  // Same as the above, but will show a warning to the user if the website
  // causes the checks to take longer than a given timeout.
  virtual void ShortWaitForElementWithSlowWarning(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
          callback) = 0;

  // Wait for up to |max_wait_time| for element conditions to match on the page,
  // then call |callback| with the last status.
  //
  // |check_elements| should register the elements to check, process their state
  // and reports its decision to the callback it's passed. WaitForDom retries as
  // long as the decision is not OK, and max_wait_time is not reached.
  //
  // If |allow_interrupt| interrupts can run while waiting.
  virtual void WaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_observer_mode,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
          callback) = 0;

  // Same as the above, but will show a warning to the user if the website
  // causes the checks to take longer than a given timeout.
  virtual void WaitForDomWithSlowWarning(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
          callback) = 0;

  // Find an element specified by |selector| on the web page.
  virtual void FindElement(
      const Selector&,
      base::OnceCallback<void(const ClientStatus&,
                              std::unique_ptr<ElementFinderResult>)> callback)
      const = 0;

  // Find all elements matching |selector|. If there are no matches, the status
  // will be ELEMENT_RESOLUTION_FAILED.
  virtual void FindAllElements(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              std::unique_ptr<ElementFinderResult>)> callback)
      const = 0;

  // Have the UI enter the prompt mode and make the given actions available.
  //
  // While a prompt is in progress, the UI looks the same as it does between
  // scripts, even though we're in the middle of a script. This includes
  // allowing access to the touchable elements set previously, in the same
  // script.
  //
  // When |browse_mode| is true, navigation and user gestures like go_back no
  // longer shut down the autofill assistant client, except for navigating to
  // a different domain.
  virtual void Prompt(
      std::unique_ptr<std::vector<UserAction>> user_actions,
      bool disable_force_expand_sheet,
      base::OnceCallback<void()> end_on_navigation_callback = base::DoNothing(),
      bool browse_mode = false,
      bool browse_mode_invisible = false) = 0;

  // Have the UI leave the prompt state and go back to its previous state.
  virtual void CleanUpAfterPrompt(bool consume_touchable_area = true) = 0;

  // Set the list of allowed domains to be used when we enter a browse state.
  // This list is used to determine whether a user initiated navigation to a
  // different domain or subdomain is allowed.
  virtual void SetBrowseDomainsAllowlist(std::vector<std::string> domains) = 0;

  // Asks the user to provide the requested user data.
  virtual void CollectUserData(
      CollectUserDataOptions* collect_user_data_options) = 0;

  // Updates the most recent successful user data options.
  virtual void SetLastSuccessfulUserDataOptions(
      std::unique_ptr<CollectUserDataOptions> collect_user_data_options) = 0;

  // Provides read access to the most recent successful user data options.
  // Returns nullptr if there is no such object.
  virtual const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const = 0;

  // Executes |write_callback| on the currently stored user_data and
  // user_data_options.
  virtual void WriteUserData(
      base::OnceCallback<void(UserData*, UserDataFieldChange*)>
          write_callback) = 0;

  using GetFullCardCallback =
      base::OnceCallback<void(const ClientStatus& status,
                              std::unique_ptr<autofill::CreditCard> card,
                              const std::u16string& cvc)>;

  // Asks for the full card information for |credit_card|. Might require the
  // user entering CVC.
  virtual void GetFullCard(const autofill::CreditCard* credit_card,
                           GetFullCardCallback callback) = 0;

  // Return |FormData| and |FormFieldData| for the element identified with
  // |selector|. The result is returned asynchronously through |callback|.
  virtual void RetrieveElementFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback) = 0;

  // Store the element that is being scrolled to, such that it can be restored
  // after an interrupt.
  virtual void StoreScrolledToElement(const ElementFinderResult& element) = 0;

  // Sets selector of areas that can be manipulated:
  // - after the end of the script and before the beginning of the next script.
  // - during the next call to SetUserActions()
  // whichever comes first.
  virtual void SetTouchableElementArea(
      const ElementAreaProto& touchable_element_area) = 0;

  // Make the next call to WaitForNavigation to expect a navigation event that
  // started after this call.
  virtual void ExpectNavigation() = 0;

  // Returns true if the expected navigation, on which WaitForNavigation() might
  // be waiting, has started and maybe even ended.
  virtual bool ExpectedNavigationHasStarted() = 0;

  // Wait for a navigation event to end that started after the last call to
  // ExpectNavigation().
  //
  // If ExpectNavigation() was never called in the script, the function returns
  // false and never calls the callback.
  //
  // The callback is passed true if navigation succeeded. The callback might be
  // called immediately if navigation has already succeeded.
  virtual bool WaitForNavigation(
      base::OnceCallback<void(bool)> on_navigation_done) = 0;

  // Waits for the value of Document.readyState to reach at least
  // |min_ready_state| in |optional_frame_element| or, if it is empty, in the
  // main document.
  virtual void WaitForDocumentReadyState(
      base::TimeDelta max_wait_time,
      DocumentReadyState min_ready_state,
      const ElementFinderResult& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState,
                              base::TimeDelta)> callback) = 0;

  // Wait until the defined |min_ready_state| in |optional_frame_element| is
  // reached or time out. If the |optional_frame_element| is empty, check the
  // main document.
  virtual void WaitUntilDocumentIsInReadyState(
      base::TimeDelta max_wait_time,
      DocumentReadyState min_ready_state,
      const ElementFinderResult& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
          callback) = 0;

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url) = 0;

  // Shut down Autofill Assistant at the end of the current script.
  virtual void Shutdown(bool show_feedback_chip) = 0;

  // Shut down Autofill Assistant and closes Chrome.
  virtual void Close() = 0;

  // Get current personal data manager.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() const = 0;

  // Get current login manager.
  virtual WebsiteLoginManager* GetWebsiteLoginManager() const = 0;

  // Get current password change success tracker.
  virtual password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() const = 0;

  // Get associated web contents.
  virtual content::WebContents* GetWebContents() const = 0;

  // Get the wrapper that owns the web contents and devtools client for js
  // flows.
  virtual JsFlowDevtoolsWrapper* GetJsFlowDevtoolsWrapper() const = 0;

  // Get the ElementStore.
  virtual ElementStore* GetElementStore() const = 0;

  // Get the WebController.
  virtual WebController* GetWebController() const = 0;

  // Returns the e-mail address that corresponds to the access token or an empty
  // string.
  virtual std::string GetEmailAddressForAccessTokenAccount() const = 0;

  // Returns the UkmRecorder.
  virtual ukm::UkmRecorder* GetUkmRecorder() const = 0;

  // Sets or updates contextual information.
  // Passing nullptr clears the contextual information.
  virtual void SetDetails(std::unique_ptr<Details> details,
                          base::TimeDelta delay) = 0;

  // Append |details| to the current contextual information.
  // Passing nullptr does nothing.
  virtual void AppendDetails(std::unique_ptr<Details> details,
                             base::TimeDelta delay) = 0;

  // Clears the info box.
  virtual void ClearInfoBox() = 0;

  // Sets or updates info box.
  virtual void SetInfoBox(const InfoBox& infoBox) = 0;

  // Set the progress bar at the |active_step| linked to the given
  // |active_step_identifier|.
  virtual bool SetProgressActiveStepIdentifier(
      const std::string& active_step_identifier) = 0;

  // Set the progress bar at the |active_step|.
  virtual void SetProgressActiveStep(int active_step) = 0;

  // Shows the progress bar when |visible| is true. Hides it when false.
  virtual void SetProgressVisible(bool visible) = 0;

  // Sets the error state of the progress bar to |error|.
  virtual void SetProgressBarErrorState(bool error) = 0;

  // Sets a new step progress bar configuration.
  virtual void SetStepProgressBarConfiguration(
      const ShowProgressBarProto_StepProgressBarConfiguration&
          configuration) = 0;

  // Set the viewport mode.
  virtual void SetViewportMode(ViewportMode mode) = 0;

  // Get the current viewport mode.
  virtual ViewportMode GetViewportMode() const = 0;

  // Set the peek mode.
  virtual void SetPeekMode(ConfigureBottomSheetProto_PeekMode peek_mode) = 0;

  // Checks the current peek mode.
  virtual ConfigureBottomSheetProto_PeekMode GetPeekMode() const = 0;

  // Expands the bottom sheet. This is the same as the user swiping up.
  virtual void ExpandBottomSheet() = 0;

  // Collapses the bottom sheet to the current peek state as set by
  // |SetPeekMode|. This is the same as the user swiping down.
  virtual void CollapseBottomSheet() = 0;

  // Calls the callback once the main document window has been resized.
  virtual void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Returns the current client settings.
  virtual const ClientSettings& GetSettings() const = 0;

  // Sets/Updates current client settings.
  virtual void SetClientSettings(
      const ClientSettingsProto& client_settings) = 0;

  // Show a form to the user and call |changed_callback| with its values
  // whenever there is a change. |changed_callback| will be called directly with
  // the initial values of the form directly after this call. Returns true if
  // the form was correctly set, false otherwise. The latter can happen if the
  // form contains unsupported or invalid inputs.
  virtual bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto_Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) = 0;

  // Force showing the UI if no UI is shown. This is useful when executing a
  // direct action which realizes it needs to interact with the user. Once
  // shown, the UI stays up until the end of the flow.
  virtual void RequireUI() = 0;

  // Gets the user data.
  virtual const UserData* GetUserData() const = 0;

  // Access to the user model.
  virtual UserModel* GetUserModel() const = 0;

  // Show |generic_ui| to the user and call |end_action_callback| when done.
  // Note that this callback needs to be tied to one or multiple interactions
  // specified in |generic_ui|, as otherwise it will never be called.
  // |view_inflation_finished_callback| will be called immediately after
  // view inflation, with a status indicating whether view inflation succeeded.
  virtual void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)> end_action_callback,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) = 0;

  // Show |generic_ui| to the user.
  // |view_inflation_finished_callback| will be called immediately after
  // view inflation, with a status indicating whether view inflation succeeded.
  virtual void SetPersistentGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) = 0;

  // Clears the generic UI. This will remove all corresponding views from the
  // view hierarchy and remove all corresponding interactions. Note that
  // |user_model| will persist and will not be affected by this call.
  virtual void ClearGenericUi() = 0;

  // Clears the persistent generic UI. This will remove all corresponding views
  // from the view hierarchy and remove all corresponding interactions. Note
  // that |user_model| will persist and will not be affected by this call.
  virtual void ClearPersistentGenericUi() = 0;

  // Sets the OverlayBehavior.
  virtual void SetOverlayBehavior(
      ConfigureUiStateProto_OverlayBehavior overlay_behavior) = 0;

  // Maybe shows a warning letting the user know that the website is unusually
  // slow, depending on the current settings.
  virtual void MaybeShowSlowWebsiteWarning(
      base::OnceCallback<void(bool)> callback) = 0;

  // Maybe shows a warning letting the user know that a slow connection was
  // detected, depending on the current settings.
  virtual void MaybeShowSlowConnectionWarning() = 0;

  // Get modifiable log information gathered while executing the action. This
  // gets attached to the action's response if non empty.
  virtual ProcessedActionStatusDetailsProto& GetLogInfo() = 0;

  // Sends a request to retrieve the required user data for this flow. Returns
  // the result through the |callback|. Enters the |RUNNING| state while doing
  // so.
  virtual void RequestUserData(
      UserDataEventField event_field,
      const CollectUserDataOptions& options,
      base::OnceCallback<void(bool, const GetUserDataResponseProto&)>
          callback) = 0;

  // Whether the current flow supports external actions.
  virtual bool SupportsExternalActions() = 0;

  // Executes the |external_action|.
  virtual void RequestExternalAction(
      const ExternalActionProto& external_action,
      base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
          start_dom_checks_callback,
      base::OnceCallback<void(const external::Result& result)>
          end_action_callback) = 0;

  // Returns whether or not this instance of Autofill Assistant must use a
  // backend endpoint to query data.
  virtual bool MustUseBackendData() const = 0;

  // Maybe sets the previously executed action. JS flow actions are excluded
  // because they act as a script executor.
  virtual void MaybeSetPreviousAction(
      const ProcessedActionProto& processed_action) = 0;

  virtual base::WeakPtr<ActionDelegate> GetWeakPtr() const = 0;

 protected:
  ActionDelegate() = default;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
