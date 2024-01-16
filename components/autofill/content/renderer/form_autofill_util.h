// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_UTIL_H_

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/i18n/rtl.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "ui/gfx/geometry/rect_f.h"

class GURL;

namespace blink {
enum class WebAutofillState;

class WebDocument;
class WebElement;
class WebFormControlElement;
class WebFormElement;
class WebInputElement;
class WebLocalFrame;
class WebNode;
class WebString;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace autofill {

struct FormData;
struct FormFieldData;

class FieldDataManager;

namespace form_util {

// Mapping from a form element's render id to results of button titles
// heuristics for a given form element.
using ButtonTitlesCache = base::flat_map<FormRendererId, ButtonTitleList>;

// A bit field mask to extract data from WebFormControlElement.
// Copied to components/autofill/ios/browser/resources/autofill_controller.js.
enum class ExtractOption {
  kValue,       // Extract value from WebFormControlElement.
  kOptionText,  // Extract option text from WebFormSelectElement. Only
                // valid when kValue is set. This is used for form submission
                // where human readable value is captured.
  kOptions,     // Extract options from WebFormControlElement.
  kBounds,      // Extract bounds from WebFormControlElement, could
                // trigger layout if needed.
  kDatalist,    // Extract datalist from WebFormControlElement, the total
                // number of options is up to kMaxListSize and each option
                // has as far as kMaxDataLength.
  kMinValue = kValue,
  kMaxValue = kDatalist,
};

// Returns the topmost <form> ancestor of |node|, or an IsNull() pointer.
//
// Generally, WebFormElements must not be nested [1]. When parsing HTML, Blink
// ignores nested form tags; the inner forms therefore never make it into the
// DOM. However, nested forms can be created and added to the DOM dynamically,
// in which case Blink associates each field with its closest ancestor.
//
// For some elements, Autofill determines the associated form without Blink's
// help (currently, these are only iframe elements). For consistency with
// Blink's behaviour, we associate them with their closest form element
// ancestor.
//
// [1] https://html.spec.whatwg.org/multipage/forms.html#the-form-element
blink::WebFormElement GetClosestAncestorFormElement(blink::WebNode node);

// Returns true if a DOM traversal (pre-order, depth-first) visits `x` before
// `y`.
// As a performance improvement, `ancestor_hint` can be set to a suspected
// ancestor of `x` and `y`. Otherwise, `ancestor_hint` can be arbitrary.
//
// This function is a simplified/specialized version of Blink's private
// Node::compareDocumentPosition().
//
// Exposed for testing purposes.
bool IsDOMPredecessor(const blink::WebNode& x,
                      const blink::WebNode& y,
                      const blink::WebNode& ancestor_hint);

// Gets up to kMaxListSize data list values (with corresponding label) for the
// given element, each value and label have as far as kMaxDataLength.
void GetDataListSuggestions(const blink::WebInputElement& element,
                            std::vector<SelectOption>* options);

// Extract FormData from `form_element` or the unowned form if
// `form_element.IsNull()`.
std::optional<FormData> ExtractFormData(
    const blink::WebDocument& document,
    const blink::WebFormElement& form_element,
    const FieldDataManager& field_data_manager,
    DenseSet<ExtractOption> extract_options = {ExtractOption::kValue,
                                               ExtractOption::kOptionText,
                                               ExtractOption::kOptions});

// Helper functions to assist in getting the canonical form of the action and
// origin. The action will properly take into account <BASE>, and both will
// strip unnecessary data (e.g. query params and HTTP credentials).
GURL GetCanonicalActionForForm(const blink::WebFormElement& form);
GURL GetDocumentUrlWithoutAuth(const blink::WebDocument& document);

// Returns true if |element| is a month input element.
bool IsMonthInput(const blink::WebInputElement& element);

// Returns true if |element| is a month input element.
bool IsMonthInput(const blink::WebFormControlElement& element);

// Returns true if |element| is a text input element.
bool IsTextInput(const blink::WebInputElement& element);

// Returns true if |element| is a text input element.
bool IsTextInput(const blink::WebFormControlElement& element);

// Returns true if `element` is either a select or a selectlist element.
bool IsSelectOrSelectListElement(const blink::WebFormControlElement& element);

// Returns true if |element| is a select element.
bool IsSelectElement(const blink::WebFormControlElement& element);

// Returns true if `element` is a selectlist element.
bool IsSelectListElement(const blink::WebFormControlElement& element);

// Returns true if |element| is a textarea element.
bool IsTextAreaElement(const blink::WebFormControlElement& element);

// Returns true if `element` is a textarea element or a text input element.
bool IsTextAreaElementOrTextInput(const blink::WebFormControlElement& element);

// Returns true if |element| is a checkbox or a radio button element.
bool IsCheckableElement(const blink::WebFormControlElement& element);

// Returns true if |element| is one of the input element types that can be
// autofilled. {Text, Radiobutton, Checkbox}.
// TODO(crbug.com/1007974): IsAutofillableInputElement() are currently used
// inconsistently. Investigate where these checks are necessary.
bool IsAutofillableInputElement(const blink::WebInputElement& element);

// Returns true if |element| is one of the element types that can be autofilled.
// {Text, Radiobutton, Checkbox, Select, TextArea}.
// TODO(crbug.com/1007974): IsAutofillableElement() are currently used
// inconsistently. Investigate where these checks are necessary.
bool IsAutofillableElement(const blink::WebFormControlElement& element);

FormControlType ToAutofillFormControlType(blink::mojom::FormControlType type);
bool IsCheckable(FormControlType form_control_type);

// Returns true iff `element` has a "webauthn" autocomplete attribute.
bool IsWebauthnTaggedElement(const blink::WebFormControlElement& element);

// Returns true if |element| can be edited (enabled and not read only).
bool IsElementEditable(const blink::WebInputElement& element);

// True if this element can take focus. If this element is a selectlist, checks
// whether a child of the selectlist can take focus.
bool IsWebElementFocusableForAutofill(const blink::WebElement& element);

// A heuristic visibility detection. See crbug.com/1335257 for an overview of
// relevant aspects.
//
// Note that WebElement::BoundsInWidget(), WebElement::GetClientSize(),
// and WebElement::GetScrollSize() include the padding but do not include the
// border and margin. BoundsInWidget() additionally scales the
// dimensions according to the zoom factor.
//
// It seems that invisible fields on websites typically have dimensions between
// 0 and 10 pixels, before the zoom factor. Therefore choosing `kMinPixelSize`
// is easier without including the zoom factor. For that reason, this function
// prefers GetClientSize() over BoundsInWidget().
//
// This function does not check the position in the viewport because fields in
// iframes commonly are visible despite the body having height zero. Therefore,
// `e.GetDocument().Body().BoundsInWidget().Intersects(
//      e.BoundsInWidget())` yields false negatives.
//
// Exposed for testing purposes.
//
// TODO(crbug.com/1335257): Can input fields or iframes actually overflow?
bool IsWebElementVisible(const blink::WebElement& element);

// Returns the maximum length value that Autofill may fill into the field. There
// are two special cases:
// - It is 0 for fields that do not support free text input (e.g., <select> and
//   <input type=month>).
// - It is the maximum 32 bit number for fields that support text values (e.g.,
//   <input type=text> or <textarea>) but have no maxlength attribute set.
//   The choice of 32 (as opposed to 64) is intentional: it allows us to still
//   do arithmetic with FormFieldData::max_length without having to worry about
//   integer overflows everywhere.
uint64_t GetMaxLength(const blink::WebFormControlElement& element);

// Returns the form's |name| attribute if non-empty; otherwise the form's |id|
// attribute.
std::u16string GetFormIdentifier(const blink::WebFormElement& form);

// Returns the FormRendererId of a given WebFormElement or contenteditable. If
// WebFormElement::IsNull(), returns a null form renderer id, which is the
// renderer id of the unowned form.
FormRendererId GetFormRendererId(const blink::WebElement& e);

// Returns the FieldRendererId of a given WebFormControlElement or
// contenteditable.
FieldRendererId GetFieldRendererId(const blink::WebElement& e);

// Returns text alignment for |element|.
base::i18n::TextDirection GetTextDirectionForElement(
    const blink::WebFormControlElement& element);

// Returns all the form control elements
// - owned by `form_element` if `!form_element.IsNull()`;
// - owned by no form otherwise.
std::vector<blink::WebFormControlElement> GetFormControlElements(
    const blink::WebDocument& document,
    const blink::WebFormElement& form_element);

// Returns all the autofillable form control elements
// - owned by `form_element` if `!form_element.IsNull()`;
// - owned by no form otherwise.
std::vector<blink::WebFormControlElement> GetAutofillableFormControlElements(
    const blink::WebDocument& document,
    const blink::WebFormElement& form_element);

struct ShadowFieldData;

// Fills out a FormField object from a given autofillable WebFormControlElement.
// |extract_options|: See the enum ExtractOption above for details. Field
// properties will be copied from |field_data_manager|, if the argument is not
// null and has entry for |element| (see properties in FieldPropertiesFlags).
void WebFormControlElementToFormField(
    const blink::WebFormElement& form_element,
    const blink::WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    DenseSet<ExtractOption> extract_options,
    FormFieldData* field,
    ShadowFieldData* shadow_data = nullptr);

// Returns the form that owns the `form_control`, or a null pointer if no form
// owns the `form_control`. exists.
//
// The form that owns `form_control` is
// - the form with which `form_control` is associated, if such a form exists,
// - the closest shadow-including ancestor WebFormElement.
blink::WebFormElement GetOwningForm(
    const blink::WebFormControlElement& form_control);

// Returns a list of elements whose id matches one of the ids found in
// `id_list`.
std::vector<blink::WebElement> GetWebElementsFromIdList(
    const blink::WebDocument& document,
    const blink::WebString& id_list);

// Finds the field that represents `element`, and the form that contains
// `element` and returns them. |extract_options| control what to extract beside
// the default options which is {ExtractOption::kValue,
// ExtractOption::kOptions}. Returns nullopt if the form is not found or cannot
// be serialized.
std::optional<std::pair<FormData, FormFieldData>>
FindFormAndFieldForFormControlElement(
    const blink::WebFormControlElement& element,
    const FieldDataManager& field_data_manager,
    DenseSet<ExtractOption> extract_options);

// Creates a FormData containing a single field out of a contenteditable
// non-form element. The FormData is synthetic in the sense that it does not
// correspond to any other DOM element. It is also conceptually distinct from
// the unowned form (i.e., the collection of form control elements that aren't
// owned by any form).
//
// `kAutofillUseDomNodeIdForRendererId` must be enabled.
//
// Returns `std::nullopt` if `contenteditable`:
// - is a WebFormElement; otherwise, there could be two FormData objects with
//   identical renderer ID referring to different conceptual forms: the one for
//   the contenteditable and an actual <form>.
// - is a WebFormControlElement; otherwise, a <textarea contenteditable> might
//   be a member of two FormData objects: the one for the contenteditable and
//   the <textarea>'s associated <form>'s FormData.
// - has a contenteditable parent; this is to disambiguate focus elements on
//   nested contenteditables because the focus event propagates up.
//
// The FormData's renderer ID has the same value as its (single) FormFieldData's
// renderer ID. This is collision-free with the renderer IDs of any other form
// in the document because DomNodeIds are unique among all DOM elements.
std::optional<FormData> FindFormForContentEditable(
    const blink::WebElement& content_editable);

// Fills or previews the fields represented by `fields`.
// `initiating_element` is the element that initiated the autofill process.
// Returns a list of pairs of the filled elements and their autofill state
// prior to the filling.
std::vector<std::pair<FieldRef, blink::WebAutofillState>> ApplyFormAction(
    base::span<const FormFieldData> fields,
    const blink::WebFormControlElement& initiating_element,
    mojom::ActionType action_type,
    mojom::ActionPersistence action_persistence,
    FieldDataManager& field_data_manager);

// Clears the suggested values in `previewed_elements`.
// `initiating_element` is the element that initiated the preview operation.
// `old_autofill_state` is the previous state of the field that initiated the
// preview.
void ClearPreviewedElements(
    mojom::ActionType action_type,
    base::span<std::pair<blink::WebFormControlElement, blink::WebAutofillState>>
        previewed_elements,
    const blink::WebFormControlElement& initiating_element);

// Indicates if |node| is owned by |frame| in the sense of
// https://dom.spec.whatwg.org/#concept-node-document. Note that being owned by
// a frame does not require being attached to its DOM.
bool IsOwnedByFrame(const blink::WebNode& node, content::RenderFrame* frame);

// Returns true if `node` is currently owned by `frame` or its frame is nullptr,
// in which case the frame is not known anymore. It is a weaker condition than
// `IsOwnedByFrame(node, frame)`.
bool MaybeWasOwnedByFrame(const blink::WebNode& node,
                          content::RenderFrame* frame);

// Checks if the webpage is empty.
// This kind of webpage is considered as empty:
// <html>
//    <head>
//    </head>
//    <body>
//    </body>
// </html>
// Meta, script and title tags don't influence the emptiness of a webpage.
bool IsWebpageEmpty(const blink::WebLocalFrame* frame);

// This function checks whether the children of |element|
// are of the type <script>, <meta>, or <title>.
bool IsWebElementEmpty(const blink::WebElement& element);

// Previews |suggestion| in |input_element| and highlights the suffix of
// |suggestion| not included in the |input_element| text. |input_element| must
// not be null. |user_input| should be the text typed by the user into
// |input_element|. Note that |user_input| cannot be easily derived from
// |input_element| by calling value(), because of http://crbug.com/507714.
void PreviewSuggestion(const std::u16string& suggestion,
                       const std::u16string& user_input,
                       blink::WebFormControlElement* input_element);

// Returns the aggregated values of the descendants of |element| that are
// non-empty text nodes.  This is a faster alternative to |innerText()| for
// performance critical operations.  It does a full depth-first search so can be
// used when the structure is not directly known.  However, unlike with
// |innerText()|, the search depth and breadth are limited to a fixed threshold.
// Whitespace is trimmed from text accumulated at descendant nodes.
std::u16string FindChildText(const blink::WebNode& node);

// Returns the button titles for |web_form|. |button_titles_cache| can be used
// to spare recomputation if called multiple times for the same form.
ButtonTitleList GetButtonTitles(const blink::WebFormElement& web_form,
                                ButtonTitlesCache* button_titles_cache);

// Same as FindChildText() below, but with a list of div nodes to skip.
std::u16string FindChildTextWithIgnoreList(
    const blink::WebNode& node,
    const std::set<blink::WebNode>& divs_to_skip);

// Infers corresponding label for `element` from surrounding context in the DOM,
// e.g. the contents of the preceding <p> tag or text element. Returns an empty
// string if it could not find a label for `element`.
std::u16string InferLabelForElement(const blink::WebFormControlElement& element,
                                    FormFieldData::LabelSource& label_source);

// Returns the form element by unique renderer id. Returns the null element if
// there is no form with the |form_renderer_id|.
blink::WebFormElement FindFormByRendererId(const blink::WebDocument& doc,
                                           FormRendererId form_renderer_id);

// Returns the form control element by unique renderer id.
// |form_to_be_searched| could be used as an optimization to only search for
// elements in it, but doesn't guarantee that the returned element will belong
// to it. Returns the null element if there is no element with the
// |queried_form_control| renderer id.
blink::WebFormControlElement FindFormControlByRendererId(
    const blink::WebDocument& doc,
    FieldRendererId queried_form_control,
    std::optional<FormRendererId> form_to_be_searched = std::nullopt);

// Note: The vector-based API of the following two functions is a tax for
// limiting the frequency and duration of retrieving a lot of DOM elements.
// Alternative solutions have been discussed on https://crrev.com/c/1108201.

// Returns form control elements identified by the given unique renderer IDs.
// The result has the same number of elements as |queried_form_controls| and
// the i-th element of the result corresponds to the i-th element of
// |queried_form_controls|. The call of this function might be time
// expensive, because it retrieves all DOM elements.
std::vector<blink::WebFormControlElement> FindFormControlsByRendererId(
    const blink::WebDocument& doc,
    base::span<const FieldRendererId> queried_form_controls);

// Returns form control elements by unique renderer id. The result has the same
// number elements as |queried_form_controls| and the i-th element of the result
// corresponds to the i-th element of |queried_form_controls|.
// |form_to_be_searched| could be used as an optimization to only search for
// elements in it, but doesn't guarantee that the returned element will belong
// to it.
std::vector<blink::WebFormControlElement> FindFormControlsByRendererId(
    const blink::WebDocument& doc,
    FormRendererId form_renderer_id,
    base::span<const FieldRendererId> queried_form_controls);

blink::WebElement FindContentEditableByRendererId(
    FieldRendererId field_renderer_id);

std::string GetAutocompleteAttribute(const blink::WebElement& element);

// Returns the ARIA label text of the elements denoted by the aria-labelledby
// attribute of |element| or the value of the aria-label attribute of
// |element|, with priority given to the aria-labelledby attribute.
std::u16string GetAriaLabel(const blink::WebDocument& document,
                            const blink::WebElement& element);

// Returns the ARIA label text of the elements denoted by the aria-describedby
// attribute of |element|.
std::u16string GetAriaDescription(const blink::WebDocument& document,
                                  const blink::WebElement& element);

// Helper function to return the next web node of `current_node` in the DOM.
// `forward` determines the direction to traverse in.
blink::WebNode NextWebNode(const blink::WebNode& current_node, bool forward);

// Iterates through the node neighbors of form and form control elements in
// `document` in search of four digit combinations.
void TraverseDomForFourDigitCombinations(
    const blink::WebDocument& document,
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches);

bool IsVisibleIframeForTesting(const blink::WebElement& iframe_element);

// TODO(crbug.com/1007974): There's no internal WebFormElementToFormData()
// anymore. Revise the test to test the interface.
std::optional<FormData> WebFormElementToFormDataForTesting(
    const blink::WebFormElement& form_element,
    const blink::WebFormControlElement& form_control_element,
    const FieldDataManager& field_data_manager,
    DenseSet<ExtractOption> extract_options,
    FormFieldData* field);

}  // namespace form_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_UTIL_H_
