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
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_element_collection.h"
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
enum ExtractMask {
  EXTRACT_NONE = 0,
  EXTRACT_VALUE = 1 << 0,        // Extract value from WebFormControlElement.
  EXTRACT_OPTION_TEXT = 1 << 1,  // Extract option text from
                                 // WebFormSelectElement. Only valid when
                                 // |EXTRACT_VALUE| is set.
                                 // This is used for form submission where
                                 // human readable value is captured.
  EXTRACT_OPTIONS = 1 << 2,      // Extract options from
                                 // WebFormControlElement.
  EXTRACT_BOUNDS = 1 << 3,       // Extract bounds from WebFormControlElement,
                                 // could trigger layout if needed.
  EXTRACT_DATALIST = 1 << 4,     // Extract datalist from WebFormControlElement,
                                 // the total number of options is up to
                                 // kMaxListSize and each option has as far as
                                 // kMaxDataLength.
};

// Autofill supports assigning <label for=x> tags to inputs if x its id/name,
// or the id/name of a shadow host element containing the input.
// This enum is used to track how often each case occurs in practise.
enum class AssignedLabelSource {
  kId = 0,
  kName = 1,
  kShadowHostId = 2,
  kShadowHostName = 3,
  kMaxValue = kShadowHostName,
};
// This temporary histogram is emitted inline, because browser files like
// AutofillMetrics cannot be included here.
// TODO(crbug.com/1339277): Remove.
inline constexpr char kAssignedLabelSourceHistogram[] =
    "Autofill.LabelInference.AssignedLabelSource";

// Indicates if an iframe |element| is considered actually visible to the user.
//
// This function is not intended to implement a perfect visibility check. It
// rather aims to strike balance between cheap tests and filtering invisible
// frames, which can then be skipped during parsing.
//
// The current visibility check requires focusability and a sufficiently large
// bounding box. Thus, particularly elements with "visibility: invisible",
// "display: none", and "width: 0; height: 0" are considered invisible.
//
// Future potential improvements include:
// * Detect potential visibility of elements with "overflow: visible".
//   (See WebElement::GetScrollSize().)
// * Detect invisibility of elements with
//   - "position: absolute; {left,top,right,bottol}: -100px"
//   - "opacity: 0.0"
//   - "clip: rect(0,0,0,0)"
//
// Exposed for testing purposes.
bool IsVisibleIframe(const blink::WebElement& iframe_element);

// Returns the topmost <form> ancestor of |node|, or an IsNull() pointer.
//
// Generally, WebFormElements must not be nested [1]. When parsing HTML, Blink
// ignores nested form tags; the inner forms therefore never make it into the
// DOM. Howevery, nested forms can be created and added to the DOM dynamically,
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
// This function is a simplified/specialised version of Blink's private
// Node::compareDocumentPosition().
//
// Exposed for testing purposes.
bool IsDOMPredecessor(const blink::WebNode& x,
                      const blink::WebNode& y,
                      const blink::WebNode& ancestor_hint);

// Gets up to kMaxListSize data list values (with corresponding label) for the
// given element, each value and label have as far as kMaxDataLength.
void GetDataListSuggestions(const blink::WebInputElement& element,
                            std::vector<std::u16string>* values,
                            std::vector<std::u16string>* labels);

// Extract FormData from the form element and return whether the operation was
// successful.
bool ExtractFormData(const blink::WebFormElement& form_element,
                     const FieldDataManager& field_data_manager,
                     FormData* data);

// Returns true if at least one element from |control_elements| is visible.
bool IsSomeControlElementVisible(
    blink::WebLocalFrame* frame,
    const std::set<FieldRendererId>& control_elements);

// Helper functions to assist in getting the canonical form of the action and
// origin. The action will proplerly take into account <BASE>, and both will
// strip unnecessary data (e.g. query params and HTTP credentials).
GURL GetCanonicalActionForForm(const blink::WebFormElement& form);
GURL GetDocumentUrlWithoutAuth(const blink::WebDocument& document);

// Returns true if |element| is a month input element.
bool IsMonthInput(const blink::WebInputElement& element);

// Returns true if |element| is a text input element.
bool IsTextInput(const blink::WebInputElement& element);

// Returns true if `element` is either a select or a selectmenu element.
bool IsSelectOrSelectMenuElement(const blink::WebFormControlElement& element);

