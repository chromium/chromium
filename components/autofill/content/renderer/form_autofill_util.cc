// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_label_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_select_element.h"

using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLabelElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

using mojom::ButtonTitleType;

namespace form_util {

namespace {

// Maximal length of a button's title.
const int kMaxLengthForSingleButtonTitle = 30;
// Maximal length of all button titles.
const int kMaxLengthForAllButtonTitles = 200;

// Text features to detect form submission buttons. Features are selected based
// on analysis of real forms and their buttons.
// TODO(crbug.com/910546): Consider to add more features (e.g. non-English
// features).
const char* const kButtonFeatures[] = {"button", "btn", "submit",
                                       "boton" /* "button" in Spanish */};

// A bit field mask for FillForm functions to not fill some fields.
enum FieldFilterMask {
  FILTER_NONE = 0,
  FILTER_DISABLED_ELEMENTS = 1 << 0,
  FILTER_READONLY_ELEMENTS = 1 << 1,
  // Filters non-focusable elements with the exception of select elements, which
  // are sometimes made non-focusable because they are present for accessibility
  // while a prettier, non-<select> dropdown is shown. We still want to autofill
  // the non-focusable <select>.
  FILTER_NON_FOCUSABLE_ELEMENTS = 1 << 2,
  FILTER_ALL_NON_EDITABLE_ELEMENTS = FILTER_DISABLED_ELEMENTS |
                                     FILTER_READONLY_ELEMENTS |
                                     FILTER_NON_FOCUSABLE_ELEMENTS,
};

// Returns whether sending autofill field metadata to the server is enabled.
// TODO(crbug.com/938804): Remove this when button titles are crowdsourced in
// all channels.
bool IsAutofillFieldMetadataEnabled() {
  static base::NoDestructor<std::string> kGroupName(
      base::FieldTrialList::FindFullName("AutofillFieldMetadata"));
  return base::StartsWith(*kGroupName, "Enabled", base::CompareCase::SENSITIVE);
}

void TruncateString(base::string16* str, size_t max_length) {
  if (str->length() > max_length)
    str->resize(max_length);
}

bool IsOptionElement(const WebElement& element) {
  static base::NoDestructor<WebString> kOption("option");
  return element.HasHTMLTagName(*kOption);
}

bool IsScriptElement(const WebElement& element) {
  static base::NoDestructor<WebString> kScript("script");
  return element.HasHTMLTagName(*kScript);
}

bool IsNoScriptElement(const WebElement& element) {
  static base::NoDestructor<WebString> kNoScript("noscript");
  return element.HasHTMLTagName(*kNoScript);
}

bool HasTagName(const WebNode& node, const blink::WebString& tag) {
  return node.IsElementNode() && node.ToConst<WebElement>().HasHTMLTagName(tag);
}

bool IsElementInControlElementSet(
    const WebElement& element,
    const std::vector<WebFormControlElement>& control_elements) {
  if (!element.IsFormControlElement())
    return false;
  const WebFormControlElement form_control_element =
      element.ToConst<WebFormControlElement>();
  return base::Contains(control_elements, form_control_element);
}

bool IsElementInsideFormOrFieldSet(const WebElement& element,
                                   bool consider_fieldset_tags) {
  for (WebNode parent_node = element.ParentNode(); !parent_node.IsNull();
       parent_node = parent_node.ParentNode()) {
    if (!parent_node.IsElementNode())
      continue;

    WebElement cur_element = parent_node.To<WebElement>();
    if (cur_element.HasHTMLTagName("form") ||
        (consider_fieldset_tags && cur_element.HasHTMLTagName("fieldset"))) {
      return true;
    }
  }
  return false;
}

// Returns true if |node| is an element and it is a container type that
// InferLabelForElement() can traverse.
bool IsTraversableContainerElement(const WebNode& node) {
  if (!node.IsElementNode())
    return false;

  const WebElement element = node.ToConst<WebElement>();
  return element.HasHTMLTagName("dd") || element.HasHTMLTagName("div") ||
         element.HasHTMLTagName("fieldset") || element.HasHTMLTagName("li") ||
         element.HasHTMLTagName("td") || element.HasHTMLTagName("table");
}

// Returns the colspan for a <td> / <th>. Defaults to 1.
size_t CalculateTableCellColumnSpan(const WebElement& element) {
  DCHECK(element.HasHTMLTagName("td") || element.HasHTMLTagName("th"));

  size_t span = 1;
  if (element.HasAttribute("colspan")) {
    base::string16 colspan = element.GetAttribute("colspan").Utf16();
    // Do not check return value to accept imperfect conversions.
    base::StringToSizeT(colspan, &span);
    // Handle overflow.
    if (span == std::numeric_limits<size_t>::max())
      span = 1;
    span = std::max(span, static_cast<size_t>(1));
  }

  return span;
}

// Appends |suffix| to |prefix| so that any intermediary whitespace is collapsed
// to a single space.  If |force_whitespace| is true, then the resulting string
// is guaranteed to have a space between |prefix| and |suffix|.  Otherwise, the
// result includes a space only if |prefix| has trailing whitespace or |suffix|
// has leading whitespace.
// A few examples:
//  * CombineAndCollapseWhitespace("foo", "bar", false)       -> "foobar"
//  * CombineAndCollapseWhitespace("foo", "bar", true)        -> "foo bar"
//  * CombineAndCollapseWhitespace("foo ", "bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", true)       -> "foo bar"
//  * CombineAndCollapseWhitespace("foo   ", "   bar", false) -> "foo bar"
//  * CombineAndCollapseWhitespace(" foo", "bar ", false)     -> " foobar "
//  * CombineAndCollapseWhitespace(" foo", "bar ", true)      -> " foo bar "
const base::string16 CombineAndCollapseWhitespace(const base::string16& prefix,
                                                  const base::string16& suffix,
                                                  bool force_whitespace) {
  base::string16 prefix_trimmed;
  base::TrimPositions prefix_trailing_whitespace =
      base::TrimWhitespace(prefix, base::TRIM_TRAILING, &prefix_trimmed);

  // Recursively compute the children's text.
  base::string16 suffix_trimmed;
  base::TrimPositions suffix_leading_whitespace =
      base::TrimWhitespace(suffix, base::TRIM_LEADING, &suffix_trimmed);

  if (prefix_trailing_whitespace || suffix_leading_whitespace ||
      force_whitespace) {
    return prefix_trimmed + base::ASCIIToUTF16(" ") + suffix_trimmed;
  }
  return prefix_trimmed + suffix_trimmed;
}

// This is a helper function for the FindChildText() function (see below).
// Search depth is limited with the |depth| parameter.
// |divs_to_skip| is a list of <div> tags to ignore if encountered.
base::string16 FindChildTextInner(const WebNode& node,
                                  int depth,
                                  const std::set<WebNode>& divs_to_skip) {
  if (depth <= 0 || node.IsNull())
    return base::string16();

  // Skip over comments.
  if (node.IsCommentNode())
    return FindChildTextInner(node.NextSibling(), depth - 1, divs_to_skip);

  if (!node.IsElementNode() && !node.IsTextNode())
    return base::string16();

  // Ignore elements known not to contain inferable labels.
  if (node.IsElementNode()) {
    const WebElement element = node.ToConst<WebElement>();
    if (IsOptionElement(element) || IsScriptElement(element) ||
        IsNoScriptElement(element) ||
        (element.IsFormControlElement() &&
         IsAutofillableElement(element.ToConst<WebFormControlElement>()))) {
      return base::string16();
    }

    if (element.HasHTMLTagName("div") && base::Contains(divs_to_skip, node))
      return base::string16();
  }

  // Extract the text exactly at this node.
  base::string16 node_text = node.NodeValue().Utf16();

  // Recursively compute the children's text.
  // Preserve inter-element whitespace separation.
  base::string16 child_text =
      FindChildTextInner(node.FirstChild(), depth - 1, divs_to_skip);
  bool add_space = node.IsTextNode() && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, child_text, add_space);

  // Recursively compute the siblings' text.
  // Again, preserve inter-element whitespace separation.
  base::string16 sibling_text =
      FindChildTextInner(node.NextSibling(), depth - 1, divs_to_skip);
  add_space = node.IsTextNode() && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, sibling_text, add_space);

  return node_text;
}

// Same as FindChildText() below, but with a list of div nodes to skip.
base::string16 FindChildTextWithIgnoreList(
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  if (node.IsTextNode())
    return node.NodeValue().Utf16();

  WebNode child = node.FirstChild();

  const int kChildSearchDepth = 10;
  base::string16 node_text =
      FindChildTextInner(child, kChildSearchDepth, divs_to_skip);
  base::TrimWhitespace(node_text, base::TRIM_ALL, &node_text);
  return node_text;
}

bool IsLabelValid(base::StringPiece16 inferred_label,
                  const std::vector<base::char16>& stop_words) {
  // If |inferred_label| has any character other than those in |stop_words|.
  auto* first_non_stop_word = std::find_if(
      inferred_label.begin(), inferred_label.end(),
      [&stop_words](base::char16 c) { return !base::Contains(stop_words, c); });
  return first_non_stop_word != inferred_label.end();
}

