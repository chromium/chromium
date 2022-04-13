// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_input.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_network.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/check_on_top_worker.h"
#include "components/autofill_assistant/browser/web/click_or_tap_worker.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_position_getter.h"
#include "components/autofill_assistant/browser/web/element_rect_getter.h"
#include "components/autofill_assistant/browser/web/selector_observer.h"
#include "components/autofill_assistant/browser/web/send_keyboard_input_worker.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/icu/source/common/unicode/umachine.h"
#include "url/gurl.h"

namespace autofill {
class AutofillableData;
class AutofillDataModel;
class AutofillProfile;
class ContentAutofillDriver;
class CreditCard;
struct FormData;
struct FormFieldData;
}  // namespace autofill

namespace content {
class WebContents;
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
  // Create web controller for a given |web_contents|. |user_data|, |log_info|
  // and |annotate_dom_model_service| (if not nullptr) must be valid
  // for the lifetime of the controller. |enable_full_stack_traces| should only
  // be enabled if the thrown exceptions will be caught and handled, otherwise
  // this will unnecessarily decrease performance.
  static std::unique_ptr<WebController> CreateForWebContents(
      content::WebContents* web_contents,
      const UserData* user_data,
      ProcessedActionStatusDetailsProto* log_info,
      AnnotateDomModelService* annotate_dom_model_service,
      bool enable_full_stack_traces);

  // |web_contents|, |user_data|, |log_info| and |annotate_dom_model_service|
  // (if not nullptr) must outlive this web controller.
  WebController(content::WebContents* web_contents,
                std::unique_ptr<DevtoolsClient> devtools_client,
                const UserData* user_data,
                ProcessedActionStatusDetailsProto* log_info,
                AnnotateDomModelService* annotate_dom_model_service);

  WebController(const WebController&) = delete;
  WebController& operator=(const WebController&) = delete;