// Returns true if |element| is a select element.
bool IsSelectElement(const blink::WebFormControlElement& element);

// Returns true if `element` is a selectmenu element.
bool IsSelectMenuElement(const blink::WebFormControlElement& element);

// Returns true if |element| is a textarea element.
bool IsTextAreaElement(const blink::WebFormControlElement& element);

// Returns true if `element` is a textarea element or a text input element.
bool IsTextAreaElementOrTextInput(const blink::WebFormControlElement& element);

// Returns true if |element| is a checkbox or a radio button element.
bool IsCheckableElement(const blink::WebFormControlElement& element);

// Returns true if |element| is one of the input element types that can be
// autofilled. {Text, Radiobutton, Checkbox}.
bool IsAutofillableInputElement(const blink::WebInputElement& element);

// Returns true if |element| is one of the element types that can be autofilled.
// {Text, Radiobutton, Checkbox, Select, TextArea}.
bool IsAutofillableElement(const blink::WebFormControlElement& element);

// Returns true if |element| can be edited (enabled and not read only).
bool IsElementEditable(const blink::WebInputElement& element);

// True if this node can take focus. If the layout is blocked, then the function
// checks if the element takes up space in the layout, i.e., this element or a
// descendant has a non-empty bounding client rect.
bool IsWebElementFocusable(const blink::WebElement& element);

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

// Returns the form's |name| attribute if non-empty; otherwise the form's |id|
// attribute.
std::u16string GetFormIdentifier(const blink::WebFormElement& form);

// Returns the FormRendererId of a given WebFormElement. If
// WebFormElement::IsNull(), returns a null form renderer id, which is the
// renderer id of the unowned form.
FormRendererId GetFormRendererId(const blink::WebFormElement& form);

// Returns the FieldRendererId of a given WebFormControlElement.
FieldRendererId GetFieldRendererId(const blink::WebFormControlElement& field);

// Returns text alignment for |element|.
base::i18n::TextDirection GetTextDirectionForElement(
    const blink::WebFormControlElement& element);

// Returns all the auto-fillable form control elements in |control_elements|.
std::vector<blink::WebFormControlElement> ExtractAutofillableElementsFromSet(
    const blink::WebVector<blink::WebFormControlElement>& control_elements);

// Returns all the auto-fillable form control elements in |form_element|.
std::vector<blink::WebFormControlElement> ExtractAutofillableElementsInForm(
    const blink::WebFormElement& form_element);

struct ShadowFieldData;

// Fills out a FormField object from a given WebFormControlElement.
// |extract_mask|: See the enum ExtractMask above for details. Field properties
// will be copied from |field_data_manager|, if the argument is not null and
// has entry for |element| (see properties in FieldPropertiesFlags).
void WebFormControlElementToFormField(
    FormRendererId form_renderer_id,
    const blink::WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormFieldData* field,
    ShadowFieldData* shadow_data = nullptr);

// Fills |form| with the FormData object corresponding to the |form_element|.
// If |field| is non-NULL, also fills |field| with the FormField object
// corresponding to the |form_control_element|. |extract_mask| controls what
// data is extracted. Returns true if |form| is filled out.  Also returns false
// if there are no fields or too many fields in the |form|. Field properties
// will be copied from |field_data_manager|, if the argument is not null and
// has entry for |element| (see properties in FieldPropertiesFlags).
bool WebFormElementToFormData(
    const blink::WebFormElement& form_element,
    const blink::WebFormControlElement& form_control_element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field);

// Get all form control elements from |elements| that are not part of a form.
std::vector<blink::WebFormControlElement> GetUnownedFormFieldElements(
    const blink::WebDocument& document);

// A shorthand for filtering the results of GetUnownedFormFieldElements with
// ExtractAutofillableElementsFromSet.
std::vector<blink::WebFormControlElement>
GetUnownedAutofillableFormFieldElements(const blink::WebDocument& document);

// Returns the <iframe> elements that are not in the scope of any <form>.
std::vector<blink::WebElement> GetUnownedIframeElements(
    const blink::WebDocument& document);

// Returns false iff the extraction fails because the number of fields exceeds
// |kMaxParseableFields|, or |field| and |element| are not nullptr but
// |element| is not among |control_elements|.
bool UnownedFormElementsToFormData(
    const std::vector<blink::WebFormControlElement>& control_elements,
    const std::vector<blink::WebElement>& iframe_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field);

