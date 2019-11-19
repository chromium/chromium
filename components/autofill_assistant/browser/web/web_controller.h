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
#include "components/autofill_assistant/browser/actions/click_action.h"
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
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_position_getter.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"
#include "third_party/icu/source/common/unicode/umachine.h"
#include "url/gurl.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace autofill {
struct FormData;
struct FormFieldData;
}  // namespace autofill

namespace autofill_assistant {
struct ClientSettings;

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
      content::WebContents* web_contents,
      const ClientSettings* settings);

  // |web_contents| and |settings| must outlive this web controller.
  WebController(content::WebContents* web_contents,
                std::unique_ptr<DevtoolsClient> devtools_client,
                const ClientSettings* settings);
  virtual ~WebController();

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url);

  // Perform a mouse left button click or a touch tap on the element given by
  // |selector| and return the result through callback.
  virtual void ClickOrTapElement(
      const Selector& selector,
      ClickAction::ClickType click_type,
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
      const base::string16& cvc,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Select the option given by |selector| and the value of the option to be
  // picked.
  virtual void SelectOption(
      const Selector& selector,
      const std::string& selected_option,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Highlight an element given by |selector|.
  virtual void HighlightElement(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Focus on element given by |selector|. |top_padding| specifies the padding
  // between focused element and the top.
  virtual void FocusElement(
      const Selector& selector,
      const TopPadding& top_padding,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Get the value of |selector| and return the result through |callback|. The
  // returned value might be false, if the element cannot be found, true and the
  // empty string in case of error or empty value.
  //
  // Normally done through BatchElementChecker.
  virtual void GetFieldValue(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Set the |value| of field |selector| and return the result through
  // |callback|. If |simulate_key_presses| is true, the value will be set by
  // clicking the field and then simulating key presses, otherwise the `value`
  // attribute will be set directly.
  virtual void SetFieldValue(
      const Selector& selector,
      const std::string& value,
      bool simulate_key_presses,
      int key_press_delay_in_millisecond,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Set the |value| of the |attribute| of the element given by |selector|.
  virtual void SetAttribute(
      const Selector& selector,
      const std::vector<std::string>& attribute,
      const std::string& value,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Sets the keyboard focus to |selector| and inputs |codepoints|, one
  // character at a time. Key presses will have a delay of |delay_in_milli|
  // between them.
  // Returns the result through |callback|.
  virtual void SendKeyboardInput(
      const Selector& selector,
      const std::vector<UChar32>& codepoints,
      int delay_in_milli,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Return the outerHTML of |selector|.
  virtual void GetOuterHtml(
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback);

  // Gets the visual viewport coordinates and size.
  //
  // The rectangle is expressed in absolute CSS coordinates.
  virtual void GetVisualViewport(
      base::OnceCallback<void(bool, const RectF&)> callback);

  // Gets the position of the element identified by the selector.
  //
  // If unsuccessful, the callback gets (false, 0, 0, 0, 0).
  //
  // If successful, the callback gets (true, left, top, right, bottom), with
  // coordinates expressed in absolute CSS coordinates.
  virtual void GetElementPosition(
      const Selector& selector,
      base::OnceCallback<void(bool, const RectF&)> callback);

  // Checks whether an element matches the given selector.
  //
  // If strict, there must be exactly one matching element for the check to
  // pass. Otherwise, there must be at least one.
  //
  // To check multiple elements, use a BatchElementChecker.
  virtual void ElementCheck(
      const Selector& selector,
      bool strict,
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Calls the callback once the main document window has been resized.
  virtual void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback);

  // Gets the value of document.readyState for |optional_frame| or, if it is
  // empty, in the main document.
  virtual void GetDocumentReadyState(
      const Selector& optional_frame,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState end_state)> callback);

  // Waits for the value of Document.readyState to satisfy |min_ready_state| in
  // |optional_frame| or, if it is empty, in the main document.
  virtual void WaitForDocumentReadyState(
      const Selector& optional_frame,
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&,
                              DocumentReadyState end_state)> callback);

 private:
  friend class WebControllerBrowserTest;

  struct FillFormInputData {
    FillFormInputData();
    ~FillFormInputData();

    // Data for filling address form.
    std::unique_ptr<autofill::AutofillProfile> profile;

    // Data for filling card form.
    std::unique_ptr<autofill::CreditCard> card;
    base::string16 cvc;
  };

  void OnFindElementForClickOrTap(
      base::OnceCallback<void(const ClientStatus&)> callback,
      ClickAction::ClickType click_type,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> result);
  void OnWaitDocumentToBecomeInteractiveForClickOrTap(
      base::OnceCallback<void(const ClientStatus&)> callback,
      ClickAction::ClickType click_type,
      std::unique_ptr<ElementFinder::Result> target_element,
      bool result);
  void OnFindElementForTap(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> result);
  void ClickOrTapElement(
      std::unique_ptr<ElementFinder::Result> target_element,
      ClickAction::ClickType click_type,
      base::OnceCallback<void(const ClientStatus&)> callback);
  void OnClickJS(base::OnceCallback<void(const ClientStatus&)> callback,
                 const DevtoolsClient::ReplyStatus& reply_status,
                 std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnScrollIntoView(std::unique_ptr<ElementFinder::Result> target_element,
                        base::OnceCallback<void(const ClientStatus&)> callback,
                        ClickAction::ClickType click_type,
                        const DevtoolsClient::ReplyStatus& reply_status,
                        std::unique_ptr<runtime::CallFunctionOnResult> result);
  void TapOrClickOnCoordinates(
      ElementPositionGetter* getter_to_release,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const std::string& node_frame_id,
      ClickAction::ClickType click_type,
      bool has_coordinates,
      int x,
      int y);
  void OnDispatchPressMouseEvent(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const std::string& node_frame_id,
      int x,
      int y,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchReleaseMouseEvent(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchTouchEventStart(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const std::string& node_frame_id,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchTouchEventResult> result);
  void OnDispatchTouchEventEnd(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchTouchEventResult> result);
  void OnFindElementForCheck(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> result);
  void OnWaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::EvaluateResult> result);

  // Find the element given by |selector|. If multiple elements match
  // |selector| and if |strict_mode| is false, return the first one that is
  // found. Otherwise if |strict-mode| is true, do not return any.
  void FindElement(const Selector& selector,
                   bool strict_mode,
                   ElementFinder::Callback callback);
  void OnFindElementResult(ElementFinder* finder_to_release,
                           ElementFinder::Callback callback,
                           const ClientStatus& status,
                           std::unique_ptr<ElementFinder::Result> result);
  void OnFindElementForFillingForm(
      std::unique_ptr<FillFormInputData> data_to_autofill,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnGetFormAndFieldDataForFillingForm(
      std::unique_ptr<FillFormInputData> data_to_autofill,
      base::OnceCallback<void(const ClientStatus&)> callback,
      content::RenderFrameHost* container_frame_host,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& form_field);
  void OnFindElementForFocusElement(
      const TopPadding& top_padding,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnWaitDocumentToBecomeInteractiveForFocusElement(
      const TopPadding& top_padding,
      base::OnceCallback<void(const ClientStatus&)> callback,
      std::unique_ptr<ElementFinder::Result> target_element,
      bool result);
  void OnFocusElement(base::OnceCallback<void(const ClientStatus&)> callback,
                      const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForSelectOption(
      const std::string& selected_option,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnSelectOption(base::OnceCallback<void(const ClientStatus&)> callback,
                      const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForHighlightElement(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnHighlightElement(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForGetFieldValue(
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnGetValueAttribute(
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void InternalSetFieldValue(
      const Selector& selector,
      const std::string& value,
      base::OnceCallback<void(const ClientStatus&)> callback);
  void OnClearFieldForSendKeyboardInput(
      const Selector& selector,
      const std::vector<UChar32>& codepoints,
      int key_press_delay_in_millisecond,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status);
  void OnClickElementForSendKeyboardInput(
      const std::string& node_frame_id,
      const std::vector<UChar32>& codepoints,
      int delay_in_milli,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& click_status);
  void DispatchKeyboardTextDownEvent(
      const std::string& node_frame_id,
      const std::vector<UChar32>& codepoints,
      size_t index,
      bool delay,
      int delay_in_milli,
      base::OnceCallback<void(const ClientStatus&)> callback);
  void DispatchKeyboardTextUpEvent(
      const std::string& node_frame_id,
      const std::vector<UChar32>& codepoints,
      size_t index,
      int delay_in_milli,
      base::OnceCallback<void(const ClientStatus&)> callback);
  void OnFindElementForSetAttribute(
      const std::vector<std::string>& attribute,
      const std::string& value,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnSetAttribute(base::OnceCallback<void(const ClientStatus&)> callback,
                      const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForSendKeyboardInput(
      const Selector& selector,
      const std::vector<UChar32>& codepoints,
      int delay_in_milli,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnFindElementForSetFieldValue(
      const std::string& value,
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnSetValueAttribute(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForGetOuterHtml(
      base::OnceCallback<void(const ClientStatus&, const std::string&)>
          callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element_result);
  void OnGetOuterHtml(base::OnceCallback<void(const ClientStatus&,
                                              const std::string&)> callback,
                      const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForPosition(
      base::OnceCallback<void(bool, const RectF&)> callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> result);
  void OnGetVisualViewport(
      base::OnceCallback<void(bool, const RectF&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::EvaluateResult> result);
  void OnGetElementPositionResult(
      base::OnceCallback<void(bool, const RectF&)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  // Creates a new instance of DispatchKeyEventParams for the specified type and
  // unicode codepoint.
  using DispatchKeyEventParamsPtr =
      std::unique_ptr<autofill_assistant::input::DispatchKeyEventParams>;
  static DispatchKeyEventParamsPtr CreateKeyEventParamsForCharacter(
      autofill_assistant::input::DispatchKeyEventType type,
      const UChar32 codepoint);

  // Waits for the document.readyState to be 'interactive' or 'complete'.
  void WaitForDocumentToBecomeInteractive(
      int remaining_rounds,
      const std::string& object_id,
      const std::string& node_frame_id,
      base::OnceCallback<void(bool)> callback);
  void OnWaitForDocumentToBecomeInteractive(
      int remaining_rounds,
      const std::string& object_id,
      const std::string& node_frame_id,
      base::OnceCallback<void(bool)> callback,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnFindElementForWaitForDocumentReadyState(
      DocumentReadyState min_ready_state,
      base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
          callback,
      const ClientStatus& status,
      std::unique_ptr<ElementFinder::Result> element);

  // Weak pointer is fine here since it must outlive this web controller, which
  // is guaranteed by the owner of this object.
  content::WebContents* web_contents_;
  std::unique_ptr<DevtoolsClient> devtools_client_;
  const ClientSettings* const settings_;

  // Currently running workers.
  std::vector<std::unique_ptr<WebControllerWorker>> pending_workers_;

  base::WeakPtrFactory<WebController> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebController);
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_H_