// Shared function for InferLabelFromPrevious() and InferLabelFromNext().
bool InferLabelFromSibling(const WebFormControlElement& element,
                           const std::vector<base::char16>& stop_words,
                           bool forward,
                           base::string16* label,
                           FormFieldData::LabelSource* label_source) {
  base::string16 inferred_label;
  FormFieldData::LabelSource inferred_label_source =
      FormFieldData::LabelSource::kUnknown;
  WebNode sibling = element;
  while (true) {
    sibling = forward ? sibling.NextSibling() : sibling.PreviousSibling();
    if (sibling.IsNull())
      break;

    // Skip over comments.
    if (sibling.IsCommentNode())
      continue;

    // Otherwise, only consider normal HTML elements and their contents.
    if (!sibling.IsElementNode() && !sibling.IsTextNode())
      break;

    // A label might be split across multiple "lightweight" nodes.
    // Coalesce any text contained in multiple consecutive
    //  (a) plain text nodes or
    //  (b) inline HTML elements that are essentially equivalent to text nodes.
    static base::NoDestructor<WebString> kBold("b");
    static base::NoDestructor<WebString> kStrong("strong");
    static base::NoDestructor<WebString> kSpan("span");
    static base::NoDestructor<WebString> kFont("font");
    if (sibling.IsTextNode() || HasTagName(sibling, *kBold) ||
        HasTagName(sibling, *kStrong) || HasTagName(sibling, *kSpan) ||
        HasTagName(sibling, *kFont)) {
      base::string16 value = FindChildText(sibling);
      // A text node's value will be empty if it is for a line break.
      bool add_space = sibling.IsTextNode() && value.empty();
      inferred_label_source = FormFieldData::LabelSource::kCombined;
      inferred_label =
          CombineAndCollapseWhitespace(value, inferred_label, add_space);
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    base::string16 trimmed_label;
    base::TrimWhitespace(inferred_label, base::TRIM_ALL, &trimmed_label);
    if (!trimmed_label.empty()) {
      inferred_label_source = FormFieldData::LabelSource::kCombined;
      break;
    }

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    static base::NoDestructor<WebString> kImage("img");
    static base::NoDestructor<WebString> kBreak("br");
    if (HasTagName(sibling, *kImage) || HasTagName(sibling, *kBreak))
      continue;

    // We only expect <p> and <label> tags to contain the full label text.
    static base::NoDestructor<WebString> kPage("p");
    static base::NoDestructor<WebString> kLabel("label");
    bool has_label_tag = HasTagName(sibling, *kLabel);
    if (HasTagName(sibling, *kPage) || has_label_tag) {
      inferred_label = FindChildText(sibling);
      inferred_label_source = has_label_tag
                                  ? FormFieldData::LabelSource::kLabelTag
                                  : FormFieldData::LabelSource::kPTag;
    }

    break;
  }

  base::TrimWhitespace(inferred_label, base::TRIM_ALL, &inferred_label);
  if (IsLabelValid(inferred_label, stop_words)) {
    *label = std::move(inferred_label);
    *label_source = inferred_label_source;
    return true;
  }
  return false;
}

// Helper function to add a button's |title| to the |list|.
void AddButtonTitleToList(base::string16 title,
                          ButtonTitleType button_type,
                          ButtonTitleList* list) {
  title = base::CollapseWhitespace(std::move(title), false);
  if (title.empty())
    return;
  TruncateString(&title, kMaxLengthForSingleButtonTitle);
  list->push_back(std::make_pair(std::move(title), button_type));
}

// Returns true iff |attribute| contains one of |kButtonFeatures|.
bool AttributeHasButtonFeature(const WebString& attribute) {
  if (attribute.IsNull())
    return false;
  std::string value = attribute.Utf8();
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  for (const char* const button_feature : kButtonFeatures) {
    if (value.find(button_feature, 0) != std::string::npos)
      return true;
  }
  return false;
}

// Returns true if |element|'s id, name or css class contain |kButtonFeatures|.
bool ElementAttributesHasButtonFeature(const WebElement& element) {
  return AttributeHasButtonFeature(element.GetAttribute("id")) ||
         AttributeHasButtonFeature(element.GetAttribute("name")) ||
         AttributeHasButtonFeature(element.GetAttribute("class"));
}

// Finds elements from |elements| that contains |kButtonFeatures|, adds them to
// |list| and updates the |total_length| of the |list|'s items.
// If |extract_value_attribute|, the "value" attribute is extracted as a button
// title. Otherwise, |WebElement::TextContent| (aka innerText in Javascript) is
// extracted as a title.
void FindElementsWithButtonFeatures(const WebElementCollection& elements,
                                    bool only_formless_elements,
                                    ButtonTitleType button_type,
                                    bool extract_value_attribute,
                                    ButtonTitleList* list) {
  static base::NoDestructor<WebString> kValue("value");
  for (WebElement item = elements.FirstItem(); !item.IsNull();
       item = elements.NextItem()) {
    if (!ElementAttributesHasButtonFeature(item))
      continue;
    if (only_formless_elements &&
        IsElementInsideFormOrFieldSet(item,
                                      false /* consider_fieldset_tags */)) {
      continue;
    }
    base::string16 title =
        extract_value_attribute
            ? (item.HasAttribute(*kValue) ? item.GetAttribute(*kValue).Utf16()
                                          : base::string16())
            : item.TextContent().Utf16();
    if (extract_value_attribute && title.empty())
      title = item.TextContent().Utf16();
    AddButtonTitleToList(std::move(title), button_type, list);
  }
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a previous sibling of |element|,
// e.g. Some Text <input ...>
// or   Some <span>Text</span> <input ...>
// or   <p>Some Text</p><input ...>
// or   <label>Some Text</label> <input ...>
// or   Some Text <img><input ...>
// or   <b>Some Text</b><br/> <input ...>.
bool InferLabelFromPrevious(const WebFormControlElement& element,
                            const std::vector<base::char16>& stop_words,
                            base::string16* label,
                            FormFieldData::LabelSource* label_source) {
  return InferLabelFromSibling(element, stop_words, false /* forward? */, label,
                               label_source);
}

// Same as InferLabelFromPrevious(), but in the other direction.
// Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
bool InferLabelFromNext(const WebFormControlElement& element,
                        const std::vector<base::char16>& stop_words,
                        base::string16* label,
                        FormFieldData::LabelSource* label_source) {
  return InferLabelFromSibling(element, stop_words, true /* forward? */, label,
                               label_source);
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// the placeholder text. e.g. <input placeholder="foo">
base::string16 InferLabelFromPlaceholder(const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kPlaceholder("placeholder");
  if (element.HasAttribute(*kPlaceholder))
    return element.GetAttribute(*kPlaceholder).Utf16();

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// the aria-label. e.g. <input aria-label="foo">
base::string16 InferLabelFromAriaLabel(const WebFormControlElement& element) {
  static const base::NoDestructor<WebString> kAriaLabel("aria-label");
  if (element.HasAttribute(*kAriaLabel))
    return element.GetAttribute(*kAriaLabel).Utf16();

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, from
// the value attribute when it is present and user has not typed in (if
// element's value attribute is same as the element's value).
base::string16 InferLabelFromValueAttr(const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kValue("value");
  if (element.HasAttribute(*kValue) &&
      element.GetAttribute(*kValue) == element.Value()) {
    return element.GetAttribute(*kValue).Utf16();
  }

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// enclosing list item,
// e.g. <li>Some Text<input ...><input ...><input ...></li>
base::string16 InferLabelFromListItem(const WebFormControlElement& element) {
  WebNode parent = element.ParentNode();
  static base::NoDestructor<WebString> kListItem("li");
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kListItem)) {
    parent = parent.ParentNode();
  }

  if (!parent.IsNull() && HasTagName(parent, *kListItem))
    return FindChildText(parent);

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// enclosing label,
// e.g. <label>Some Text<input ...><input ...><input ...></label>
base::string16 InferLabelFromEnclosingLabel(
    const WebFormControlElement& element) {
  WebNode parent = element.ParentNode();
  static base::NoDestructor<WebString> kLabel("label");
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kLabel)) {
    parent = parent.ParentNode();
  }

  if (!parent.IsNull() && HasTagName(parent, *kLabel))
    return FindChildText(parent);

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td><td><input ...></td></tr>
// or   <tr><th>Some Text</th><td><input ...></td></tr>
// or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
// or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
base::string16 InferLabelFromTableColumn(const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kTableCell("td");
  WebNode parent = element.ParentNode();
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kTableCell)) {
    parent = parent.ParentNode();
  }

  if (parent.IsNull())
    return base::string16();

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  base::string16 inferred_label;
  WebNode previous = parent.PreviousSibling();
  static base::NoDestructor<WebString> kTableHeader("th");
  while (inferred_label.empty() && !previous.IsNull()) {
    if (HasTagName(previous, *kTableCell) ||
        HasTagName(previous, *kTableHeader))
      inferred_label = FindChildText(previous);

    previous = previous.PreviousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
//
// If there are multiple cells and the row with the input matches up with the
// previous row, then look for a specific cell within the previous row.
// e.g. <tr><td>Input 1 label</td><td>Input 2 label</td></tr>
//      <tr><td><input name="input 1"></td><td><input name="input2"></td></tr>
//
// Otherwise, just look in the entire previous row.
// e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
base::string16 InferLabelFromTableRow(const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kTableCell("td");
  base::string16 inferred_label;

  // First find the <td> that contains |element|.
  WebNode cell = element.ParentNode();
  while (!cell.IsNull()) {
    if (cell.IsElementNode() &&
        cell.To<WebElement>().HasHTMLTagName(*kTableCell)) {
      break;
    }
    cell = cell.ParentNode();
  }

  // Not in a cell - bail out.
  if (cell.IsNull())
    return inferred_label;

  // Count the cell holding |element|.
  size_t cell_count = CalculateTableCellColumnSpan(cell.To<WebElement>());
  size_t cell_position = 0;
  size_t cell_position_end = cell_count - 1;

  // Count cells to the left to figure out |element|'s cell's position.
  for (WebNode cell_it = cell.PreviousSibling(); !cell_it.IsNull();
       cell_it = cell_it.PreviousSibling()) {
    if (cell_it.IsElementNode() &&
        cell_it.To<WebElement>().HasHTMLTagName(*kTableCell)) {
      cell_position += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Count cells to the right.
  for (WebNode cell_it = cell.NextSibling(); !cell_it.IsNull();
       cell_it = cell_it.NextSibling()) {
    if (cell_it.IsElementNode() &&
        cell_it.To<WebElement>().HasHTMLTagName(*kTableCell)) {
      cell_count += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Combine left + right.
  cell_count += cell_position;
  cell_position_end += cell_position;

  // Find the current row.
  static base::NoDestructor<WebString> kTableRow("tr");
  WebNode parent = element.ParentNode();
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kTableRow)) {
    parent = parent.ParentNode();
  }

  if (parent.IsNull())
    return inferred_label;

  // Now find the previous row.
  WebNode row_it = parent.PreviousSibling();
  while (!row_it.IsNull()) {
    if (row_it.IsElementNode() &&
        row_it.To<WebElement>().HasHTMLTagName(*kTableRow)) {
      break;
    }
    row_it = row_it.PreviousSibling();
  }

  // If there exists a previous row, check its cells and size. If they align
  // with the current row, infer the label from the cell above.
  if (!row_it.IsNull()) {
    WebNode matching_cell;
    size_t prev_row_count = 0;
    WebNode prev_row_it = row_it.FirstChild();
    static base::NoDestructor<WebString> kTableHeader("th");
    while (!prev_row_it.IsNull()) {
      if (prev_row_it.IsElementNode()) {
        WebElement prev_row_element = prev_row_it.To<WebElement>();
        if (prev_row_element.HasHTMLTagName(*kTableCell) ||
            prev_row_element.HasHTMLTagName(*kTableHeader)) {
          size_t span = CalculateTableCellColumnSpan(prev_row_element);
          size_t prev_row_count_end = prev_row_count + span - 1;
          if (prev_row_count == cell_position &&
              prev_row_count_end == cell_position_end) {
            matching_cell = prev_row_it;
          }
          prev_row_count += span;
        }
      }
      prev_row_it = prev_row_it.NextSibling();
    }
    if ((cell_count == prev_row_count) && !matching_cell.IsNull()) {
      inferred_label = FindChildText(matching_cell);
      if (!inferred_label.empty())
        return inferred_label;
    }
  }

  // If there is no previous row, or if the previous row and current row do not
  // align, check all previous siblings, skipping non-element nodes, until we
  // find a non-empty text block.
  WebNode previous = parent.PreviousSibling();
  while (inferred_label.empty() && !previous.IsNull()) {
    if (HasTagName(previous, *kTableRow))
      inferred_label = FindChildText(previous);

    previous = previous.PreviousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding div table,
// e.g. <div>Some Text<span><input ...></span></div>
// e.g. <div>Some Text</div><div><input ...></div>
//
// Because this is already traversing the <div> structure, if it finds a <label>
// sibling along the way, infer from that <label>.
base::string16 InferLabelFromDivTable(const WebFormControlElement& element) {
  WebNode node = element.ParentNode();
  bool looking_for_parent = true;
  std::set<WebNode> divs_to_skip;

  // Search the sibling and parent <div>s until we find a candidate label.
  base::string16 inferred_label;
  static base::NoDestructor<WebString> kDiv("div");
  static base::NoDestructor<WebString> kLabel("label");
  while (inferred_label.empty() && !node.IsNull()) {
    if (HasTagName(node, *kDiv)) {
      if (looking_for_parent)
        inferred_label = FindChildTextWithIgnoreList(node, divs_to_skip);
      else
        inferred_label = FindChildText(node);

      // Avoid sibling DIVs that contain autofillable fields.
      if (!looking_for_parent && !inferred_label.empty()) {
        static base::NoDestructor<WebString> kSelector(
            "input, select, textarea");
        WebElement result_element = node.QuerySelector(*kSelector);
        if (!result_element.IsNull()) {
          inferred_label.clear();
          divs_to_skip.insert(node);
        }
      }

      looking_for_parent = false;
    } else if (!looking_for_parent && HasTagName(node, *kLabel)) {
      WebLabelElement label_element = node.To<WebLabelElement>();
      if (label_element.CorrespondingControl().IsNull())
        inferred_label = FindChildText(node);
    } else if (looking_for_parent && IsTraversableContainerElement(node)) {
      // If the element is in a non-div container, its label most likely is too.
      break;
    }

    if (node.PreviousSibling().IsNull()) {
      // If there are no more siblings, continue walking up the tree.
      looking_for_parent = true;
    }

    node = looking_for_parent ? node.ParentNode() : node.PreviousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding definition list,
// e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
// e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
base::string16 InferLabelFromDefinitionList(
    const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kDefinitionData("dd");
  WebNode parent = element.ParentNode();
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kDefinitionData))
    parent = parent.ParentNode();

  if (parent.IsNull() || !HasTagName(parent, *kDefinitionData))
    return base::string16();

  // Skip by any intervening text nodes.
  WebNode previous = parent.PreviousSibling();
  while (!previous.IsNull() && previous.IsTextNode())
    previous = previous.PreviousSibling();

  static base::NoDestructor<WebString> kDefinitionTag("dt");
  if (previous.IsNull() || !HasTagName(previous, *kDefinitionTag))
    return base::string16();

  return FindChildText(previous);
}

// Returns the element type for all ancestor nodes in CAPS, starting with the
// parent node.
std::vector<std::string> AncestorTagNames(
    const WebFormControlElement& element) {
  std::vector<std::string> tag_names;
  for (WebNode parent_node = element.ParentNode(); !parent_node.IsNull();
       parent_node = parent_node.ParentNode()) {
    if (!parent_node.IsElementNode())
      continue;

    tag_names.push_back(parent_node.To<WebElement>().TagName().Utf8());
  }
  return tag_names;
}

// Infers corresponding label for |element| from surrounding context in the DOM,
// e.g. the contents of the preceding <p> tag or text element.
bool InferLabelForElement(const WebFormControlElement& element,
                          const std::vector<base::char16>& stop_words,
                          base::string16* label,
                          FormFieldData::LabelSource* label_source) {
  if (IsCheckableElement(ToWebInputElement(&element))) {
    if (InferLabelFromNext(element, stop_words, label, label_source))
      return true;
  }

  if (InferLabelFromPrevious(element, stop_words, label, label_source))
    return true;

  // If we didn't find a label, check for placeholder text.
  base::string16 inferred_label = InferLabelFromPlaceholder(element);
  if (IsLabelValid(inferred_label, stop_words)) {
    *label_source = FormFieldData::LabelSource::kPlaceHolder;
    *label = std::move(inferred_label);
    return true;
  }

  // If we didn't find a placeholder, check for aria-label text.
  inferred_label = InferLabelFromAriaLabel(element);
  if (IsLabelValid(inferred_label, stop_words)) {
    *label_source = FormFieldData::LabelSource::kAriaLabel;
    *label = std::move(inferred_label);
    return true;
  }

  // For all other searches that involve traversing up the tree, the search
  // order is based on which tag is the closest ancestor to |element|.
  std::vector<std::string> tag_names = AncestorTagNames(element);
  std::set<std::string> seen_tag_names;
  FormFieldData::LabelSource ancestor_label_source =
      FormFieldData::LabelSource::kUnknown;
  for (const std::string& tag_name : tag_names) {
    if (base::Contains(seen_tag_names, tag_name))
      continue;

    seen_tag_names.insert(tag_name);
    if (tag_name == "LABEL") {
      ancestor_label_source = FormFieldData::LabelSource::kLabelTag;
      inferred_label = InferLabelFromEnclosingLabel(element);
    } else if (tag_name == "DIV") {
      ancestor_label_source = FormFieldData::LabelSource::kDivTable;
      inferred_label = InferLabelFromDivTable(element);
    } else if (tag_name == "TD") {
      ancestor_label_source = FormFieldData::LabelSource::kTdTag;
      inferred_label = InferLabelFromTableColumn(element);
      if (!IsLabelValid(inferred_label, stop_words))
        inferred_label = InferLabelFromTableRow(element);
    } else if (tag_name == "DD") {
      ancestor_label_source = FormFieldData::LabelSource::kDdTag;
      inferred_label = InferLabelFromDefinitionList(element);
    } else if (tag_name == "LI") {
      ancestor_label_source = FormFieldData::LabelSource::kLiTag;
      inferred_label = InferLabelFromListItem(element);
    } else if (tag_name == "FIELDSET") {
      break;
    }

    if (IsLabelValid(inferred_label, stop_words)) {
      *label_source = ancestor_label_source;
      *label = std::move(inferred_label);
      return true;
    }
  }

  // If we didn't find a label, check the value attr used as the placeholder.
  inferred_label = InferLabelFromValueAttr(element);
  if (IsLabelValid(inferred_label, stop_words)) {
    *label_source = FormFieldData::LabelSource::kValue;
    *label = std::move(inferred_label);
    return true;
  }
  return false;
}

// Removes the duplicate titles and limits totals length. The order of the list
// is preserved as first elements are more reliable features than following
// ones.
void RemoveDuplicatesAndLimitTotalLength(ButtonTitleList* result) {
  std::set<ButtonTitleInfo> already_added;
  ButtonTitleList unique_titles;
  int total_length = 0;
  for (auto title : *result) {
    if (already_added.find(title) != already_added.end())
      continue;
    already_added.insert(title);

    total_length += title.first.length();
    if (total_length > kMaxLengthForAllButtonTitles) {
      int new_length =
          title.first.length() - (total_length - kMaxLengthForAllButtonTitles);
      TruncateString(&title.first, new_length);
    }
    unique_titles.push_back(std::move(title));

    if (total_length >= kMaxLengthForAllButtonTitles)
      break;
  }
  *result = std::move(unique_titles);
}

// Infer button titles enclosed by |root_element|. |root_element| may be a
// <form> or a <body> if there are <input>s that are not enclosed in a <form>
// element.
ButtonTitleList InferButtonTitlesForForm(const WebElement& root_element) {
  static base::NoDestructor<WebString> kA("a");
  static base::NoDestructor<WebString> kButton("button");
  static base::NoDestructor<WebString> kDiv("div");
  static base::NoDestructor<WebString> kForm("form");
  static base::NoDestructor<WebString> kInput("input");
  static base::NoDestructor<WebString> kSpan("span");
  static base::NoDestructor<WebString> kSubmit("submit");

  // If the |root_element| is not a <form>, ignore the elements inclosed in a
  // <form>.
  bool only_formless_elements = !root_element.HasHTMLTagName(*kForm);

  ButtonTitleList result;
  WebElementCollection input_elements =
      root_element.GetElementsByHTMLTagName(*kInput);
  int total_length = 0;
  for (WebElement item = input_elements.FirstItem();
       !item.IsNull() && total_length < kMaxLengthForAllButtonTitles;
       item = input_elements.NextItem()) {
    DCHECK(item.IsFormControlElement());
    WebFormControlElement control_element =
        item.ToConst<WebFormControlElement>();
    if (only_formless_elements && !control_element.Form().IsNull())
      continue;
    bool is_submit_input =
        control_element.FormControlTypeForAutofill() == *kSubmit;
    bool is_button_input =
        control_element.FormControlTypeForAutofill() == *kButton;
    if (!is_submit_input && !is_button_input)
      continue;
    base::string16 title = control_element.Value().Utf16();
    AddButtonTitleToList(std::move(title),
                         is_submit_input
                             ? ButtonTitleType::INPUT_ELEMENT_SUBMIT_TYPE
                             : ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE,
                         &result);
  }
  WebElementCollection buttons =
      root_element.GetElementsByHTMLTagName(*kButton);
  for (WebElement item = buttons.FirstItem(); !item.IsNull();
       item = buttons.NextItem()) {
    WebString type_attribute = item.GetAttribute("type");
    if (!type_attribute.IsNull() && type_attribute != *kButton &&
        type_attribute != *kSubmit) {
      // Neither type='submit' nor type='button'. Skip this button.
      continue;
    }
    if (only_formless_elements &&
        IsElementInsideFormOrFieldSet(item,
                                      false /* consider_fieldset_tags */)) {
      continue;
    }
    bool is_submit_type = type_attribute.IsNull() || type_attribute == *kSubmit;
    base::string16 title = item.TextContent().Utf16();
    AddButtonTitleToList(std::move(title),
                         is_submit_type
                             ? ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE
                             : ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE,
                         &result);
  }
  FindElementsWithButtonFeatures(
      root_element.GetElementsByHTMLTagName(*kA), only_formless_elements,
      ButtonTitleType::HYPERLINK, true /* extract_value_attribute */, &result);
  FindElementsWithButtonFeatures(root_element.GetElementsByHTMLTagName(*kDiv),
                                 only_formless_elements, ButtonTitleType::DIV,
                                 false /* extract_value_attribute */, &result);
  FindElementsWithButtonFeatures(root_element.GetElementsByHTMLTagName(*kSpan),
                                 only_formless_elements, ButtonTitleType::SPAN,
                                 false /* extract_value_attribute */, &result);
  RemoveDuplicatesAndLimitTotalLength(&result);
  return result;
}

// Fills |option_strings| with the values of the <option> elements present in
// |select_element|.
void GetOptionStringsFromElement(const WebSelectElement& select_element,
                                 std::vector<base::string16>* option_values,
                                 std::vector<base::string16>* option_contents) {
  DCHECK(!select_element.IsNull());

  option_values->clear();
  option_contents->clear();
  WebVector<WebElement> list_items = select_element.GetListItems();

  // Constrain the maximum list length to prevent a malicious site from DOS'ing
  // the browser, without entirely breaking autocomplete for some extreme
  // legitimate sites: http://crbug.com/49332 and http://crbug.com/363094
  if (list_items.size() > kMaxListSize)
    return;

  option_values->reserve(list_items.size());
  option_contents->reserve(list_items.size());
  for (size_t i = 0; i < list_items.size(); ++i) {
    if (IsOptionElement(list_items[i])) {
      const WebOptionElement option = list_items[i].ToConst<WebOptionElement>();
      option_values->push_back(option.Value().Utf16());
      option_contents->push_back(option.GetText().Utf16());
    }
  }
}

// The callback type used by |ForEachMatchingFormField()|.
typedef void (*Callback)(const FormFieldData&,
                         bool, /* is_initiating_element */
                         blink::WebFormControlElement*);

void ForEachMatchingFormFieldCommon(
    std::vector<WebFormControlElement>* control_elements,
    const WebElement& initiating_element,
    const FormData& data,
    FieldFilterMask filters,
    bool force_override,
    bool is_preview,
    const Callback& callback) {
  DCHECK(control_elements);

  const bool num_elements_matches_num_fields =
      control_elements->size() == data.fields.size();
  UMA_HISTOGRAM_BOOLEAN("Autofill.NumElementsMatchesNumFields",
                        num_elements_matches_num_fields);
  if (!num_elements_matches_num_fields) {
    // http://crbug.com/841784
    // This pathological case was only thought to be reachable iff the fields
    // are added/removed from the form while the user is interacting with the
    // autofill popup.
    //
    // Is is also reachable for formless non-checkout forms when checkout
    // restrictions are applied.
    //
    // TODO(crbug/847221): Add a UKM to capture these events.
    return;
  }

  // The intended behaviour is:
  // * Autofill the currently focused element.
  // * Send the blur event.
  // * For each other element, focus -> autofill -> blur.
  // * Send the focus event for the initially focused element.
  WebFormControlElement* initially_focused_element = nullptr;

  // This container stores the indexes of non-focused elements to be autofilled.
  std::vector<size_t> autofillable_elements_index;

  // It's possible that the site has injected fields into the form after the
  // page has loaded, so we can't assert that the size of the cached control
  // elements is equal to the size of the fields in |form|.  Fortunately, the
  // one case in the wild where this happens, paypal.com signup form, the fields
  // are appended to the end of the form and are not visible.
  for (size_t i = 0; i < control_elements->size(); ++i) {
    WebFormControlElement* element = &(*control_elements)[i];
    element->SetAutofillSection(WebString::FromUTF8(data.fields[i].section));

    bool is_initiating_element = (*element == initiating_element);

    // Only autofill empty fields (or those with the field's default value
    // attribute) and the field that initiated the filling, i.e. the field the
    // user is currently editing and interacting with.
    const WebInputElement* input_element = ToWebInputElement(element);
    static base::NoDestructor<WebString> kValue("value");
    static base::NoDestructor<WebString> kPlaceholder("placeholder");

    if (FieldRendererId(element->UniqueRendererFormControlId()) !=
        data.fields[i].unique_renderer_id) {
      continue;
    }

    if (((filters & FILTER_DISABLED_ELEMENTS) && !element->IsEnabled()) ||
        ((filters & FILTER_READONLY_ELEMENTS) && element->IsReadOnly()) ||
        // See description for FILTER_NON_FOCUSABLE_ELEMENTS.
        ((filters & FILTER_NON_FOCUSABLE_ELEMENTS) && !element->IsFocusable() &&
         !IsSelectElement(*element))) {
      continue;
    }

    // Autofill the initiating element.
    if (is_initiating_element) {
      if (!is_preview && element->Focused())
        initially_focused_element = element;

      callback(data.fields[i], is_initiating_element, element);
      continue;
    }

    if (element->GetAutofillState() == WebAutofillState::kAutofilled)
      continue;

    if (!force_override &&
        // A text field, with a non-empty value that is entered by the user,
        // and is NOT the value of the input field's "value" or "placeholder"
        // attribute, is skipped. Some sites fill the fields with formatting
        // string. To tell the difference between the values entered by the user
        // and the site, we'll sanitize the value. If the sanitized value is
        // empty, it means that the site has filled the field, in this case, the
        // field is not skipped. Nevertheless the below condition does not hold
        // for sites set the |kValue| attribute to the user-input value.
        (IsAutofillableInputElement(input_element) ||
         IsTextAreaElement(*element)) &&
        element->UserHasEditedTheField() &&
        !SanitizedFieldIsEmpty(element->Value().Utf16()) &&
        (!element->HasAttribute(*kValue) ||
         element->GetAttribute(*kValue) != element->Value()) &&
        (!element->HasAttribute(*kPlaceholder) ||
         base::i18n::ToLower(element->GetAttribute(*kPlaceholder).Utf16()) !=
             base::i18n::ToLower(element->Value().Utf16()))) {
      continue;
    }

    // Check if we should autofill/preview/clear a select element or leave it.
    if (!force_override && IsSelectElement(*element) &&
        element->UserHasEditedTheField() &&
        !SanitizedFieldIsEmpty(element->Value().Utf16())) {
      continue;
    }

    // Storing the indexes of non-initiating elements to be autofilled after
    // triggering the blur event for the initiating element.
    autofillable_elements_index.push_back(i);
  }

  // If there is no other field to be autofilled, sending the blur event and
  // then the focus event for the initiating element does not make sense.
  if (autofillable_elements_index.empty())
    return;

  // A blur event is emitted for the focused element if it is the initiating
  // element before all other elements are autofilled.
  if (initially_focused_element)
    initially_focused_element->DispatchBlurEvent();

  // Autofill the non-initiating elements.
  for (const auto& index : autofillable_elements_index)
    callback(data.fields[index], false, &(*control_elements)[index]);

  // A focus event is emitted for the initiating element after autofilling is
  // completed. It is not intended to work for the preview filling.
  if (initially_focused_element)
    initially_focused_element->DispatchFocusEvent();
}

// For each autofillable field in |data| that matches a field in the |form|,
// the |callback| is invoked with the corresponding |form| field data.
void ForEachMatchingFormField(const WebFormElement& form_element,
                              const WebElement& initiating_element,
                              const FormData& data,
                              FieldFilterMask filters,
                              bool force_override,
                              bool is_preview,
                              const Callback& callback) {
  std::vector<WebFormControlElement> control_elements =
      ExtractAutofillableElementsInForm(form_element);
  ForEachMatchingFormFieldCommon(&control_elements, initiating_element, data,
                                 filters, force_override, is_preview, callback);
}

// For each autofillable field in |data| that matches a field in the set of
// unowned autofillable form fields, the |callback| is invoked with the
// corresponding |data| field.
void ForEachMatchingUnownedFormField(const WebElement& initiating_element,
                                     const FormData& data,
                                     FieldFilterMask filters,
                                     bool force_override,
                                     bool is_preview,
                                     const Callback& callback) {
  if (initiating_element.IsNull())
    return;

  std::vector<WebFormControlElement> control_elements =
      GetUnownedAutofillableFormFieldElements(
          initiating_element.GetDocument().All(), nullptr);
  if (!IsElementInControlElementSet(initiating_element, control_elements))
    return;

  ForEachMatchingFormFieldCommon(&control_elements, initiating_element, data,
                                 filters, force_override, is_preview, callback);
}

// Sets the |field|'s value to the value in |data|, and specifies the section
// for filled fields.  Also sets the "autofilled" attribute,
// causing the background to be yellow.
void FillFormField(const FormFieldData& data,
                   bool is_initiating_node,
                   blink::WebFormControlElement* field) {
  // Nothing to fill.
  if (data.value.empty())
    return;

  if (!data.is_autofilled)
    return;

  WebInputElement* input_element = ToWebInputElement(field);

  if (IsCheckableElement(input_element)) {
    input_element->SetChecked(IsChecked(data.check_status), true);
  } else {
    base::string16 value = data.value;
    if (IsTextInput(input_element) || IsMonthInput(input_element)) {
      // If the maxlength attribute contains a negative value, maxLength()
      // returns the default maxlength value.
      TruncateString(&value, input_element->MaxLength());
    }
    field->SetAutofillValue(blink::WebString::FromUTF16(value));
  }
  // Setting the form might trigger JavaScript, which is capable of
  // destroying the frame.
  if (!field->GetDocument().GetFrame())
    return;

  field->SetAutofillState(WebAutofillState::kAutofilled);

  if (is_initiating_node &&
      ((IsTextInput(input_element) || IsMonthInput(input_element)) ||
       IsTextAreaElement(*field))) {
    int length = field->Value().length();
    field->SetSelectionRange(length, length);
    // Clear the current IME composition (the underline), if there is one.
    field->GetDocument().GetFrame()->UnmarkText();
  }
}

// Sets the |field|'s "suggested" (non JS visible) value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void PreviewFormField(const FormFieldData& data,
                      bool is_initiating_node,
                      blink::WebFormControlElement* field) {
  // Nothing to preview.
  if (data.value.empty())
    return;

  if (!data.is_autofilled)
    return;

  // Preview input, textarea and select fields. For input fields, excludes
  // checkboxes and radio buttons, as there is no provision for
  // setSuggestedCheckedValue in WebInputElement.
  WebInputElement* input_element = ToWebInputElement(field);
  if (IsTextInput(input_element) || IsMonthInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    input_element->SetSuggestedValue(blink::WebString::FromUTF16(
        data.value.substr(0, input_element->MaxLength())));
    input_element->SetAutofillState(WebAutofillState::kPreviewed);
  } else if (IsTextAreaElement(*field) || IsSelectElement(*field)) {
    field->SetSuggestedValue(blink::WebString::FromUTF16(data.value));
    field->SetAutofillState(WebAutofillState::kPreviewed);
  }

  if (is_initiating_node &&
      (IsTextInput(input_element) || IsTextAreaElement(*field))) {
    // Select the part of the text that the user didn't type.
    PreviewSuggestion(field->SuggestedValue().Utf16(), field->Value().Utf16(),
                      field);
  }
}

// Extracts the fields from |control_elements| with |extract_mask| to
// |form_fields|. The extracted fields are also placed in |element_map|.
// |form_fields| and |element_map| should start out empty.
// |fields_extracted| should have as many elements as |control_elements|,
// initialized to false.
// Returns true if the number of fields extracted is within
// [1, kMaxParseableFields].
bool ExtractFieldsFromControlElements(
    const WebVector<WebFormControlElement>& control_elements,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    std::vector<std::unique_ptr<FormFieldData>>* form_fields,
    std::vector<bool>* fields_extracted,
    std::map<WebFormControlElement, FormFieldData*>* element_map) {
  DCHECK(form_fields->empty());
  DCHECK(element_map->empty());
  DCHECK_EQ(control_elements.size(), fields_extracted->size());

  for (size_t i = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& control_element = control_elements[i];

    if (!IsAutofillableElement(control_element))
      continue;

    // Create a new FormFieldData, fill it out and map it to the field's name.
    auto form_field = std::make_unique<FormFieldData>();
    WebFormControlElementToFormField(control_element, field_data_manager,
                                     extract_mask, form_field.get());
    (*element_map)[control_element] = form_field.get();
    form_fields->push_back(std::move(form_field));
    (*fields_extracted)[i] = true;

    // To avoid overly expensive computation, we impose a maximum number of
    // allowable fields.
    if (form_fields->size() > kMaxParseableFields)
      return false;
  }

  // Succeeded if fields were extracted.
  return !form_fields->empty();
}

// For each label element, get the corresponding form control element, use the
// form control element's name as a key into the
// <WebFormControlElement, FormFieldData> map to find the previously created
// FormFieldData and set the FormFieldData's label to the
// label.firstChild().nodeValue() of the label element.
void MatchLabelsAndFields(
    const WebElementCollection& labels,
    std::map<WebFormControlElement, FormFieldData*>* element_map) {
  static base::NoDestructor<WebString> kFor("for");
  static base::NoDestructor<WebString> kHidden("hidden");

  for (WebElement item = labels.FirstItem(); !item.IsNull();
       item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    WebElement control = label.CorrespondingControl();
    FormFieldData* field_data = nullptr;

    if (control.IsNull()) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      base::string16 element_name = label.GetAttribute(*kFor).Utf16();
      if (element_name.empty())
        continue;
      // Look through the list for elements with this name. There can actually
      // be more than one. In this case, the label may not be particularly
      // useful, so just discard it.
      for (const auto& iter : *element_map) {
        if (iter.second->name == element_name) {
          if (field_data) {
            field_data = nullptr;
            break;
          }
          field_data = iter.second;
        }
      }
    } else if (control.IsFormControlElement()) {
      WebFormControlElement form_control = control.To<WebFormControlElement>();
      if (form_control.FormControlTypeForAutofill() == *kHidden)
        continue;
      // Typical case: look up |field_data| in |element_map|.
      auto iter = element_map->find(form_control);
      if (iter == element_map->end())
        continue;
      field_data = iter->second;
    }

    if (!field_data)
      continue;

    base::string16 label_text = FindChildText(label);

    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (!field_data->label.empty() && !label_text.empty())
      field_data->label += base::ASCIIToUTF16(" ");
    field_data->label += label_text;
  }
}

// Common function shared by WebFormElementToFormData() and
// UnownedFormElementsAndFieldSetsToFormData(). Either pass in:
// 1) |form_element| and an empty |fieldsets|.
// or
// 2) a NULL |form_element|.
//
// If |field| is not NULL, then |form_control_element| should not be NULL.
bool FormOrFieldsetsToFormData(
    const blink::WebFormElement* form_element,
    const blink::WebFormControlElement* form_control_element,
    const std::vector<blink::WebElement>& fieldsets,
    const WebVector<WebFormControlElement>& control_elements,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  static base::NoDestructor<WebString> kLabel("label");

  if (form_element)
    DCHECK(fieldsets.empty());
  if (field)
    DCHECK(form_control_element);

  // A map from a FormFieldData's name to the FormFieldData itself.
  std::map<WebFormControlElement, FormFieldData*> element_map;

  // The extracted FormFields. We use pointers so we can store them in
  // |element_map|.
  std::vector<std::unique_ptr<FormFieldData>> form_fields;

  // A vector of bools that indicate whether each field in the form meets the
  // requirements and thus will be in the resulting |form|.
  std::vector<bool> fields_extracted(control_elements.size(), false);

  if (!ExtractFieldsFromControlElements(control_elements, field_data_manager,
                                        extract_mask, &form_fields,
                                        &fields_extracted, &element_map)) {
    return false;
  }

  if (form_element) {
    // Loop through the label elements inside the form element.  For each label
    // element, get the corresponding form control element, use the form control
    // element's name as a key into the <name, FormFieldData> map to find the
    // previously created FormFieldData and set the FormFieldData's label to the
    // label.firstChild().nodeValue() of the label element.
    WebElementCollection labels =
        form_element->GetElementsByHTMLTagName(*kLabel);
    DCHECK(!labels.IsNull());
    MatchLabelsAndFields(labels, &element_map);
  } else {
    // Same as the if block, but for all the labels in fieldsets.
    for (size_t i = 0; i < fieldsets.size(); ++i) {
      WebElementCollection labels =
          fieldsets[i].GetElementsByHTMLTagName(*kLabel);
      DCHECK(!labels.IsNull());
      MatchLabelsAndFields(labels, &element_map);
    }
  }

  // List of characters a label can't be entirely made of (this list can grow).
  // Since the term |stop_words| is a known text processing concept we use here
  // it to refer to such characters. They are not to be confused with words.
  std::vector<base::char16> stop_words;
  stop_words.push_back(static_cast<base::char16>(' '));
  stop_words.push_back(static_cast<base::char16>('*'));
  stop_words.push_back(static_cast<base::char16>(':'));
  stop_words.push_back(static_cast<base::char16>('-'));
  stop_words.push_back(static_cast<base::char16>(L'\u2013'));
  stop_words.push_back(static_cast<base::char16>('('));
  stop_words.push_back(static_cast<base::char16>(')'));

  // Loop through the form control elements, extracting the label text from
  // the DOM.  We use the |fields_extracted| vector to make sure we assign the
  // extracted label to the correct field, as it's possible |form_fields| will
  // not contain all of the elements in |control_elements|.
  bool found_field = false;
  for (size_t i = 0, field_idx = 0;
       i < control_elements.size() && field_idx < form_fields.size(); ++i) {
    // This field didn't meet the requirements, so don't try to find a label
    // for it.
    if (!fields_extracted[i])
      continue;

    const WebFormControlElement& control_element = control_elements[i];
    if (form_fields[field_idx]->label.empty()) {
      InferLabelForElement(control_element, stop_words,
                           &(form_fields[field_idx]->label),
                           &(form_fields[field_idx]->label_source));
    }
    TruncateString(&form_fields[field_idx]->label, kMaxDataLength);

    if (field && *form_control_element == control_element) {
      *field = *form_fields[field_idx];
      found_field = true;
    }

    ++field_idx;
  }

  // The form_control_element was not found in control_elements. This can
  // happen if elements are dynamically removed from the form while it is
  // being processed. See http://crbug.com/849870
  if (field && !found_field)
    return false;

  // Copy the created FormFields into the resulting FormData object.
  for (const auto& field : form_fields)
    form->fields.push_back(*field);
  return true;
}

bool UnownedFormElementsAndFieldSetsToFormData(
    const std::vector<blink::WebElement>& fieldsets,
    const std::vector<blink::WebFormControlElement>& control_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  form->url = GetCanonicalOriginForDocument(document);
  if (document.GetFrame() && document.GetFrame()->Top()) {
    form->main_frame_origin = document.GetFrame()->Top()->GetSecurityOrigin();
  } else {
    form->main_frame_origin = url::Origin();
  }

  form->is_form_tag = false;

  return FormOrFieldsetsToFormData(nullptr, element, fieldsets,
                                   control_elements, field_data_manager,
                                   extract_mask, form, field);
}

// Check if a script modified username is suitable for Password Manager to
// remember.
bool ScriptModifiedUsernameAcceptable(
    const base::string16& value,
    const base::string16& typed_value,
    const FieldDataManager* field_data_manager) {
  // The minimal size of a field value that will be substring-matched.
  constexpr size_t kMinMatchSize = 3u;
  const auto lowercase = base::i18n::ToLower(value);
  const auto typed_lowercase = base::i18n::ToLower(typed_value);
  // If the page-generated value is just a completion of the typed value, that's
  // likely acceptable.
  if (base::StartsWith(lowercase, typed_lowercase,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  if (typed_lowercase.size() >= kMinMatchSize &&
      lowercase.find(typed_lowercase) != base::string16::npos) {
    return true;
  }

  // If the page-generated value comes from user typed or autofilled values in
  // other fields, that's also likely OK.
  return field_data_manager->FindMachedValue(value);
}

// Trim the vector before sending it to the browser process to ensure we
// don't send too much data through the IPC.
void TrimStringVectorForIPC(std::vector<base::string16>* strings) {
  // Limit the size of the vector.
  if (strings->size() > kMaxListSize)
    strings->resize(kMaxListSize);

  // Limit the size of the strings in the vector.
  for (auto& string : *strings) {
    if (string.length() > kMaxDataLength)
      string.resize(kMaxDataLength);
  }
}

// Helper function that strips any authentication data, as well as query and
// ref portions of URL.
GURL StripAuthAndParams(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

}  // namespace

void GetDataListSuggestions(const WebInputElement& element,
                            std::vector<base::string16>* values,
                            std::vector<base::string16>* labels) {
  for (const auto& option : element.FilteredDataListOptions()) {
    values->push_back(option.Value().Utf16());
    if (option.Value() != option.Label())
      labels->push_back(option.Label().Utf16());
    else
      labels->push_back(base::string16());
  }
  TrimStringVectorForIPC(values);
  TrimStringVectorForIPC(labels);
}

bool ExtractFormData(const WebFormElement& form_element,
                     const FieldDataManager& field_data_manager,
                     FormData* data) {
  return WebFormElementToFormData(
      form_element, WebFormControlElement(), &field_data_manager,
      static_cast<form_util::ExtractMask>(form_util::EXTRACT_VALUE |
                                          form_util::EXTRACT_OPTION_TEXT |
                                          form_util::EXTRACT_OPTIONS),
      data, nullptr);
}

bool IsFormVisible(blink::WebLocalFrame* frame,
                   FormRendererId form_renderer_id) {
  WebDocument doc = frame->GetDocument();
  if (doc.IsNull())
    return false;
  WebFormElement form = FindFormByUniqueRendererId(doc, form_renderer_id);
  return form.IsNull() ? false : AreFormContentsVisible(form);
}

bool IsFormControlVisible(blink::WebLocalFrame* frame,
                          FieldRendererId field_renderer_id) {
  WebDocument doc = frame->GetDocument();
  if (doc.IsNull())
    return false;
  WebFormControlElement field =
      FindFormControlElementByUniqueRendererId(doc, field_renderer_id);
  return field.IsNull() ? false : IsWebElementVisible(field);
}

bool IsSomeControlElementVisible(
    const WebVector<WebFormControlElement>& control_elements) {
  for (const WebFormControlElement& control_element : control_elements) {
    if (IsWebElementVisible(control_element))
      return true;
  }
  return false;
}

bool AreFormContentsVisible(const WebFormElement& form) {
  return IsSomeControlElementVisible(form.GetFormControlElements());
}

GURL GetCanonicalActionForForm(const WebFormElement& form) {
  WebString action = form.Action();
  if (action.IsNull())
    action = WebString("");  // missing 'action' attribute implies current URL.
  GURL full_action(form.GetDocument().CompleteURL(action));
  return StripAuthAndParams(full_action);
}

GURL GetCanonicalOriginForDocument(const WebDocument& document) {
  GURL full_origin(document.Url());
  return StripAuthAndParams(full_origin);
}

GURL GetDocumentUrlWithoutAuth(const WebDocument& document) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  GURL full_origin(document.Url());
  return full_origin.ReplaceComponents(rep);
}

bool IsMonthInput(const WebInputElement* element) {
  static base::NoDestructor<WebString> kMonth("month");
  return element && !element->IsNull() &&
         element->FormControlTypeForAutofill() == *kMonth;
}

// All text fields, including password fields, should be extracted.
bool IsTextInput(const WebInputElement* element) {
  return element && !element->IsNull() && element->IsTextField();
}

bool IsSelectElement(const WebFormControlElement& element) {
  // Static for improved performance.
  static base::NoDestructor<WebString> kSelectOne("select-one");
  return !element.IsNull() &&
         element.FormControlTypeForAutofill() == *kSelectOne;
}

bool IsTextAreaElement(const WebFormControlElement& element) {
  // Static for improved performance.
  static base::NoDestructor<WebString> kTextArea("textarea");
  return !element.IsNull() &&
         element.FormControlTypeForAutofill() == *kTextArea;
}

bool IsCheckableElement(const WebInputElement* element) {
  if (!element || element->IsNull())
    return false;

  return element->IsCheckbox() || element->IsRadioButton();
}

bool IsAutofillableInputElement(const WebInputElement* element) {
  return IsTextInput(element) || IsMonthInput(element) ||
         IsCheckableElement(element);
}

bool IsAutofillableElement(const WebFormControlElement& element) {
  const WebInputElement* input_element = ToWebInputElement(&element);
  return IsAutofillableInputElement(input_element) ||
         IsSelectElement(element) || IsTextAreaElement(element);
}

bool IsWebElementVisible(const blink::WebElement& element) {
  return element.IsFocusable();
}

base::string16 GetFormIdentifier(const WebFormElement& form) {
  base::string16 identifier = form.GetName().Utf16();
  static base::NoDestructor<WebString> kId("id");
  if (identifier.empty())
    identifier = form.GetAttribute(*kId).Utf16();

  return identifier;
}

FormRendererId GetFormRendererId(const blink::WebFormElement& form) {
  return form.IsNull() ? FormRendererId()
                       : FormRendererId(form.UniqueRendererFormId());
}

base::i18n::TextDirection GetTextDirectionForElement(
    const blink::WebFormControlElement& element) {
  // Use 'text-align: left|right' if set or 'direction' otherwise.
  // See https://crbug.com/482339
  base::i18n::TextDirection direction = element.DirectionForFormData() == "rtl"
                                            ? base::i18n::RIGHT_TO_LEFT
                                            : base::i18n::LEFT_TO_RIGHT;
  if (element.AlignmentForFormData() == "left")
    direction = base::i18n::LEFT_TO_RIGHT;
  else if (element.AlignmentForFormData() == "right")
    direction = base::i18n::RIGHT_TO_LEFT;
  return direction;
}

std::vector<blink::WebFormControlElement> ExtractAutofillableElementsFromSet(
    const WebVector<WebFormControlElement>& control_elements) {
  std::vector<blink::WebFormControlElement> autofillable_elements;
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement element = control_elements[i];
    if (!IsAutofillableElement(element))
      continue;

    autofillable_elements.push_back(element);
  }
  return autofillable_elements;
}

std::vector<WebFormControlElement> ExtractAutofillableElementsInForm(
    const WebFormElement& form_element) {
  return ExtractAutofillableElementsFromSet(
      form_element.GetFormControlElements());
}

void WebFormControlElementToFormField(
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormFieldData* field) {
  DCHECK(field);
  DCHECK(!element.IsNull());
  static base::NoDestructor<WebString> kAutocomplete("autocomplete");
  static base::NoDestructor<WebString> kId("id");
  static base::NoDestructor<WebString> kName("name");
  static base::NoDestructor<WebString> kRole("role");
  static base::NoDestructor<WebString> kPlaceholder("placeholder");
  static base::NoDestructor<WebString> kClass("class");

  // Save both id and name attributes, if present. If there is only one of them,
  // it will be saved to |name|. See HTMLFormControlElement::nameForAutofill.
  field->name = element.NameForAutofill().Utf16();
  field->id_attribute = element.GetAttribute(*kId).Utf16();
  field->name_attribute = element.GetAttribute(*kName).Utf16();
  field->unique_renderer_id =
      FieldRendererId(element.UniqueRendererFormControlId());
  field->form_control_ax_id = element.GetAxId();
  field->form_control_type = element.FormControlTypeForAutofill().Utf8();
  field->autocomplete_attribute = element.GetAttribute(*kAutocomplete).Utf8();
  if (field->autocomplete_attribute.size() > kMaxDataLength) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process.  However, send over a default string to indicate that the
    // attribute was present.
    field->autocomplete_attribute = "x-max-data-length-exceeded";
  }
  if (base::LowerCaseEqualsASCII(element.GetAttribute(*kRole).Utf16(),
                                 "presentation"))
    field->role = FormFieldData::RoleAttribute::kPresentation;

  field->placeholder = element.GetAttribute(*kPlaceholder).Utf16();
  if (element.HasAttribute(*kClass))
    field->css_classes = element.GetAttribute(*kClass).Utf16();

  const FieldRendererId renderer_id(element.UniqueRendererFormControlId());
  if (field_data_manager && field_data_manager->HasFieldData(renderer_id)) {
    field->properties_mask =
        field_data_manager->GetFieldPropertiesMask(renderer_id);
  }

  field->aria_label = GetAriaLabel(element.GetDocument(), element);
  field->aria_description = GetAriaDescription(element.GetDocument(), element);

  if (!IsAutofillableElement(element))
    return;

  const WebInputElement* input_element = ToWebInputElement(&element);
  if (IsAutofillableInputElement(input_element) || IsTextAreaElement(element) ||
      IsSelectElement(element)) {
    // The browser doesn't need to differentiate between preview and autofill.
    field->is_autofilled = element.IsAutofilled();
    field->is_focusable = IsWebElementVisible(element);
    field->should_autocomplete = element.AutoComplete();

    field->text_direction = GetTextDirectionForElement(element);
    field->is_enabled = element.IsEnabled();
    field->is_readonly = element.IsReadOnly();
  }

  if (IsAutofillableInputElement(input_element)) {
    if (IsTextInput(input_element))
      field->max_length = input_element->MaxLength();

    SetCheckStatus(field, IsCheckableElement(input_element),
                   input_element->IsChecked());
  } else if (IsTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else if (extract_mask & EXTRACT_OPTIONS) {
    // Set option strings on the field if available.
    DCHECK(IsSelectElement(element));
    const WebSelectElement select_element = element.ToConst<WebSelectElement>();
    GetOptionStringsFromElement(select_element, &field->option_values,
                                &field->option_contents);
  }
  if (extract_mask & EXTRACT_BOUNDS) {
    if (auto* local_frame = element.GetDocument().GetFrame()) {
      if (auto* render_frame =
              content::RenderFrame::FromWebFrame(local_frame)) {
        field->bounds = render_frame->ElementBoundsInWindow(element);
      }
    }
  }
  if (extract_mask & EXTRACT_DATALIST) {
    if (auto* input = blink::ToWebInputElement(&element)) {
      GetDataListSuggestions(*input, &field->datalist_values,
                             &field->datalist_labels);
    }
  }

  if (!(extract_mask & EXTRACT_VALUE))
    return;

  base::string16 value = element.Value().Utf16();

  if (IsSelectElement(element) && (extract_mask & EXTRACT_OPTION_TEXT)) {
    const WebSelectElement select_element = element.ToConst<WebSelectElement>();
    // Convert the |select_element| value to text if requested.
    WebVector<WebElement> list_items = select_element.GetListItems();
    for (size_t i = 0; i < list_items.size(); ++i) {
      if (IsOptionElement(list_items[i])) {
        const WebOptionElement option_element =
            list_items[i].ToConst<WebOptionElement>();
        if (option_element.Value().Utf16() == value) {
          value = option_element.GetText().Utf16();
          break;
        }
      }
    }
  }

  // Constrain the maximum data length to prevent a malicious site from DOS'ing
  // the browser: http://crbug.com/49332
  TruncateString(&value, kMaxDataLength);

  field->value = value;

  // If the field was autofilled or the user typed into it, check the value
  // stored in |field_data_manager| against the value property of the DOM
  // element. If they differ, then the scripts on the website modified the
  // value afterwards. Store the original value as the |typed_value|, unless
  // this is one of recognised situations when the site-modified value is more
  // useful for filling.
  if (field_data_manager &&
      field->properties_mask & (FieldPropertiesFlags::kUserTyped |
                                FieldPropertiesFlags::kAutofilled)) {
    const base::string16 typed_value = field_data_manager->GetUserTypedValue(
        FieldRendererId(element.UniqueRendererFormControlId()));

    // The typed value is preserved for all passwords. It is also preserved for
    // potential usernames, as long as the |value| is not deemed acceptable.
    if (field->form_control_type == "password" ||
        !ScriptModifiedUsernameAcceptable(value, typed_value,
                                          field_data_manager)) {
      field->typed_value = typed_value;
    }
  }
}

bool WebFormElementToFormData(
    const blink::WebFormElement& form_element,
    const blink::WebFormControlElement& form_control_element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  WebLocalFrame* frame = form_element.GetDocument().GetFrame();
  if (!frame)
    return false;

  form->name = GetFormIdentifier(form_element);
  form->unique_renderer_id =
      FormRendererId(form_element.UniqueRendererFormId());
  form->url = GetCanonicalOriginForDocument(frame->GetDocument());
  form->action = GetCanonicalActionForForm(form_element);
  form->is_action_empty =
      form_element.Action().IsNull() || form_element.Action().IsEmpty();
  if (frame->Top()) {
    form->main_frame_origin = frame->Top()->GetSecurityOrigin();
  } else {
    form->main_frame_origin = url::Origin();
    NOTREACHED();
  }
  // If the completed URL is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(blink::WebStringToGURL(form_element.Action()));

  std::vector<blink::WebElement> dummy_fieldset;
  return FormOrFieldsetsToFormData(
      &form_element, &form_control_element, dummy_fieldset,
      form_element.GetFormControlElements(), field_data_manager, extract_mask,
      form, field);
}

std::vector<WebFormControlElement> GetUnownedFormFieldElements(
    const WebElementCollection& elements,
    std::vector<WebElement>* fieldsets) {
  std::vector<WebFormControlElement> unowned_fieldset_children;
  for (WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (element.IsFormControlElement()) {
      WebFormControlElement control = element.To<WebFormControlElement>();
      if (control.Form().IsNull())
        unowned_fieldset_children.push_back(control);
    }

    if (fieldsets && element.HasHTMLTagName("fieldset") &&
        !IsElementInsideFormOrFieldSet(element,
                                       true /* consider_fieldset_tags */)) {
      fieldsets->push_back(element);
    }
  }
  return unowned_fieldset_children;
}

std::vector<WebFormControlElement> GetUnownedAutofillableFormFieldElements(
    const WebElementCollection& elements,
    std::vector<WebElement>* fieldsets) {
  return ExtractAutofillableElementsFromSet(
      GetUnownedFormFieldElements(elements, fieldsets));
}

bool UnownedCheckoutFormElementsAndFieldSetsToFormData(
    const std::vector<blink::WebElement>& fieldsets,
    const std::vector<blink::WebFormControlElement>& control_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillRestrictUnownedFieldsToFormlessCheckout)) {
    return UnownedFormElementsAndFieldSetsToFormData(
        fieldsets, control_elements, element, document, field_data_manager,
        extract_mask, form, field);
  }

  // Only attempt formless Autofill on checkout flows. This avoids the many
  // false positives found on the non-checkout web. See
  // http://crbug.com/462375.
  WebElement html_element = document.DocumentElement();

  // For now this restriction only applies to English-language pages, because
  // the keywords are not translated. Note that an empty "lang" attribute
  // counts as English.
  std::string lang;
  if (!html_element.IsNull())
    lang = html_element.GetAttribute("lang").Utf8();
  if (!lang.empty() &&
      !base::StartsWith(lang, "en", base::CompareCase::INSENSITIVE_ASCII)) {
    return UnownedFormElementsAndFieldSetsToFormData(
        fieldsets, control_elements, element, document, field_data_manager,
        extract_mask, form, field);
  }

  // A potential problem is that this only checks document.title(), but should
  // actually check the main frame's title. Thus it may make bad decisions for
  // iframes.
  base::string16 title(base::ToLowerASCII(document.Title().Utf16()));

  // Don't check the path for url's without a standard format path component,
  // such as data:.
  std::string path;
  GURL url(document.Url());
  if (url.IsStandard())
    path = base::ToLowerASCII(url.path());

  const char* const kKeywords[] = {"payment",  "checkout", "address",
                                   "delivery", "shipping", "wallet"};

  for (const auto* keyword : kKeywords) {
    // Compare char16 elements of |title| with char elements of |keyword| using
    // operator==.
    auto title_pos = std::search(title.begin(), title.end(), keyword,
                                 keyword + strlen(keyword));
    if (title_pos != title.end() || path.find(keyword) != std::string::npos) {
      form->is_formless_checkout = true;
      // Found a keyword: treat this as an unowned form.
      return UnownedFormElementsAndFieldSetsToFormData(
          fieldsets, control_elements, element, document, field_data_manager,
          extract_mask, form, field);
    }
  }

  // Since it's not a checkout flow, only add fields that have a non-"off"
  // autocomplete attribute to the formless autofill.
  static base::NoDestructor<WebString> kOffAttribute("off");
  static base::NoDestructor<WebString> kFalseAttribute("false");
  std::vector<WebFormControlElement> elements_with_autocomplete;
  for (const WebFormControlElement& element : control_elements) {
    blink::WebString autocomplete = element.GetAttribute("autocomplete");
    if (autocomplete.length() && autocomplete != *kOffAttribute &&
        autocomplete != *kFalseAttribute) {
      elements_with_autocomplete.push_back(element);
    }
  }

  // http://crbug.com/841784
  // Capture the number of times this formless checkout logic prevents a from
  // being autofilled (fill logic expects to receive a autofill field entry,
  // possibly not fillable, for each control element).
  // Note: this will be fixed by http://crbug.com/806987
  UMA_HISTOGRAM_BOOLEAN(
      "Autofill.UnownedFieldsWereFiltered",
      elements_with_autocomplete.size() != control_elements.size());

  if (elements_with_autocomplete.empty())
    return false;

  return UnownedFormElementsAndFieldSetsToFormData(
      fieldsets, elements_with_autocomplete, element, document,
      field_data_manager, extract_mask, form, field);
}

bool UnownedPasswordFormElementsAndFieldSetsToFormData(
    const std::vector<blink::WebElement>& fieldsets,
    const std::vector<blink::WebFormControlElement>& control_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  return UnownedFormElementsAndFieldSetsToFormData(
      fieldsets, control_elements, element, document, field_data_manager,
      extract_mask, form, field);
}

bool FindFormAndFieldForFormControlElement(
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  DCHECK(!element.IsNull());

  if (!IsAutofillableElement(element))
    return false;

  extract_mask =
      static_cast<ExtractMask>(EXTRACT_VALUE | EXTRACT_OPTIONS | extract_mask);
  const WebFormElement form_element = element.Form();
  if (form_element.IsNull()) {
    // No associated form, try the synthetic form for unowned form elements.
    WebDocument document = element.GetDocument();
    std::vector<WebElement> fieldsets;
    std::vector<WebFormControlElement> control_elements =
        GetUnownedAutofillableFormFieldElements(document.All(), &fieldsets);
    return UnownedCheckoutFormElementsAndFieldSetsToFormData(
        fieldsets, control_elements, &element, document, field_data_manager,
        extract_mask, form, field);
  }

  return WebFormElementToFormData(form_element, element, field_data_manager,
                                  extract_mask, form, field);
}

bool FindFormAndFieldForFormControlElement(
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    FormData* form,
    FormFieldData* field) {
  return FindFormAndFieldForFormControlElement(
      element, field_data_manager, form_util::EXTRACT_NONE, form, field);
}

void FillForm(const FormData& form, const WebFormControlElement& element) {
  WebFormElement form_element = element.Form();
  if (form_element.IsNull()) {
    ForEachMatchingUnownedFormField(element, form,
                                    FILTER_ALL_NON_EDITABLE_ELEMENTS,
                                    false, /* dont force override */
                                    false, /* not a preview filling */
                                    &FillFormField);
    return;
  }

  ForEachMatchingFormField(form_element, element, form,
                           FILTER_ALL_NON_EDITABLE_ELEMENTS,
                           false, /* dont force override */
                           false, /* not a preview filling */
                           &FillFormField);
}

void PreviewForm(const FormData& form, const WebFormControlElement& element) {
  WebFormElement form_element = element.Form();
  if (form_element.IsNull()) {
    ForEachMatchingUnownedFormField(element, form,
                                    FILTER_ALL_NON_EDITABLE_ELEMENTS,
                                    false, /* dont force override */
                                    true,  /* preview filling */
                                    &PreviewFormField);
    return;
  }

  ForEachMatchingFormField(form_element, element, form,
                           FILTER_ALL_NON_EDITABLE_ELEMENTS,
                           false, /* dont force override */
                           true,  /* preview filling */
                           &PreviewFormField);
}

bool ClearPreviewedFormWithElement(const WebFormControlElement& element,
                                   blink::WebAutofillState old_autofill_state) {
  WebFormElement form_element = element.Form();
  std::vector<WebFormControlElement> control_elements;
  if (form_element.IsNull()) {
    control_elements = GetUnownedAutofillableFormFieldElements(
        element.GetDocument().All(), nullptr);
    if (!IsElementInControlElementSet(element, control_elements))
      return false;
  } else {
    control_elements = ExtractAutofillableElementsInForm(form_element);
  }

  for (size_t i = 0; i < control_elements.size(); ++i) {
    // There might be unrelated elements in this form which have already been
    // auto-filled.  For example, the user might have already filled the address
    // part of a form and now be dealing with the credit card section.  We only
    // want to reset the auto-filled status for fields that were previewed.
    WebFormControlElement control_element = control_elements[i];

    // Only text input, textarea and select elements can be previewed.
    WebInputElement* input_element = ToWebInputElement(&control_element);
    if (!IsTextInput(input_element) && !IsMonthInput(input_element) &&
        !IsTextAreaElement(control_element) &&
        !IsSelectElement(control_element))
      continue;

    // Only clear previewed fields.
    if (control_element.GetAutofillState() != WebAutofillState::kPreviewed)
      continue;

    if ((IsTextInput(input_element) || IsMonthInput(input_element) ||
         IsTextAreaElement(control_element) ||
         IsSelectElement(control_element)) &&
        control_element.SuggestedValue().IsEmpty())
      continue;

    // Clear the suggested value. For the initiating node, also restore the
    // original value.
    if (IsTextInput(input_element) || IsMonthInput(input_element) ||
        IsTextAreaElement(control_element)) {
      control_element.SetSuggestedValue(WebString());
      bool is_initiating_node = (element == control_element);
      if (is_initiating_node) {
        // Clearing the suggested value in the focused node (above) can cause
        // selection to be lost. We force selection range to restore the text
        // cursor.
        int length = control_element.Value().length();
        control_element.SetSelectionRange(length, length);
        control_element.SetAutofillState(old_autofill_state);

      } else {
        control_element.SetAutofillState(WebAutofillState::kNotFilled);
      }
    } else if (IsSelectElement(control_element)) {
      control_element.SetSuggestedValue(WebString());
      control_element.SetAutofillState(WebAutofillState::kNotFilled);
    }
  }

  return true;
}

bool IsWebpageEmpty(const blink::WebLocalFrame* frame) {
  blink::WebDocument document = frame->GetDocument();

  return IsWebElementEmpty(document.Head()) &&
         IsWebElementEmpty(document.Body());
}

bool IsWebElementEmpty(const blink::WebElement& root) {
  static base::NoDestructor<WebString> kScript("script");
  static base::NoDestructor<WebString> kMeta("meta");
  static base::NoDestructor<WebString> kTitle("title");

  if (root.IsNull())
    return true;

  for (WebNode child = root.FirstChild(); !child.IsNull();
       child = child.NextSibling()) {
    if (child.IsTextNode() && !base::ContainsOnlyChars(child.NodeValue().Utf8(),
                                                       base::kWhitespaceASCII))
      return false;

    if (!child.IsElementNode())
      continue;

    WebElement element = child.To<WebElement>();
    if (!element.HasHTMLTagName(*kScript) && !element.HasHTMLTagName(*kMeta) &&
        !element.HasHTMLTagName(*kTitle))
      return false;
  }
  return true;
}

void PreviewSuggestion(const base::string16& suggestion,
                       const base::string16& user_input,
                       blink::WebFormControlElement* input_element) {
  size_t selection_start = user_input.length();
  if (IsFeatureSubstringMatchEnabled()) {
    size_t offset = GetTextSelectionStart(suggestion, user_input, false);
    // Zero selection start is for password manager, which can show usernames
    // that do not begin with the user input value.
    selection_start = (offset == base::string16::npos) ? 0 : offset;
  }

  input_element->SetSelectionRange(selection_start, suggestion.length());
}

base::string16 FindChildText(const WebNode& node) {
  return FindChildTextWithIgnoreList(node, std::set<WebNode>());
}

ButtonTitleList GetButtonTitles(const WebFormElement& web_form,
                                const WebDocument& document,
                                ButtonTitlesCache* button_titles_cache) {
  DCHECK(button_titles_cache);
  if (!IsAutofillFieldMetadataEnabled() && web_form.IsNull())
    return ButtonTitleList();

  // True if the cache has no entry for |web_form|.
  bool cache_miss = true;
  // Iterator pointing to the entry for |web_form| if the entry for |web_form|
  // is found.
  ButtonTitlesCache::iterator form_position;
  std::tie(form_position, cache_miss) = button_titles_cache->emplace(
      GetFormRendererId(web_form), ButtonTitleList());
  if (!cache_miss)
    return form_position->second;

  ButtonTitleList button_titles;
  DCHECK(!web_form.IsNull() || !document.IsNull());
  if (web_form.IsNull()) {
    const WebElement& body = document.Body();
    if (!body.IsNull()) {
      SCOPED_UMA_HISTOGRAM_TIMER(
          "PasswordManager.ButtonTitlePerformance.NoFormTag");
      button_titles = InferButtonTitlesForForm(body);
    }
  } else {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "PasswordManager.ButtonTitlePerformance.HasFormTag");
    button_titles = InferButtonTitlesForForm(web_form);
  }
  form_position->second = std::move(button_titles);
  return form_position->second;
}

base::string16 FindChildTextWithIgnoreListForTesting(
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  return FindChildTextWithIgnoreList(node, divs_to_skip);
}

bool InferLabelForElementForTesting(const WebFormControlElement& element,
                                    const std::vector<base::char16>& stop_words,
                                    base::string16* label,
                                    FormFieldData::LabelSource* label_source) {
  return InferLabelForElement(element, stop_words, label, label_source);
}

WebFormElement FindFormByUniqueRendererId(WebDocument doc,
                                          FormRendererId form_renderer_id) {
  for (const auto& form : doc.Forms()) {
    if (FormRendererId(form.UniqueRendererFormId()) == form_renderer_id)
      return form;
  }
  return WebFormElement();
}

WebFormControlElement FindFormControlElementByUniqueRendererId(
    WebDocument doc,
    FieldRendererId form_control_renderer_id) {
  WebElementCollection elements = doc.All();

  for (WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (!element.IsFormControlElement())
      continue;
    WebFormControlElement control = element.To<WebFormControlElement>();
    if (form_control_renderer_id ==
        FieldRendererId(control.UniqueRendererFormControlId()))
      return control;
  }

  return WebFormControlElement();
}

std::vector<WebFormControlElement> FindFormControlElementsByUniqueRendererId(
    WebDocument doc,
    const std::vector<FieldRendererId>& form_control_renderer_ids) {
  WebElementCollection elements = doc.All();
  std::vector<WebFormControlElement> result(form_control_renderer_ids.size());

  // Build a map from entries in |form_control_renderer_ids| to their indices,
  // for more efficient lookup.
  std::map<FieldRendererId, size_t> renderer_id_to_index;
  for (size_t i = 0; i < form_control_renderer_ids.size(); i++)
    renderer_id_to_index[form_control_renderer_ids[i]] = i;

  for (WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (!element.IsFormControlElement())
      continue;
    WebFormControlElement control = element.To<WebFormControlElement>();
    auto it = renderer_id_to_index.find(
        FieldRendererId(control.UniqueRendererFormControlId()));
    if (it == renderer_id_to_index.end())
      continue;
    result[it->second] = control;
  }

  return result;
}

std::vector<WebFormControlElement> FindFormControlElementsByUniqueRendererId(
    WebDocument doc,
    FormRendererId form_renderer_id,
    const std::vector<FieldRendererId>& form_control_renderer_ids) {
  std::vector<WebFormControlElement> result(form_control_renderer_ids.size());
  WebFormElement form = FindFormByUniqueRendererId(doc, form_renderer_id);
  if (form.IsNull())
    return result;

  // Build a map from entries in |form_control_renderer_ids| to their indices,
  // for more efficient lookup.
  std::map<FieldRendererId, size_t> renderer_id_to_index;
  for (size_t i = 0; i < form_control_renderer_ids.size(); i++)
    renderer_id_to_index[form_control_renderer_ids[i]] = i;

  for (const auto& field : form.GetFormControlElements()) {
    auto it = renderer_id_to_index.find(
        FieldRendererId(field.UniqueRendererFormControlId()));
    if (it == renderer_id_to_index.end())
      continue;
    result[it->second] = field;
  }
  return result;
}

namespace {

// Returns the coalesced child of the elements who's ids are founc in |id_list|.
//
// For example, given this document...
//
//      <div id="billing">Billing</div>
//      <div>
//        <div id="name">Name</div>
//        <input id="field1" type="text" aria-labelledby="billing name"/>
//     </div>
//     <div>
//       <div id="address">Address</div>
//       <input id="field2" type="text" aria-labelledby="billing address"/>
//     </div>
//
// The coalesced text by the id_list found in the aria-labelledby attribute
// of the field1 input element would be "Billing Name" and for field2 it would
// be "Billing Address".
base::string16 CoalesceTextByIdList(const WebDocument& document,
                                    const WebString& id_list) {
  const base::string16 kSpace = base::ASCIIToUTF16(" ");

  base::string16 text;
  base::string16 id_list_utf16 = id_list.Utf16();
  for (const auto& id : base::SplitStringPiece(
           id_list_utf16, base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    auto node = document.GetElementById(WebString(id.data(), id.length()));
    if (!node.IsNull()) {
      base::string16 child_text = FindChildText(node);
      if (!child_text.empty()) {
        if (!text.empty())
          text.append(kSpace);
        text.append(child_text);
      }
    }
  }
  base::TrimWhitespace(text, base::TRIM_ALL, &text);
  return text;
}

}  // namespace

base::string16 GetAriaLabel(const blink::WebDocument& document,
                            const WebFormControlElement& element) {
  static const base::NoDestructor<WebString> kAriaLabelledBy("aria-labelledby");
  if (element.HasAttribute(*kAriaLabelledBy)) {
    base::string16 text =
        CoalesceTextByIdList(document, element.GetAttribute(*kAriaLabelledBy));
    if (!text.empty())
      return text;
  }

  static const base::NoDestructor<WebString> kAriaLabel("aria-label");
  if (element.HasAttribute(*kAriaLabel))
    return element.GetAttribute(*kAriaLabel).Utf16();

  return base::string16();
}

base::string16 GetAriaDescription(const blink::WebDocument& document,
                                  const WebFormControlElement& element) {
  static const base::NoDestructor<WebString> kAriaDescribedBy(
      "aria-describedby");
  return CoalesceTextByIdList(document,
                              element.GetAttribute(*kAriaDescribedBy));
}
}  // namespace form_util
}  // namespace autofill