// Finds the form that contains |element| and returns it in |form|.  If |field|
// is non-NULL, fill it with the FormField representation for |element|.
// |additional_extract_mask| control what to extract beside the default mask
// which is EXTRACT_VALUE | EXTRACT_OPTIONS. Returns false if the form is not
// found or cannot be serialized.
bool FindFormAndFieldForFormControlElement(
    const blink::WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    ExtractMask additional_extract_mask,
    FormData* form,
    FormFieldData* field);

// Same as above but with default ExtractMask.
bool FindFormAndFieldForFormControlElement(
    const blink::WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    FormData* form,
    FormFieldData* field);

// Fills or previews the form represented by |form|.  |element| is the input
// element that initiated the auto-fill process. Returns the filled fields.
std::vector<blink::WebFormControlElement> FillOrPreviewForm(
    const FormData& form,
    const blink::WebFormControlElement& element,
    mojom::RendererFormDataAction action);

// Clears the suggested values in |control_elements|. The state of
// |initiating_element| is set to |old_autofill_state|; all other fields are set
// to kNotFilled.
void ClearPreviewedElements(
    std::vector<blink::WebFormControlElement>& control_elements,
    const blink::WebFormControlElement& initiating_element,
    blink::WebAutofillState old_autofill_state);

// Indicates if |node| is owned by |frame| in the sense of
// https://dom.spec.whatwg.org/#concept-node-document. Note that being owned by
// a frame does not require being attached to its DOM.
bool IsOwnedByFrame(const blink::WebNode& node, content::RenderFrame* frame);

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

// Exposed for testing purposes.
std::u16string FindChildTextWithIgnoreListForTesting(
    const blink::WebNode& node,
    const std::set<blink::WebNode>& divs_to_skip);
bool InferLabelForElementForTesting(const blink::WebFormControlElement& element,
                                    std::u16string& label,
                                    FormFieldData::LabelSource& label_source);

// Returns the form element by unique renderer id. Returns the null element if
// there is no form with the |form_renderer_id|.
blink::WebFormElement FindFormByUniqueRendererId(
    const blink::WebDocument& doc,
    FormRendererId form_renderer_id);

// Returns the form control element by unique renderer id. It searches the
// |form_to_be_searched| if specified, otherwise the whole document. Returns the
// null element if there is no element with the |queried_form_control| renderer
// id.
blink::WebFormControlElement FindFormControlElementByUniqueRendererId(
    const blink::WebDocument& doc,
    FieldRendererId queried_form_control,
    absl::optional<FormRendererId> form_to_be_searched = absl::nullopt);

// Note: The vector-based API of the following two functions is a tax for
// limiting the frequency and duration of retrieving a lot of DOM elements.
// Alternative solutions have been discussed on https://crrev.com/c/1108201.

// Returns form control elements identified by the given unique renderer IDs.
// The result has the same number of elements as |queried_form_controls| and
// the i-th element of the result corresponds to the i-th element of
// |queried_form_controls|. The call of this function might be time
// expensive, because it retrieves all DOM elements.
std::vector<blink::WebFormControlElement>
FindFormControlElementsByUniqueRendererId(
    const blink::WebDocument& doc,
    const std::vector<FieldRendererId>& queried_form_controls);

// Returns form control elements by unique renderer id from the form with
// |form_renderer_id|. The result has the same number elements as
// |queried_form_controls| and the i-th element of the result corresponds to
// the i-th element of |queried_form_controls|. This function is faster than
// the previous one, because it only retrieves form control elements from a
// single form.
std::vector<blink::WebFormControlElement>
FindFormControlElementsByUniqueRendererId(
    const blink::WebDocument& doc,
    FormRendererId form_renderer_id,
    const std::vector<FieldRendererId>& queried_form_controls);

// Returns the ARIA label text of the elements denoted by the aria-labelledby
// attribute of |element| or the value of the aria-label attribute of
// |element|, with priority given to the aria-labelledby attribute.
std::u16string GetAriaLabel(const blink::WebDocument& document,
                            const blink::WebElement& element);

// Returns the ARIA label text of the elements denoted by the aria-describedby
// attribute of |element|.
std::u16string GetAriaDescription(const blink::WebDocument& document,
                                  const blink::WebElement& element);

}  // namespace form_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_AUTOFILL_UTIL_H_
