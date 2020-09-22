// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/viewport_mode.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/icu/source/common/unicode/umachine.h"

class GURL;

namespace autofill {
class AutofillProfile;
class CreditCard;
struct FormData;
struct FormFieldData;
class PersonalDataManager;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {
class ClientStatus;
struct ClientSettings;
struct CollectUserDataOptions;
class UserAction;
class WebsiteLoginManager;

// Action delegate called when processing actions.
class ActionDelegate {
 public:
  virtual ~ActionDelegate() = default;

  // Show status message on the bottom bar.
  virtual void SetStatusMessage(const std::string& message) = 0;

  // Returns the current status message. Usually used to restore a message after
  // the action.
  virtual std::string GetStatusMessage() = 0;

  // Show a bubble / tooltip on the bottom bar. Dismisses the bubble if
  // |message| is empty.
  virtual void SetBubbleMessage(const std::string& message) = 0;

  // Returns the current bubble / status message. Usually used to restore a
  // message after the action.
  virtual std::string GetBubbleMessage() = 0;

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
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

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
      bool allow_interrupt,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Find an element specified by |selector| on the web page.
  virtual void FindElement(const Selector&,
                           ElementFinder::Callback callback) = 0;

  // Click or tap the |element|.
  virtual void ClickOrTapElement(
      ClickType click_type,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Wait for the |element|'s document to become interactive.
  virtual void WaitForDocumentToBecomeInteractive(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Scroll the |element| into view.
  virtual void ScrollIntoView(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

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
  virtual void CleanUpAfterPrompt() = 0;

  // Set the list of whitelisted domains to be used when we enter a browse
  // state. This list is used to determine whether a user initiated navigation
  // to a different domain or subdomain is allowed.
  virtual void SetBrowseDomainsWhitelist(std::vector<std::string> domains) = 0;

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
      base::OnceCallback<void(UserData*, UserData::FieldChange*)>
          write_callback) = 0;

  using GetFullCardCallback =
      base::OnceCallback<void(std::unique_ptr<autofill::CreditCard> card,
                              const base::string16& cvc)>;

  // Asks for the full card information for |credit_card|. Might require the
  // user entering CVC.
  virtual void GetFullCard(const autofill::CreditCard* credit_card,
                           GetFullCardCallback callback) = 0;

  // Fill the address form given by |selector| with the given address
  // |profile|. |profile| cannot be nullptr.
  virtual void FillAddressForm(
      const autofill::AutofillProfile* profile,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Fill the card form given by |selector| with the given |card| and its
  // |cvc|. Return result asynchronously through |callback|.
  virtual void FillCardForm(
      std::unique_ptr<autofill::CreditCard> card,
      const base::string16& cvc,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Return |FormData| and |FormFieldData| for the element identified with
  // |selector|. The result is returned asynchronously through |callback|.
  virtual void RetrieveElementFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback) = 0;

  // Select the option to be picked given by the |value| in the |element|.
  virtual void SelectOption(
      const std::string& value,
      DropdownSelectStrategy select_strategy,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Focus on element given by |selector|. |top_padding| specifies the padding
  // between focused element and the top.
  virtual void FocusElement(
      const Selector& selector,
      const TopPadding& top_padding,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Sets selector of areas that can be manipulated:
  // - after the end of the script and before the beginning of the next script.
  // - during the next call to SetUserActions()
  // whichever comes first.
  virtual void SetTouchableElementArea(
      const ElementAreaProto& touchable_element_area) = 0;

  // Highlight the element given by |selector|.
  virtual void HighlightElement(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Get the value of |selector| and return the result through |callback|. The
  // returned value might be false, if the element cannot be found, true and the
  // empty string in case of error or empty value.
  virtual void GetFieldValue(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback) = 0;

  // Set the |value| of field |element| and return the result through
  // |callback|. If |simulate_key_presses| is true, the value will be set by
  // clicking the field and then simulating key presses, otherwise the `value`
  // attribute will be set directly.
  virtual void SetFieldValue(
      const std::string& value,
      KeyboardValueFillStrategy fill_strategy,
      int key_press_delay_in_millisecond,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Set the |value| of all the |attributes| of the |element|.
  virtual void SetAttribute(
      const std::vector<std::string>& attributes,
      const std::string& value,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Sets the keyboard focus to |element| and inputs the specified codepoints.
  // Returns the result through |callback|.
  virtual void SendKeyboardInput(
      const std::vector<UChar32>& codepoints,
      int key_press_delay_in_millisecond,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Return the outerHTML of an element given by |selector|.
  virtual void GetOuterHtml(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback) = 0;

  // Return the tag of the |element|. In case of an error, returns an empty
  // string.
  virtual void GetElementTag(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback) = 0;

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
  // |min_ready_state| in |optional_frame| or, if it is empty, in the main
  // document.
  virtual void WaitForDocumentReadyState(
      const Selector& optional_frame,
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback) = 0;

  // Gets the value of Document.readyState in |optional_frame| or, if it is
  // empty, in the main document.
  virtual void GetDocumentReadyState(
      const Selector& optional_frame,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback) = 0;

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url) = 0;

  // Shut down Autofill Assistant at the end of the current script.
  virtual void Shutdown() = 0;

  // Shut down Autofill Assistant and closes Chrome.
  virtual void Close() = 0;

  // Get current personal data manager.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  // Get current login fetcher.
  virtual WebsiteLoginManager* GetWebsiteLoginManager() = 0;

  // Get associated web contents.
  virtual content::WebContents* GetWebContents() = 0;

  // Returns the e-mail address that corresponds to the access token or an empty
  // string.
  virtual std::string GetEmailAddressForAccessTokenAccount() = 0;

  // Returns the locale for the current device or platform.
  virtual std::string GetLocale() = 0;

  // Sets or updates contextual information.
  // Passing nullptr clears the contextual information.
  virtual void SetDetails(std::unique_ptr<Details> details) = 0;

  // Clears the info box.
  virtual void ClearInfoBox() = 0;

  // Sets or updates info box.
  virtual void SetInfoBox(const InfoBox& infoBox) = 0;

  // Set the progress bar at |progress|%.
  virtual void SetProgress(int progress) = 0;

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
      const ShowProgressBarProto::StepProgressBarConfiguration&
          configuration) = 0;

  // Set the viewport mode.
  virtual void SetViewportMode(ViewportMode mode) = 0;

  // Get the current viewport mode.
  virtual ViewportMode GetViewportMode() = 0;

  // Set the peek mode.
  virtual void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) = 0;

  // Checks the current peek mode.
  virtual ConfigureBottomSheetProto::PeekMode GetPeekMode() = 0;

  // Expands the bottom sheet. This is the same as the user swiping up.
  virtual void ExpandBottomSheet() = 0;

  // Collapses the bottom sheet to the current peek state as set by
  // |SetPeekMode|. This is the same as the user swiping down.
  virtual void CollapseBottomSheet() = 0;

  // Calls the callback once the main document window has been resized.
  virtual void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) = 0;

  // Returns the current client settings.
  virtual const ClientSettings& GetSettings() = 0;

  // Show a form to the user and call |changed_callback| with its values
  // whenever there is a change. |changed_callback| will be called directly with
  // the initial values of the form directly after this call. Returns true if
  // the form was correctly set, false otherwise. The latter can happen if the
  // form contains unsupported or invalid inputs.
  virtual bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) = 0;

  // Force showing the UI if no UI is shown. This is useful when executing a
  // direct action which realizes it needs to interact with the user. Once
  // shown, the UI stays up until the end of the flow.
  virtual void RequireUI() = 0;

  // Gets the user data.
  virtual const UserData* GetUserData() const = 0;

  // Access to the user model.
  virtual UserModel* GetUserModel() = 0;

  // Show |generic_ui| to the user and call |end_action_callback| when done.
  // Note that this callback needs to be tied to one or multiple interactions
  // specified in |generic_ui|, as otherwise it will never be called.
  // |view_inflation_finished_callback| should be called immediately after
  // view inflation, with a status indicating whether view inflation succeeded.
  virtual void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)> end_action_callback,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) = 0;

  // Clears the generic UI. This will remove all corresponding views from the
  // view hierarchy and remove all corresponding interactions. Note that
  // |user_model| will persist and will not be affected by this call.
  virtual void ClearGenericUi() = 0;

  // Sets the OverlayBehavior.
  virtual void SetOverlayBehavior(
      ConfigureUiStateProto::OverlayBehavior overlay_behavior) = 0;

  virtual base::WeakPtr<ActionDelegate> GetWeakPtr() = 0;

 protected:
  ActionDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