  virtual ~WebController();

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url);

  // Find the element given by |selector|. If multiple elements match
  // |selector| and if |strict_mode| is false, return the first one that is
  // found. Otherwise if |strict_mode| is true, do not return any.
  //
  // To check multiple elements, use a BatchElementChecker.
  virtual void FindElement(const Selector& selector,
                           bool strict_mode,
                           ElementFinder::Callback callback);

  // Find the element given by |selector| starting from the given
  // |start_element|. Returns results or errors based on the |result_type|.
  virtual void RunElementFinder(const ElementFinderResult& start_element,
                                const Selector& selector,
                                ElementFinder::ResultType result_type,
                                ElementFinder::Callback callback);

  // Find all elements matching |selector|. If there are no matches, the status
  // will be ELEMENT_RESOLUTION_FAILED.
  virtual void FindAllElements(const Selector& selector,
                               ElementFinder::Callback callback);

  virtual ClientStatus ObserveSelectors(
      const std::vector<SelectorObserver::ObservableSelector>& selectors,
      base::TimeDelta timeout_ms,
      base::TimeDelta periodic_check_interval,
      base::TimeDelta extra_timeout,
      SelectorObserver::Callback callback);

  // Scroll the |element| into view. |animation| defines the transition
  // animation, |vertical_alignment| defines the vertical alignment,
  // |horizontal_alignment| defines the horizontal alignment.
  virtual void ScrollIntoView(
      const std::string& animation,
      const std::string& vertical_alignment,
      const std::string& horizontal_alignment,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Scroll the |element| into view only if needed. |center| the element if
  // requested.
  virtual void ScrollIntoViewIfNeeded(
      bool center,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Scroll the window by |scroll_distance|. |animation| defines the transition
  // animation. Specify |optional_frame| if an iFrame instead of the main
  // window should be scrolled.
  virtual void ScrollWindow(
      const ScrollDistance& scroll_distance,
      const std::string& animation,
      const ElementFinderResult& optional_frame,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Scroll the |element| by |scroll_distance|. |animation| defines the
  // transition animation. Specify |optional_frame| if an iFrame instead of the
  // main window should be scrolled.
  virtual void ScrollContainer(
      const ScrollDistance& scroll_distance,
      const std::string& animation,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Send a JS click to the |element|.
  virtual void JsClickElement(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Perform a mouse left button click or a touch tap on the |element|
  // return the result through callback.
  virtual void ClickOrTapElement(
      ClickType click_type,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Get a stable position of the given element. Fail with ELEMENT_UNSTABLE if
  // the element position doesn't stabilize quickly enough.
  virtual void WaitUntilElementIsStable(
      int max_rounds,
      base::TimeDelta check_interval,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback);

  // Check whether the center given element is on top. Fail with
  // ELEMENT_NOT_ON_TOP if the center of the element is covered.
  virtual void CheckOnTop(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Fill the address form given by |element| with the given address
  // |profile|.
  virtual void FillAddressForm(
      std::unique_ptr<autofill::AutofillProfile> profile,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Fill the card form given by |element| with the given |card| and its
  // |cvc|.
  virtual void FillCardForm(
      std::unique_ptr<autofill::CreditCard> card,
      const std::u16string& cvc,
      const ElementFinderResult& element,
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
      bool strict,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Set the selected |option| of the |element|.
  virtual void SelectOptionElement(
      const ElementFinderResult& option,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Check if the selected option of the |element| is the expected |option|.
  virtual void CheckSelectedOptionElement(
      const ElementFinderResult& option,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Scrolls |container| to an |element|'s position. |top_padding|
  // specifies the padding between the focused element and the top of the
  // container. If |scrollable_element| is not specified, the window will be
  // scrolled instead.
  virtual void ScrollToElementPosition(
      std::unique_ptr<ElementFinderResult> container,
      const TopPadding& top_padding,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Get the value attribute of an |element| and return the result through
  // |callback|. If the lookup fails, the value will be empty. An empty result
  // does not mean an error.
  //
  // Normally done through BatchElementChecker.
  virtual void GetFieldValue(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Get the value of a nested |attribute| from an |element| and return the
  // result through |callback|. If the lookup fails, the value will be empty.
  // An empty result does not mean an error.
  virtual void GetStringAttribute(
      const std::vector<std::string>& attributes,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Set the value attribute of an |element| to the specified |value| and
  // trigger an onchange event.
  virtual void SetValueAttribute(
      const std::string& value,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Set the nested |attributes| of an |element| to the specified |value|.
  virtual void SetAttribute(
      const std::vector<std::string>& attributes,
      const std::string& value,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Select the current value in a text |element|.
  virtual void SelectFieldValue(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Focus the current |element|.
  virtual void FocusField(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Blur the current |element| that might have focus to remove its focus.
  virtual void BlurField(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Inputs the specified codepoints into |element|. Expects the |element| to
  // have focus. Key presses will have a delay of
  // |key_press_delay_in_millisecond| between them. Returns the result through
  // |callback|.
  virtual void SendKeyboardInput(
      const std::vector<UChar32>& codepoints,
      int key_press_delay_in_millisecond,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Inputs the specified |value| into |element| with keystrokes per character.
  // Expects the |element| to have focus. Key presses will have a delay of
  // |key_press_delay_in_millisecond| between them. Returns the result through
  // |callback|.
  virtual void SendTextInput(
      int key_press_delay_in_millisecond,
      const std::string& value,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Sends the specified key event. Expects |element| to have focus.
  virtual void SendKeyEvent(
      const KeyEvent& key_event,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Return the outerHTML of |element|.
  virtual void GetOuterHtml(
      bool include_all_inner_text,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Return the outerHTML of each element in |elements|. |elements| must contain
  // the object ID of a JS array containing the elements.
  virtual void GetOuterHtmls(
      bool include_all_inner_text,
      const ElementFinderResult& elements,
      base::OnceCallback<void(const ClientStatus&,
                              const std::vector<std::string>&)> callback);

  // Return the tag of the |element|. In case of an error, will return an empty
  // string.
  virtual void GetElementTag(
      const ElementFinderResult& element,
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
  virtual void GetElementRect(const ElementFinderResult& element,
                              ElementRectGetter::ElementRectCallback callback);

  // Calls the callback once the main document window has been resized.
  virtual void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Gets the value of document.readyState for |optional_frame_element| or, if
  // it is empty, in the main document.
  virtual void GetDocumentReadyState(
      const ElementFinderResult& optional_frame_element,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback);

  // Waits for the value of Document.readyState to satisfy |min_ready_state| in
  // |optional_frame_element| or, if it is empty, in the main document.
  virtual void WaitForDocumentReadyState(
      const ElementFinderResult& optional_frame_element,
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState,
                              base::TimeDelta)> callback);

  // Trigger a "change" event on the |element|.
  virtual void SendChangeEvent(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Dispatch a custom JS event 'duplexweb'.
  virtual void DispatchJsEvent(
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Execute an arbitrary JS snippet on the |element|. `this` in the snippet
  // will refer to the |element|.
  virtual void ExecuteJS(
      const std::string& js_snippet,
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&)> callback);

  virtual base::WeakPtr<WebController> GetWeakPtr() const;

 private:
  friend class WebControllerBrowserTest;
  friend class BatchElementCheckerBrowserTest;

  void OnJavaScriptResult(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnJavaScriptResultForInt(
      base::OnceCallback<void(const ClientStatus&, int)> callback,
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
  void ExecuteJsWithoutArguments(
      const ElementFinderResult& element,
      const std::string& js_snippet,
      WebControllerErrorInfoProto::WebAction web_action,
      base::OnceCallback<void(const ClientStatus&)> callback);
  void OnExecuteJsWithoutArguments(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnScrollWindow(base::OnceCallback<void(const ClientStatus&)> callback,
                      const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<runtime::EvaluateResult> result);
  void OnCheckOnTop(CheckOnTopWorker* worker,
                    base::OnceCallback<void(const ClientStatus&)> callback,
                    const ClientStatus& status);
  void OnWaitUntilElementIsStable(
      ElementPositionGetter* getter_to_release,
      base::TimeTicks wait_time_start,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
      const ClientStatus& status);
  void OnClickOrTapElement(
      ClickOrTapWorker* getter_to_release,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status);
  void OnWaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::EvaluateResult> result);
  void OnFindElementResult(ElementFinder* finder_to_release,
                           ElementFinder::Callback callback,
                           const ClientStatus& status,
                           std::unique_ptr<ElementFinderResult> result);

  void OnSelectorObserverFinished(SelectorObserver* observer);
  void OnFindElementForRetrieveElementFormAndFieldData(
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData& form_data,
                              const autofill::FormFieldData& field_data)>
          callback,
      const ClientStatus& element_status,
      std::unique_ptr<ElementFinderResult> element_result);
  void GetElementFormAndFieldData(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&,
                              autofill::ContentAutofillDriver* driver,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback);
  void GetBackendNodeId(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&, int)> callback);
  void OnGetBackendNodeId(
      base::OnceCallback<void(const ClientStatus&, int)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnGetBackendNodeIdForFormAndFieldData(
      const ElementFinderResult& element,
      base::OnceCallback<void(const ClientStatus&,
                              autofill::ContentAutofillDriver* driver,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback,
      const ClientStatus& node_status,
      int backend_node_id);
  void OnGetFormAndFieldData(
      base::OnceCallback<void(const ClientStatus&,
                              autofill::ContentAutofillDriver* driver,
                              const autofill::FormData&,
                              const autofill::FormFieldData&)> callback,
      autofill::ContentAutofillDriver* driver,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  // Use |retain_data| to retain the source data until the form filling has
  // been performed.
  void OnGetFormAndFieldDataForFilling(
      const autofill::AutofillableData& data_to_autofill,
      std::unique_ptr<autofill::AutofillDataModel> retain_data,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& form_status,
      autofill::ContentAutofillDriver* driver,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  void OnGetFormAndFieldDataForRetrieving(
      std::unique_ptr<ElementFinderResult> element,
      base::OnceCallback<void(const ClientStatus&,
                              const autofill::FormData& form_data,
                              const autofill::FormFieldData& field_data)>
          callback,
      const ClientStatus& form_status,
      autofill::ContentAutofillDriver* driver,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  // Handling a JS result for a "SelectOption" action. This expects the JS
  // result to contain an integer mapped to a ClientStatus.
  void OnSelectOptionJavascriptResult(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void SendKeyEvents(WebControllerErrorInfoProto::WebAction web_action,
                     const std::vector<KeyEvent>& key_events,
                     int key_press_delay,
                     const ElementFinderResult& element,
                     base::OnceCallback<void(const ClientStatus&)> callback);
  void OnSendKeyboardInputDone(
      SendKeyboardInputWorker* worker_to_release,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status);
  void OnGetVisualViewport(
      base::OnceCallback<void(const ClientStatus&, const RectF&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::EvaluateResult> result);
  void OnGetElementRect(ElementRectGetter* getter_to_release,
                        ElementRectGetter::ElementRectCallback callback,
                        const ClientStatus& rect_status,
                        const RectF& element_rect);
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

  // Weak pointer is fine here since it must outlive this web controller, which
  // is guaranteed by the owner of this object.
  const raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<DevtoolsClient> devtools_client_;
  // Must not be |nullptr| and outlive this web controller.
  const raw_ptr<const UserData> user_data_;
  const raw_ptr<ProcessedActionStatusDetailsProto> log_info_;
  // Can be |nullptr|, if not must outlive this web controller.
  const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;

  // Currently running workers.
  std::vector<std::unique_ptr<WebControllerWorker>> pending_workers_;

  base::WeakPtrFactory<WebController> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_H_
