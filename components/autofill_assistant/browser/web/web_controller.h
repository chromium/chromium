// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_input.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_network.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/web/check_on_top_worker.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_position_getter.h"
#include "components/autofill_assistant/browser/web/element_rect_getter.h"
#include "components/autofill_assistant/browser/web/send_keyboard_input_worker.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/icu/source/common/unicode/umachine.h"
#include "url/gurl.h"

namespace autofill {
class AutofillProfile;
class ContentAutofillDriver;
class CreditCard;
struct FormData;
struct FormFieldData;
}  // namespace autofill

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace autofill_assistant {

// Controller to interact with the web pages.
//
// WARNING: Accessing or modifying page elements must be run in sequence: wait
// until the result of the first operation has been given to the callback before
// starting a new operation.
//
// TODO(crbug.com/806868): Figure out the reason for this limitation and fix it.
// Also, consider structuring the WebController to make it easier to run
// multiple operations, whether in sequence or in parallel.
class WebController {
 public:
  // Create web controller for a given |web_contents|. |settings| must be valid
  // for the lifetime of the controller.
  static std::unique_ptr<WebController> CreateForWebContents(
      content::WebContents* web_contents);

  // |web_contents| and |settings| must outlive this web controller.
  WebController(content::WebContents* web_contents,
                std::unique_ptr<DevtoolsClient> devtools_client);
  virtual ~WebController();

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url);

  // Find the element given by |selector|. If multiple elements match
  // |selector| and if |strict_mode| is false, return the first one that is
  // found. Otherwise if |strict-mode| is true, do not return any.
  //
  // To check multiple elements, use a BatchElementChecker.
  virtual void FindElement(const Selector& selector,
                           bool strict_mode,
                           ElementFinder::Callback callback);

  // Find all elements matching |selector|. If there are no matches, the status
  // will be ELEMENT_RESOLUTION_FAILED.
  virtual void FindAllElements(const Selector& selector,
                               ElementFinder::Callback callback);

  // Scroll the |element| into view if needed, center the element on the screen
  // if specified.
  virtual void ScrollIntoView(
      bool center,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Perform a mouse left button click or a touch tap on the |element|
  // return the result through callback.
  virtual void ClickOrTapElement(
      const ElementFinder::Result& element,
      ClickType click_type,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Get a stable position of the given element. Fail with ELEMENT_UNSTABLE if
  // the element position doesn't stabilize quickly enough.
  virtual void WaitUntilElementIsStable(
      int max_rounds,
      base::TimeDelta check_interval,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback);

  // Check whether the center given element is on top. Fail with
  // ELEMENT_NOT_ON_TOP if the center of the element is covered.
  virtual void CheckOnTop(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Fill the address form given by |selector| with the given address
  // |profile|.
  virtual void FillAddressForm(
      const autofill::AutofillProfile* profile,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Fill the card form given by |selector| with the given |card| and its
  // |cvc|.
  virtual void FillCardForm(
      std::unique_ptr<autofill::CreditCard> card,
      const std::u16string& cvc,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Return |FormData| and |FormFieldData| for the element identified with
  // |selector|. The result is returned asynchronously through |callback|.
  virtual void RetrieveElementFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData& form_data,
                              const autofill::FormFieldData& field_data)>
          callback);

  // Select the option to be picked given by the |re2| in the |element|.
  virtual void SelectOption(
      const std::string& re2,
      bool case_sensitive,
      SelectOptionProto::OptionComparisonAttribute option_comparison_attribute,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Highlight an |element|.
  virtual void HighlightElement(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Scrolls |container| to an |element|'s position. |top_padding|
  // specifies the padding between the focused element and the top of the
  // container. If |scrollable_element| is not specified, the window will be
  // scrolled instead.
  virtual void ScrollToElementPosition(
      std::unique_ptr<ElementFinder::Result> container,
      const ElementFinder::Result& element,
      const TopPadding& top_padding,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Get the value attribute of an |element| and return the result through
  // |callback|. If the lookup fails, the value will be empty. An empty result
  // does not mean an error.
  //
  // Normally done through BatchElementChecker.
  virtual void GetFieldValue(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Get the value of a nested |attribute| from an |element| and return the
  // result through |callback|. If the lookup fails, the value will be empty.
  // An empty result does not mean an error.
  virtual void GetStringAttribute(
      const std::vector<std::string>& attributes,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Set the value attribute of an |element| to the specified |value| and
  // trigger an onchange event.
  virtual void SetValueAttribute(
      const std::string& value,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Set the nested |attributes| of an |element| to the specified |value|.
  virtual void SetAttribute(
      const std::vector<std::string>& attributes,
      const std::string& value,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Select the current value in a text |element|.
  virtual void SelectFieldValue(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Focus the current |element|.
  virtual void FocusField(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Inputs the specified codepoints into |element|. Expects the |element| to
  // have focus. Key presses will have a delay of
  // |key_press_delay_in_millisecond| between them. Returns the result through
  // |callback|.
  virtual void SendKeyboardInput(
      const std::vector<UChar32>& codepoints,
      int key_press_delay_in_millisecond,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Inputs the specified |value| into |element| with keystrokes per character.
  // Expects the |element| to have focus. Key presses will have a delay of
  // |key_press_delay_in_millisecond| between them. Returns the result through
  // |callback|.
  virtual void SendTextInput(
      int key_press_delay_in_millisecond,
      const std::string& value,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Sends the specified key event. Expects |element| to have focus.
  virtual void SendKeyEvent(
      const KeyEvent& key_event,
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Return the outerHTML of |element|.
  virtual void GetOuterHtml(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Return the outerHTML of each element in |elements|. |elements| must contain
  // the object ID of a JS array containing the elements.
  virtual void GetOuterHtmls(
      const ElementFinder::Result& elements,
      base::OnceCallback<void(const ClientStatus&,
                              const std::vector<std::string>&)> callback);

  // Return the tag of the |element|. In case of an error, will return an empty
  // string.
  virtual void GetElementTag(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Gets the visual viewport coordinates and size.
  //
  // The rectangle is expressed in absolute CSS coordinates.
  virtual void GetVisualViewport(
      base::OnceCallback<void(const ClientStatus&, const RectF&)> callback);

  // Gets the position of the |element|.
  //
  // If unsuccessful, the callback gets the failure status with an empty rect.
  //
  // If successful, the callback gets a success status with a set of
  // (left, top, right, bottom) coordinates rect, expressed in absolute CSS
  // coordinates.
  virtual void GetElementRect(const ElementFinder::Result& element,
                              ElementRectGetter::ElementRectCallback callback);

  // Calls the callback once the main document window has been resized.
  virtual void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Gets the value of document.readyState for |optional_frame_element| or, if
  // it is empty, in the main document.
  virtual void GetDocumentReadyState(
      const ElementFinder::Result& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback);

  // Waits for the value of Document.readyState to satisfy |min_ready_state| in
  // |optional_frame_element| or, if it is empty, in the main document.
  virtual void WaitForDocumentReadyState(
      const ElementFinder::Result& optional_frame_element,
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState,
                              base::TimeDelta)> callback);

  // Trigger a "change" event on the |element|.
  virtual void SendChangeEvent(
      const ElementFinder::Result& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Dispatch a custom JS event 'duplexweb' with an optional payload.
  virtual void DispatchJsEvent(
      base::OnceCallback<void(const ClientStatus&)> callback) const;

  virtual base::WeakPtr<WebController> GetWeakPtr() const;

 private:
  friend class WebControllerBrowserTest;

  struct FillFormInputData {
    FillFormInputData();
    ~FillFormInputData();

    // Data for filling address form.
    std::unique_ptr<autofill::AutofillProfile> profile;

    // Data for filling card form.
    std::unique_ptr<autofill::CreditCard> card;
    std::u16string cvc;
  };

  // RAII object that sets the action state to "running" when the object is
  // allocated and to "not running" when it gets deallocated.
  class ScopedAssistantActionStateRunning
      : private content::WebContentsObserver {
   public:
    explicit ScopedAssistantActionStateRunning(
        content::WebContents* web_contents,
        content::RenderFrameHost* render_frame_host);
    ~ScopedAssistantActionStateRunning() override;

    ScopedAssistantActionStateRunning(
        const ScopedAssistantActionStateRunning&) = delete;
    ScopedAssistantActionStateRunning& operator=(
        const ScopedAssistantActionStateRunning&) = delete;

   private:
    void SetAssistantActionState(bool running);

    // Overrides content::WebContentsObserver:
    void RenderFrameDeleted(
        content::RenderFrameHost* render_frame_host) override;

    content::RenderFrameHost* render_frame_host_;
  };

  void OnJavaScriptResult(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnJavaScriptResultForString(
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnJavaScriptResultForStringArray(
      base::OnceCallback<void(const ClientStatus&,
                              const std::vector<std::string>&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnCheckOnTop(CheckOnTopWorker* worker,
                    base::OnceCallback<void(const ClientStatus&)> callback,
                    const ClientStatus& status);
  void OnWaitUntilElementIsStable(
      ElementPositionGetter* getter_to_release,
      base::TimeTicks wait_time_start,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
      const ClientStatus& status);
  void TapOrClickOnCoordinates(
      ElementPositionGetter* getter_to_release,
      const std::string& node_frame_id,
      ClickType click_type,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status);
  void OnDispatchPressMouseEvent(
      const std::string& node_frame_id,
      base::OnceCallback<void(const ClientStatus&)> callback,
      int x,
      int y,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchReleaseMouseEvent(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchTouchEventStart(
      const std::string& node_frame_id,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchTouchEventResult> result);
  void OnDispatchTouchEventEnd(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchTouchEventResult> result);
  void OnWaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::EvaluateResult> result);
  void RunElementFinder(const Selector& selector,
                        ElementFinder::ResultType result_type,
                        ElementFinder::Callback callback);
  void OnFindElementResult(ElementFinder* finder_to_release,
                           ElementFinder::Callback callback,
                           const ClientStatus& status,
                           std::unique_ptr<ElementFinder::Result> result);
  void GetElementFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              autofill::ContentAutofillDriver* driver,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback);
  void OnFindElementForGetFormAndFieldData(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&,
                              autofill::ContentAutofillDriver* driver,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback,
      const ClientStatus& element_status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnGetFormAndFieldData(
      base::OnceCallback<void(const ClientStatus&,
                              autofill::ContentAutofillDriver* driver,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback,
      autofill::ContentAutofillDriver* driver,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  void OnGetFormAndFieldDataForFilling(
      std::unique_ptr<FillFormInputData> data_to_autofill,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& form_status,
      autofill::ContentAutofillDriver* driver,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  void OnGetFormAndFieldDataForRetrieving(
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData& form_data,
                              const autofill::FormFieldData& field_data)>
          callback,
      const ClientStatus& form_status,
      autofill::ContentAutofillDriver* driver,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  void OnSelectOption(base::OnceCallback<void(const ClientStatus&)> callback,
                      const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);

  void OnSendKeyboardInputDone(
      SendKeyboardInputWorker* worker_to_release,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status);

  void OnGetElementRect(ElementRectGetter* getter_to_release,
                        ElementRectGetter::ElementRectCallback callback,
                        const ClientStatus& rect_status,
                        const RectF& element_rect);
  void OnGetVisualViewport(
      base::OnceCallback<void(const ClientStatus&, const RectF&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::EvaluateResult> result);

  void OnWaitForDocumentReadyState(
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState,
                              base::TimeDelta)> callback,
      base::TimeTicks wait_start_time,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::EvaluateResult> result);

  void OnDispatchJsEvent(base::OnceCallback<void(const ClientStatus&)> callback,
                         const DevtoolsClient::ReplyStatus& reply_status,
                         std::unique_ptr<runtime::EvaluateResult> result) const;

  // Wrapper for calling the |callback| after re-enabling the keyboard by
  // setting the assistant action state to "not running".
  void RetainAssistantActionRunningStateAndExecuteCallback(
      std::unique_ptr<ScopedAssistantActionStateRunning> scoped_state,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& client_status);
  // Disables the keyboard by setting the assistant action state to "running"
  // and wraps the |callback| such that the keyboard is re-enabled before
  // calling it. Uses the |RenderFrameHost| of the |ElementFinder::Result| to
  // extract the appropriate |ContentAutofillDriver|.
  base::OnceCallback<void(const ClientStatus&)>
  GetAssistantActionRunningStateRetainingCallback(
      const ElementFinder::Result& element_result,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Weak pointer is fine here since it must outlive this web controller, which
  // is guaranteed by the owner of this object.
  content::WebContents* web_contents_;
  std::unique_ptr<DevtoolsClient> devtools_client_;

  // Currently running workers.
  std::vector<std::unique_ptr<WebControllerWorker>> pending_workers_;

  base::WeakPtrFactory<WebController> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebController);
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_H_
