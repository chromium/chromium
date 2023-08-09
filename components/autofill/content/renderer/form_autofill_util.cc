// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
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
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_label_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "third_party/blink/public/web/web_select_menu_element.h"
#include "third_party/re2/src/re2/re2.h"

using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLabelElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebSelectElement;
using blink::WebSelectMenuElement;
using blink::WebString;
using blink::WebVector;
using blink::mojom::GenericIssueErrorType;

namespace autofill::form_util {

using ::autofill::mojom::ButtonTitleType;

struct ShadowFieldData {
  ShadowFieldData() = default;
  ShadowFieldData(ShadowFieldData&& other) = default;
  ShadowFieldData& operator=(ShadowFieldData&& other) = default;
  ShadowFieldData(const ShadowFieldData& other) = delete;
  ShadowFieldData& operator=(const ShadowFieldData& other) = delete;
  ~ShadowFieldData() = default;

  // If the form control is inside shadow DOM, then these lists will contain
  // id and name attributes of the parent shadow host elements. There may be
  // more than one if the form control is in nested shadow DOM.
  std::vector<std::u16string> shadow_host_id_attributes;
  std::vector<std::u16string> shadow_host_name_attributes;
};

namespace {

// Maximal length of a button's title.
constexpr int kMaxLengthForSingleButtonTitle = 30;
// Maximal length of all button titles.
constexpr int kMaxLengthForAllButtonTitles = 200;

// Number of shadow roots to traverse upwards when looking for relevant forms
// and labels of an input element inside a shadow root.
constexpr size_t kMaxShadowLevelsUp = 2;

// Text features to detect form submission buttons. Features are selected based
// on analysis of real forms and their buttons.
// TODO(crbug.com/910546): Consider to add more features (e.g. non-English
// features).
const char* const kButtonFeatures[] = {"button", "btn", "submit",
                                       "boton" /* "button" in Spanish */};

// Number of form neighbor nodes to traverse in search of four digit
// combinations on the webpage.
constexpr int kFormNeighborNodesToTraverse = 50;

// Maximum number of consecutive numbers to allow in the four digit combination
// matches.
constexpr int kMaxConsecutiveInFourDigitCombinationMatches = 2;

// Maximum number of four digit combination matches to find in the DOM.
constexpr size_t kMaxFourDigitCombinationMatches = 5;

// Constants to be passed to GetWebString<kConstant>().
constexpr base::StringPiece kAnchor = "a";
constexpr base::StringPiece kAutocomplete = "autocomplete";
constexpr base::StringPiece kAriaDescribedBy = "aria-describedby";
constexpr base::StringPiece kAriaLabel = "aria-label";
constexpr base::StringPiece kAriaLabelledBy = "aria-labelledby";
constexpr base::StringPiece kBold = "b";
constexpr base::StringPiece kBreak = "br";
constexpr base::StringPiece kButton = "button";
constexpr base::StringPiece kClass = "class";
constexpr base::StringPiece kColspan = "colspan";
constexpr base::StringPiece kDefinitionDescriptionTag = "dd";
constexpr base::StringPiece kDefinitionTermTag = "dt";
constexpr base::StringPiece kDiv = "div";
constexpr base::StringPiece kFieldset = "fieldset";
constexpr base::StringPiece kFont = "font";
constexpr base::StringPiece kFor = "for";
constexpr base::StringPiece kForm = "form";
constexpr base::StringPiece kFormControlSelector = "input, select, textarea";
constexpr base::StringPiece kHidden = "hidden";
constexpr base::StringPiece kId = "id";
constexpr base::StringPiece kIframe = "iframe";
constexpr base::StringPiece kImage = "img";
constexpr base::StringPiece kInput = "input";
constexpr base::StringPiece kLabel = "label";
constexpr base::StringPiece kListItem = "li";
constexpr base::StringPiece kMeta = "meta";
constexpr base::StringPiece kName = "name";
constexpr base::StringPiece kNoScript = "noscript";
constexpr base::StringPiece kOption = "option";
constexpr base::StringPiece kParagraph = "p";
constexpr base::StringPiece kPlaceholder = "placeholder";
constexpr base::StringPiece kRole = "role";
constexpr base::StringPiece kScript = "script";
constexpr base::StringPiece kSpan = "span";
constexpr base::StringPiece kStrong = "strong";
constexpr base::StringPiece kSubmit = "submit";
constexpr base::StringPiece kTable = "table";
constexpr base::StringPiece kTableCell = "td";
constexpr base::StringPiece kTableHeader = "th";
constexpr base::StringPiece kTableRow = "tr";
constexpr base::StringPiece kTitle = "title";
constexpr base::StringPiece kType = "type";
constexpr base::StringPiece kValue = "value";

// Wrapper for frequently used WebString constants.
template <const base::StringPiece& string>
const WebString& GetWebString() {
  static const base::NoDestructor<WebString> web_string(
      WebString::FromUTF8(string.data(), string.length()));
  return *web_string;
}

template <const base::StringPiece& tag_name>
bool HasTagName(const WebElement& element) {
  return element.HasHTMLTagName(GetWebString<tag_name>());
}

template <const base::StringPiece& tag_name>
bool HasTagName(const WebNode& node) {
  return node.IsElementNode() && HasTagName<tag_name>(node.To<WebElement>());
}

template <const base::StringPiece& attribute>
bool HasAttribute(const WebElement& element) {
  return element.HasAttribute(GetWebString<attribute>());
}

template <const base::StringPiece& attribute>
WebString GetAttribute(const WebElement& element) {
  return element.GetAttribute(GetWebString<attribute>());
}

bool IsElementInControlElementSet(
    const WebElement& element,
    const std::vector<WebFormControlElement>& control_elements) {
  if (!element.IsFormControlElement())
    return false;
  const WebFormControlElement form_control_element =
      element.To<WebFormControlElement>();
  return base::Contains(control_elements, form_control_element);
}

// Returns true if |node| is an element and it is a container type that
// InferLabelForElement() can traverse.
bool IsTraversableContainerElement(const WebNode& node) {
  if (!node.IsElementNode())
    return false;

  const WebElement element = node.To<WebElement>();
  return HasTagName<kDefinitionDescriptionTag>(element) ||
         HasTagName<kDiv>(element) || HasTagName<kFieldset>(element) ||
         HasTagName<kListItem>(element) || HasTagName<kTableCell>(element) ||
         HasTagName<kTable>(element);
}

// Returns the colspan for a <td> / <th>. Defaults to 1.
size_t CalculateTableCellColumnSpan(const WebElement& element) {
  DCHECK(HasTagName<kTableCell>(element) || HasTagName<kTableHeader>(element));

  size_t span = 1;
  if (HasAttribute<kColspan>(element)) {
    std::u16string colspan = GetAttribute<kColspan>(element).Utf16();
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
const std::u16string CombineAndCollapseWhitespace(const std::u16string& prefix,
                                                  const std::u16string& suffix,
                                                  bool force_whitespace) {
  std::u16string prefix_trimmed;
  base::TrimPositions prefix_trailing_whitespace =
      base::TrimWhitespace(prefix, base::TRIM_TRAILING, &prefix_trimmed);

  // Recursively compute the children's text.
  std::u16string suffix_trimmed;
  base::TrimPositions suffix_leading_whitespace =
      base::TrimWhitespace(suffix, base::TRIM_LEADING, &suffix_trimmed);

  if (prefix_trailing_whitespace || suffix_leading_whitespace ||
      force_whitespace) {
    return prefix_trimmed + u" " + suffix_trimmed;
  }
  return prefix_trimmed + suffix_trimmed;
}

// This is a helper function for the FindChildText() function (see below).
// Search depth is limited with the |depth| parameter.
// |divs_to_skip| is a list of <div> tags to ignore if encountered.
std::u16string FindChildTextInner(const WebNode& node,
                                  int depth,
                                  const std::set<WebNode>& divs_to_skip) {
  if (depth <= 0 || node.IsNull())
    return std::u16string();

  // Skip over comments.
  if (node.IsCommentNode())
    return FindChildTextInner(node.NextSibling(), depth - 1, divs_to_skip);

  if (!node.IsElementNode() && !node.IsTextNode())
    return std::u16string();

  // Ignore elements known not to contain inferable labels.
  bool skip_node = false;
  if (node.IsElementNode()) {
    const WebElement element = node.To<WebElement>();
    if (HasTagName<kOption>(element) ||
        (HasTagName<kDiv>(element) && base::Contains(divs_to_skip, node)) ||
        (element.IsFormControlElement() &&
         IsAutofillableElement(element.To<WebFormControlElement>()))) {
      return std::u16string();
    }
    skip_node = HasTagName<kScript>(element) || HasTagName<kNoScript>(element);
  }

  std::u16string node_text;

  if (!skip_node) {
    // Extract the text exactly at this node.
    node_text = node.NodeValue().Utf16();

    // Recursively compute the children's text.
    // Preserve inter-element whitespace separation.
    std::u16string child_text =
        FindChildTextInner(node.FirstChild(), depth - 1, divs_to_skip);
    bool add_space = node.IsTextNode() && node_text.empty();
    node_text = CombineAndCollapseWhitespace(node_text, child_text, add_space);
  }

  // Recursively compute the siblings' text.
  // Again, preserve inter-element whitespace separation.
  std::u16string sibling_text =
      FindChildTextInner(node.NextSibling(), depth - 1, divs_to_skip);
  bool add_space = node.IsTextNode() && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, sibling_text, add_space);

  return node_text;
}

// Same as FindChildText() below, but with a list of div nodes to skip.
std::u16string FindChildTextWithIgnoreList(
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  if (node.IsTextNode())
    return node.NodeValue().Utf16();

  WebNode child = node.FirstChild();

  const int kChildSearchDepth = 10;
  std::u16string node_text =
      FindChildTextInner(child, kChildSearchDepth, divs_to_skip);
  base::TrimWhitespace(node_text, base::TRIM_ALL, &node_text);
  return node_text;
}

bool IsLabelValid(base::StringPiece16 inferred_label) {
  // List of characters a label can't be entirely made of (this list can grow).
  const base::StringPiece16 invalid_chars =
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u" *:"
          : u" *:-–()";  // U+2013 dash
  return !base::ranges::all_of(inferred_label, [&](char16_t c) {
    return base::Contains(invalid_chars, c);
  });
}

// Shared function for InferLabelFromPrevious() and InferLabelFromNext().
bool InferLabelFromSibling(const WebFormControlElement& element,
                           bool forward,
                           std::u16string& label,
                           FormFieldData::LabelSource& label_source) {
  std::u16string inferred_label;
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
    if (sibling.IsTextNode() || HasTagName<kBold>(sibling) ||
        HasTagName<kStrong>(sibling) || HasTagName<kSpan>(sibling) ||
        HasTagName<kFont>(sibling)) {
      std::u16string value = FindChildText(sibling);
      // A text node's value will be empty if it is for a line break.
      bool add_space = sibling.IsTextNode() && value.empty();
      inferred_label_source = FormFieldData::LabelSource::kCombined;
      if (forward) {
        inferred_label =
            CombineAndCollapseWhitespace(inferred_label, value, add_space);
      } else {
        inferred_label =
            CombineAndCollapseWhitespace(value, inferred_label, add_space);
      }
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    std::u16string trimmed_label;
    base::TrimWhitespace(inferred_label, base::TRIM_ALL, &trimmed_label);
    if (!trimmed_label.empty()) {
      inferred_label_source = FormFieldData::LabelSource::kCombined;
      break;
    }

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    if (HasTagName<kImage>(sibling) || HasTagName<kBreak>(sibling)) {
      continue;
    }

    // We only expect <p> and <label> tags to contain the full label text.
    bool has_label_tag = HasTagName<kLabel>(sibling);
    if (HasTagName<kParagraph>(sibling) || has_label_tag) {
      inferred_label = FindChildText(sibling);
      inferred_label_source = has_label_tag
                                  ? FormFieldData::LabelSource::kLabelTag
                                  : FormFieldData::LabelSource::kPTag;
    }

    break;
  }

  base::TrimWhitespace(inferred_label, base::TRIM_ALL, &inferred_label);
  if (IsLabelValid(inferred_label)) {
    label = std::move(inferred_label);
    label_source = inferred_label_source;
    return true;
  }
  return false;
}

// Helper function to add a button's |title| to the |list|.
void AddButtonTitleToList(std::u16string title,
                          ButtonTitleType button_type,
                          ButtonTitleList* list) {
  title = base::CollapseWhitespace(std::move(title), false);
  if (title.empty()) {
    return;
  }
  list->emplace_back(std::move(title).substr(0, kMaxLengthForSingleButtonTitle),
                     button_type);
}

// Returns true iff |attribute| contains one of |kButtonFeatures|.
bool AttributeHasButtonFeature(const WebString& attribute) {
  if (attribute.IsNull())
    return false;
  std::string value = attribute.Utf8();
  base::ranges::transform(value, value.begin(), ::tolower);
  for (const char* const button_feature : kButtonFeatures) {
    if (value.find(button_feature, 0) != std::string::npos)
      return true;
  }
  return false;
}

// Returns true if |element|'s id, name or css class contain |kButtonFeatures|.
bool ElementAttributesHasButtonFeature(const WebElement& element) {
  return AttributeHasButtonFeature(GetAttribute<kId>(element)) ||
         AttributeHasButtonFeature(GetAttribute<kName>(element)) ||
         AttributeHasButtonFeature(GetAttribute<kClass>(element));
}

// Finds elements from |elements| that contains |kButtonFeatures| and appends it
// to the |list|. If |extract_value_attribute|, the "value" attribute is
// extracted as a button title. Otherwise, |WebElement::TextContent| (aka
// innerText in Javascript) is extracted as a title.
void FindElementsWithButtonFeatures(const WebElementCollection& elements,
                                    ButtonTitleType button_type,
                                    bool extract_value_attribute,
                                    ButtonTitleList* list) {
  for (WebElement item = elements.FirstItem(); !item.IsNull();
       item = elements.NextItem()) {
    if (!ElementAttributesHasButtonFeature(item))
      continue;
    std::u16string title =
        extract_value_attribute
            ? (HasAttribute<kValue>(item) ? GetAttribute<kValue>(item).Utf16()
                                          : std::u16string())
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
                            std::u16string& label,
                            FormFieldData::LabelSource& label_source) {
  return InferLabelFromSibling(element, /*forward=*/false, label, label_source);
}

// Same as InferLabelFromPrevious(), but in the other direction.
// Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
bool InferLabelFromNext(const WebFormControlElement& element,
                        std::u16string& label,
                        FormFieldData::LabelSource& label_source) {
  return InferLabelFromSibling(element, /*forward=*/true, label, label_source);
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// the placeholder text. e.g. <input placeholder="foo">
std::u16string InferLabelFromPlaceholder(const WebFormControlElement& element) {
  if (HasAttribute<kPlaceholder>(element)) {
    return GetAttribute<kPlaceholder>(element).Utf16();
  }

  return std::u16string();
}

// Detects a label declared after the `element`, which is visually positioned
// above the element (usually using CSS). Such labels often act as
// placeholders. E.g.
// <div>
//  <input>
//  <span>Placeholder</span>
// </div>
std::u16string InferLabelFromOverlayingSuccessor(
    const WebFormControlElement& element) {
  WebNode next = element.NextSibling();
  while (!next.IsNull() && !next.IsElementNode())
    next = next.NextSibling();
  if (!next.IsNull()) {
    gfx::Rect bounds = next.To<WebElement>().BoundsInWidget();
    if (!bounds.IsEmpty() && element.BoundsInWidget().Contains(bounds))
      return FindChildText(next);
  }
  return std::u16string();
}

// Helper for |InferLabelForElement()| that infers a label, from
// the value attribute when it is present and user has not typed in (if
// element's value attribute is same as the element's value).
std::u16string InferLabelFromValueAttr(const WebFormControlElement& element) {
  if (HasAttribute<kValue>(element) &&
      GetAttribute<kValue>(element) == element.Value()) {
    return GetAttribute<kValue>(element).Utf16();
  }

  return std::u16string();
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td><td><input ...></td></tr>
// or   <tr><th>Some Text</th><td><input ...></td></tr>
// or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
// or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
// `cell` represents the <td> tag containing the input element.
std::u16string InferLabelFromTableColumn(const WebNode& cell) {
  DCHECK(HasTagName<kTableCell>(cell));
  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  std::u16string inferred_label;
  WebNode previous = cell.PreviousSibling();
  while (inferred_label.empty() && !previous.IsNull()) {
    if (HasTagName<kTableCell>(previous) ||
        HasTagName<kTableHeader>(previous)) {
      inferred_label = FindChildText(previous);
    }

    previous = previous.PreviousSibling();
  }

  return inferred_label;
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// surrounding table structure.
//
// If there are multiple cells and the row with the input matches up with the
// previous row, then look for a specific cell within the previous row.
// e.g. <tr><td>Input 1 label</td><td>Input 2 label</td></tr>
//      <tr><td><input name="input 1"></td><td><input name="input2"></td></tr>
//
// Otherwise, just look in the entire previous row.
// e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
// `cell` represents the <td> tag containing the input element.
std::u16string InferLabelFromTableRow(const WebNode& cell) {
  DCHECK(HasTagName<kTableCell>(cell));
  std::u16string inferred_label;

  // Count the cell holding the input element.
  size_t cell_count = CalculateTableCellColumnSpan(cell.To<WebElement>());
  size_t cell_position = 0;
  size_t cell_position_end = cell_count - 1;

  // Count cells to the left to figure out |element|'s cell's position.
  for (WebNode cell_it = cell.PreviousSibling(); !cell_it.IsNull();
       cell_it = cell_it.PreviousSibling()) {
    if (HasTagName<kTableCell>(cell_it)) {
      cell_position += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Count cells to the right.
  for (WebNode cell_it = cell.NextSibling(); !cell_it.IsNull();
       cell_it = cell_it.NextSibling()) {
    if (HasTagName<kTableCell>(cell_it)) {
      cell_count += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Combine left + right.
  cell_count += cell_position;
  cell_position_end += cell_position;

  // Find the current row.
  WebNode parent = cell.ParentNode();
  while (!parent.IsNull() && !HasTagName<kTableRow>(parent)) {
    parent = parent.ParentNode();
  }
  if (parent.IsNull())
    return inferred_label;

  // Now find the previous row.
  WebNode row_it = parent.PreviousSibling();
  while (!row_it.IsNull() && !HasTagName<kTableRow>(row_it)) {
    row_it = row_it.PreviousSibling();
  }

  // If there exists a previous row, check its cells and size. If they align
  // with the current row, infer the label from the cell above.
  if (!row_it.IsNull()) {
    WebNode matching_cell;
    size_t prev_row_count = 0;
    WebNode prev_row_it = row_it.FirstChild();
    while (!prev_row_it.IsNull()) {
      if (prev_row_it.IsElementNode()) {
        WebElement prev_row_element = prev_row_it.To<WebElement>();
        if (prev_row_element.HasHTMLTagName(GetWebString<kTableCell>()) ||
            prev_row_element.HasHTMLTagName(GetWebString<kTableHeader>())) {
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
    if (HasTagName<kTableRow>(previous)) {
      inferred_label = FindChildText(previous);
    }
    previous = previous.PreviousSibling();
  }

  return inferred_label;
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// a surrounding div table,
// e.g. <div>Some Text<span><input ...></span></div>
// e.g. <div>Some Text</div><div><input ...></div>
//
// Contrary to the other InferLabelFrom* functions, this functions walks up
// the DOM tree from the original input, instead of down from the surrounding
// tag. While doing so, if a <label> or text node sibling are found along the
// way, a label is inferred from them directly. For example, <div>First
// name<div><input></div>Last name<div><input></div></div> infers "First name"
// and "Last name" for the two inputs, respectively, by picking up the text
// nodes on the way to the surrounding div. Without doing so, the label of both
// inputs becomes "First nameLast name".
std::u16string InferLabelFromDivTable(const WebFormControlElement& element) {
  WebNode node = element.ParentNode();
  bool looking_for_parent = true;
  std::set<WebNode> divs_to_skip;

  // Search the sibling and parent <div>s until we find a candidate label.
  std::u16string inferred_label;
  while (inferred_label.empty() && !node.IsNull()) {
    if (HasTagName<kDiv>(node)) {
      if (looking_for_parent)
        inferred_label = FindChildTextWithIgnoreList(node, divs_to_skip);
      else
        inferred_label = FindChildText(node);

      // Avoid sibling DIVs that contain autofillable fields.
      if (!looking_for_parent && !inferred_label.empty()) {
        WebElement result_element =
            node.QuerySelector(GetWebString<kFormControlSelector>());
        if (!result_element.IsNull()) {
          inferred_label.clear();
          divs_to_skip.insert(node);
        }
      }

      looking_for_parent = false;
    } else if (!looking_for_parent) {
      // Infer a label from text nodes and unassigned <label> siblings.
      if (HasTagName<kLabel>(node) &&
          node.To<WebLabelElement>().CorrespondingControl().IsNull()) {
        inferred_label = FindChildText(node);
      } else if (node.IsTextNode()) {
        // TODO(crbug.com/796918): Ideally `FindChildText()` should be used
        // here as well. But because the function doesn't trim it's return
        // value on every code path, the `NodeValue()` is explicitly extracted
        // here. Trimming is necessary to skip indentation.
        inferred_label = node.NodeValue().Utf16();
        base::TrimWhitespace(inferred_label, base::TrimPositions::TRIM_ALL,
                             &inferred_label);
      }
    } else if (IsTraversableContainerElement(node)) {
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

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// a surrounding definition list,
// e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
// e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
std::u16string InferLabelFromDefinitionList(const WebNode& dd) {
  DCHECK(HasTagName<kDefinitionDescriptionTag>(dd));

  // Skip by any intervening text nodes.
  WebNode previous = dd.PreviousSibling();
  while (!previous.IsNull() && previous.IsTextNode())
    previous = previous.PreviousSibling();

  if (previous.IsNull() || !HasTagName<kDefinitionTermTag>(previous)) {
    return std::u16string();
  }
  return FindChildText(previous);
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// the first surrounding <label>, <div>, <td>, <dd> or <li> tag (if any).
// See `FindChildText()`, `InferLabelFromDivTable()`,
// `InferLabelFromTableColumn()`, `InferLabelFromTableRow()` and
// `InferLabelFromDefinitionList()` for examples how a label is extracted from
// the different tags.
bool InferLabelFromAncestors(const WebFormControlElement& element,
                             std::u16string& label,
                             FormFieldData::LabelSource& label_source) {
  std::set<std::string> seen_tag_names;
  for (WebNode parent = element.ParentNode(); !parent.IsNull();
       parent = parent.ParentNode()) {
    if (!parent.IsElementNode())
      continue;

    std::string tag_name = parent.To<WebElement>().TagName().Utf8();
    if (base::Contains(seen_tag_names, tag_name))
      continue;
    seen_tag_names.insert(tag_name);

    FormFieldData::LabelSource ancestor_label_source;
    std::u16string inferred_label;
    if (tag_name == "LABEL") {
      ancestor_label_source = FormFieldData::LabelSource::kLabelTag;
      inferred_label = FindChildText(parent);
    } else if (tag_name == "DIV") {
      ancestor_label_source = FormFieldData::LabelSource::kDivTable;
      inferred_label = InferLabelFromDivTable(element);
    } else if (tag_name == "TD") {
      ancestor_label_source = FormFieldData::LabelSource::kTdTag;
      inferred_label = InferLabelFromTableColumn(parent);
      if (!IsLabelValid(inferred_label))
        inferred_label = InferLabelFromTableRow(parent);
    } else if (tag_name == "DD") {
      ancestor_label_source = FormFieldData::LabelSource::kDdTag;
      inferred_label = InferLabelFromDefinitionList(parent);
    } else if (tag_name == "LI") {
      ancestor_label_source = FormFieldData::LabelSource::kLiTag;
      inferred_label = FindChildText(parent);
    } else if (tag_name == "FIELDSET") {
      break;
    }

    if (IsLabelValid(inferred_label)) {
      label_source = ancestor_label_source;
      label = std::move(inferred_label);
      return true;
    }
  }
  return false;
}

// Infers corresponding label for `element` from surrounding context in the DOM,
// e.g. the contents of the preceding <p> tag or text element.
bool InferLabelForElement(const WebFormControlElement& element,
                          std::u16string& label,
                          FormFieldData::LabelSource& label_source) {
  if (IsCheckableElement(element)) {
    if (InferLabelFromNext(element, label, label_source))
      return true;
  }

  if (InferLabelFromPrevious(element, label, label_source))
    return true;

  if (!base::FeatureList::IsEnabled(
          features::kAutofillAlwaysParsePlaceholders)) {
    std::u16string inferred_label = InferLabelFromPlaceholder(element);
    if (IsLabelValid(inferred_label)) {
      label_source = FormFieldData::LabelSource::kPlaceHolder;
      label = std::move(inferred_label);
      return true;
    }
  }

  std::u16string inferred_label = InferLabelFromOverlayingSuccessor(element);
  if (IsLabelValid(inferred_label)) {
    label_source = FormFieldData::LabelSource::kOverlayingLabel;
    label = std::move(inferred_label);
    return true;
  }

  // If we didn't find a placeholder, check for aria-label text.
  inferred_label = GetAriaLabel(element.GetDocument(), element);
  if (IsLabelValid(inferred_label)) {
    label_source = FormFieldData::LabelSource::kAriaLabel;
    label = std::move(inferred_label);
    return true;
  }

  // If we didn't find a label, check the `element`'s ancestors.
  if (InferLabelFromAncestors(element, label, label_source))
    return true;

  // If we didn't find a label, check the value attr used as the placeholder.
  inferred_label = InferLabelFromValueAttr(element);
  if (IsLabelValid(inferred_label)) {
    label_source = FormFieldData::LabelSource::kValue;
    label = std::move(inferred_label);
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
      title.first = std::move(title.first).substr(0, new_length);
    }
    unique_titles.push_back(std::move(title));

    if (total_length >= kMaxLengthForAllButtonTitles) {
      break;
    }
  }
  *result = std::move(unique_titles);
}

// Return button titles with highest priority based on credibility of their HTML
// tags and attributes.
ButtonTitleList InferButtonTitlesForForm(const WebFormElement& web_form) {
  // Different button types have different credibility of being the main button.
  // Highest - <input type='submit'>, <button type='submit'>, <button>.
  // Moderate - <input type='button'> <button type='button'>.
  // Least - <a>, <div>. <span> with attributes having button features.
  ButtonTitleList highest_priority_buttons;
  ButtonTitleList moderate_priority_buttons;

  WebElementCollection input_elements =
      web_form.GetElementsByHTMLTagName(GetWebString<kInput>());
  for (WebElement item = input_elements.FirstItem(); !item.IsNull();
       item = input_elements.NextItem()) {
    DCHECK(item.IsFormControlElement());
    WebFormControlElement control_element = item.To<WebFormControlElement>();
    bool is_submit_type =
        control_element.FormControlTypeForAutofill() == GetWebString<kSubmit>();
    bool is_button_type =
        control_element.FormControlTypeForAutofill() == GetWebString<kButton>();
    if (!is_submit_type && !is_button_type) {
      continue;
    }
    std::u16string title = control_element.Value().Utf16();
    AddButtonTitleToList(std::move(title),
                         is_submit_type
                             ? ButtonTitleType::INPUT_ELEMENT_SUBMIT_TYPE
                             : ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE,
                         is_submit_type ? &highest_priority_buttons
                                        : &moderate_priority_buttons);
  }

  WebElementCollection button_elements =
      web_form.GetElementsByHTMLTagName(GetWebString<kButton>());
  for (WebElement item = button_elements.FirstItem(); !item.IsNull();
       item = button_elements.NextItem()) {
    const WebString& type_attribute = GetAttribute<kType>(item);
    if (!type_attribute.IsNull() && type_attribute != GetWebString<kButton>() &&
        type_attribute != GetWebString<kSubmit>()) {
      // Neither type='submit' nor type='button'. Skip this button.
      continue;
    }

    bool is_submit_type =
        type_attribute.IsNull() || type_attribute == GetWebString<kSubmit>();
    std::u16string title = item.TextContent().Utf16();
    AddButtonTitleToList(std::move(title),
                         is_submit_type
                             ? ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE
                             : ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE,
                         is_submit_type ? &highest_priority_buttons
                                        : &moderate_priority_buttons);
  }

  if (!highest_priority_buttons.empty()) {
    RemoveDuplicatesAndLimitTotalLength(&highest_priority_buttons);
    return highest_priority_buttons;
  }
  if (!moderate_priority_buttons.empty()) {
    RemoveDuplicatesAndLimitTotalLength(&moderate_priority_buttons);
    return moderate_priority_buttons;
  }

  ButtonTitleList least_priority_buttons;
  FindElementsWithButtonFeatures(
      web_form.GetElementsByHTMLTagName(GetWebString<kAnchor>()),
      ButtonTitleType::HYPERLINK,
      /*extract_value_attribute=*/true, &least_priority_buttons);
  FindElementsWithButtonFeatures(
      web_form.GetElementsByHTMLTagName(GetWebString<kDiv>()),
      ButtonTitleType::DIV,
      /*extract_value_attribute=*/false, &least_priority_buttons);
  FindElementsWithButtonFeatures(
      web_form.GetElementsByHTMLTagName(GetWebString<kSpan>()),
      ButtonTitleType::SPAN,
      /*extract_value_attribute=*/false, &least_priority_buttons);
  RemoveDuplicatesAndLimitTotalLength(&least_priority_buttons);
  return least_priority_buttons;
}

// Returns the list items for the passed-in <select> or <selectmenu>.
WebVector<WebElement> GetListItemsForSelectOrSelectMenu(
    const WebFormControlElement& element) {
  if (IsSelectElement(element)) {
    return element.To<WebSelectElement>().GetListItems();
  } else {
    DCHECK(IsSelectMenuElement(element));
    return element.To<WebSelectMenuElement>().GetListItems();
  }
}

// Fills `options` with the <option> elements present in `option_elements`.
void FilterOptionElementsAndGetOptionStrings(
    const WebVector<WebElement>& option_elements,
    std::vector<SelectOption>* options) {
  options->clear();

  // Constrain the maximum list length to prevent a malicious site from DOS'ing
  // the browser, without entirely breaking autocomplete for some extreme
  // legitimate sites: http://crbug.com/49332 and http://crbug.com/363094
  if (option_elements.size() > kMaxListSize) {
    return;
  }

  options->reserve(option_elements.size());
  for (const auto& option_element : option_elements) {
    if (HasTagName<kOption>(option_element)) {
      const WebOptionElement option = option_element.To<WebOptionElement>();
      options->push_back(
          {.value = option.Value().Utf16().substr(0, kMaxStringLength),
           .content = option.GetText().Utf16().substr(0, kMaxStringLength)});
    }
  }
}

// Use insertion sort to sort the almost sorted |elements|.
void SortByFieldRendererIds(std::vector<WebFormControlElement>& elements) {
  for (auto it = elements.begin(); it != elements.end(); it++) {
    // insertion_point will point to the first element that is greater than
    // to_be_inserted.
    const auto& to_be_inserted = *it;
    const auto insertion_point = base::ranges::upper_bound(
        elements.begin(), it, to_be_inserted.UniqueRendererFormControlId(),
        base::ranges::less{},
        &WebFormControlElement::UniqueRendererFormControlId);

    // Shift all elements from [insertion_point, it) right and move |it| to the
    // front.
    std::rotate(insertion_point, it, it + 1);
  }
}

std::vector<WebFormControlElement>::iterator SearchInSortedVector(
    const FormFieldData& field,
    std::vector<WebFormControlElement>& sorted_elements) {
  auto get_field_renderer_id = [](const WebFormControlElement& e) {
    return GetFieldRendererId(e);
  };
  // Find the first element whose unique renderer ID is greater or equal to
  // |fields|.
  auto it = base::ranges::lower_bound(
      sorted_elements.begin(), sorted_elements.end(), field.unique_renderer_id,
      base::ranges::less{}, get_field_renderer_id);
  if (it == sorted_elements.end() ||
      GetFieldRendererId(*it) != field.unique_renderer_id) {
    return sorted_elements.end();
  }
  return it;
}

bool ShouldSkipFillField(const FormFieldData& field,
                         const WebFormControlElement& element,
                         const WebFormControlElement& trigger_element) {
  // Skip all non-modifiable elements, except select fields because some
  // synthetic select element use a hidden select element.
  if (!element.IsEnabled() || element.IsReadOnly() ||
      (!IsWebElementFocusableForAutofill(element) &&
       !IsSelectElement(element))) {
    return true;
  }

  if (element == trigger_element) {
    return false;
  }

  // Skip if there is no value to fill.
  if (field.value.empty() || !field.is_autofilled) {
    return true;
  }

  // Skip filling previously autofilled fields unless autofill is instructed to
  // override it.
  if (element.GetAutofillState() == WebAutofillState::kAutofilled &&
      !field.force_override) {
    return true;
  }

  // A text field is skipped if it has a non-empty value that is entered by
  // the user and is NOT the value of the input field's "value" or "placeholder"
  // attribute. (The "value" attribute in <input value="foo"> indicates the
  // value of the input element at loading time, not its runtime value after the
  // user entered something into the field.)
  //
  // Some sites fill the fields with a formatting string like (___)-___-____.
  // To tell the difference between the values entered by the user nd the site,
  // we'll sanitize the value. If the sanitized value is empty, it means that
  // the site has filled the field, in this case, the field is not skipped.
  // Nevertheless the below condition does not hold for sites set the |kValue|
  // attribute to the user-input value.
  auto HasAttributeWithValue = [&element](const auto& attribute,
                                          const auto& value) {
    return element.HasAttribute(attribute) &&
           base::i18n::ToLower(element.GetAttribute(attribute).Utf16()) ==
               base::i18n::ToLower(value);
  };
  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  const std::u16string current_element_value = element.Value().Utf16();
  if ((IsAutofillableInputElement(input_element) ||
       IsTextAreaElement(element)) &&
      element.UserHasEditedTheField() &&
      !SanitizedFieldIsEmpty(current_element_value) && !field.force_override &&
      !HasAttributeWithValue(GetWebString<kValue>(), current_element_value) &&
      !HasAttributeWithValue(GetWebString<kPlaceholder>(),
                             current_element_value)) {
    return true;
  }

  // Check if we should autofill/preview/clear a select element or leave it.
  if (IsSelectOrSelectMenuElement(element) && element.UserHasEditedTheField() &&
      !SanitizedFieldIsEmpty(current_element_value) && !field.force_override) {
    return true;
  }

  return false;
}

// Sets the |field|'s value to the value in |data|, and specifies the section
// for filled fields.  Also sets the "autofilled" attribute,
// causing the background to be blue.
void FillFormField(const FormFieldData& data,
                   bool is_initiating_node,
                   blink::WebFormControlElement* field) {
  WebInputElement input_element = field->DynamicTo<WebInputElement>();

  if (IsCheckableElement(input_element)) {
    input_element.SetChecked(IsChecked(data.check_status), true,
                             WebAutofillState::kAutofilled);
  } else {
    std::u16string value = data.value;
    if (IsTextInput(input_element) || IsMonthInput(input_element)) {
      // If the maxlength attribute contains a negative value, maxLength()
      // returns the default maxlength value.
      value = std::move(value).substr(0, input_element.MaxLength());
    }
    field->SetAutofillValue(blink::WebString::FromUTF16(value),
                            WebAutofillState::kAutofilled);
  }
  // Changing the field's value might trigger JavaScript, which is capable of
  // destroying the frame
  if (!field->GetDocument().GetFrame()) {
    return;
  }

  if (is_initiating_node &&
      ((IsTextInput(input_element) || IsMonthInput(input_element)) ||
       IsTextAreaElement(*field))) {
    int length = field->Value().length();
    field->SetSelectionRange(length, length);
    // selectionchange event is capable of destroying the frame.
    if (!field->GetDocument().GetFrame()) {
      return;
    }
    // Clear the current IME composition (the underline), if there is one.
    field->GetDocument().GetFrame()->UnmarkText();
  }
}

// Sets the value of `field` to the value in `data`, and resets the "autofilled"
// attribute. `is_initiating_node` indicates whether the current field processed
// is the one where Undo was triggered.
void UndoFormField(const FormFieldData& data,
                   bool is_initiating_node,
                   blink::WebFormControlElement* field) {
  WebInputElement input_element = field->DynamicTo<WebInputElement>();
  // If a field is not currently autofilled in blink there's no current autofill
  // value to undo. Autofill currently has no support for checkable elements.
  if (field->GetAutofillState() != blink::WebAutofillState::kAutofilled ||
      IsCheckableElement(input_element)) {
    return;
  }

  std::u16string value = data.value;
  if (IsTextInput(input_element) || IsMonthInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    value = std::move(value).substr(0, input_element.MaxLength());
  }
  field->SetAutofillValue(blink::WebString::FromUTF16(value),
                          data.is_autofilled ? WebAutofillState::kAutofilled
                                             : WebAutofillState::kNotFilled);
  // Changing the field's value might trigger JavaScript, which is capable of
  // destroying the frame.
  if (!field->GetDocument().GetFrame()) {
    return;
  }

  if (is_initiating_node &&
      ((IsTextInput(input_element) || IsMonthInput(input_element)) ||
       IsTextAreaElement(*field))) {
    int length = field->Value().length();
    field->SetSelectionRange(length, length);
    // selectionchange event is capable of destroying the frame.
    if (!field->GetDocument().GetFrame()) {
      return;
    }
    // Clear the current IME composition (the underline), if there is one.
    field->GetDocument().GetFrame()->UnmarkText();
  }
}

// Sets the |field|'s "suggested" (non JS visible) value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void PreviewFormField(const FormFieldData& data,
                      bool is_initiating_node,
                      blink::WebFormControlElement* field) {
  // Preview input, textarea and select fields. For input fields, excludes
  // checkboxes and radio buttons, as there is no provision for
  // setSuggestedCheckedValue in WebInputElement.
  WebInputElement input_element = field->DynamicTo<WebInputElement>();
  if (IsTextInput(input_element) || IsMonthInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    input_element.SetSuggestedValue(blink::WebString::FromUTF16(
        data.value.substr(0, input_element.MaxLength())));
  } else if (IsTextAreaElement(*field) || IsSelectOrSelectMenuElement(*field)) {
    field->SetSuggestedValue(blink::WebString::FromUTF16(data.value));
  }

  if (is_initiating_node &&
      (IsTextInput(input_element) || IsTextAreaElement(*field))) {
    // Select the part of the text that the user didn't type.
    PreviewSuggestion(field->SuggestedValue().Utf16(), field->Value().Utf16(),
                      field);
  }
}

// A less-than comparator for FormFieldDatas pointer by their FieldRendererId.
// It also supports direct comparison of a FieldRendererId with a FormFieldData
// pointer.
struct CompareByRendererId {
  using is_transparent = void;
  bool operator()(const std::pair<FormFieldData*, ShadowFieldData>& f,
                  const std::pair<FormFieldData*, ShadowFieldData>& g) const {
    DCHECK(f.first && g.first);
    return f.first->unique_renderer_id < g.first->unique_renderer_id;
  }
  bool operator()(const FieldRendererId f,
                  const std::pair<FormFieldData*, ShadowFieldData>& g) const {
    DCHECK(g.first);
    return f < g.first->unique_renderer_id;
  }
  bool operator()(const std::pair<FormFieldData*, ShadowFieldData>& f,
                  FieldRendererId g) const {
    DCHECK(f.first);
    return f.first->unique_renderer_id < g;
  }
};

// Searches |field_set| for a unique field with name |field_name|. If there is
// none or more than one field with that name, the fields' shadow hosts' name
// and id attributes are tested, and the first match is returned. Returns
// nullptr if no match was found.
FormFieldData* SearchForFormControlByName(
    const std::u16string& field_name,
    const base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                         CompareByRendererId>& field_set,
    AssignedLabelSource& label_source) {
  if (field_name.empty())
    return nullptr;

  auto get_field_name = [](const auto& p) { return p.first->name; };
  auto it = base::ranges::find(field_set, field_name, get_field_name);
  auto end = field_set.end();
  if (it == end ||
      base::ranges::find(it + 1, end, field_name, get_field_name) != end) {
    auto ShadowHostHasTargetName = [&](const auto& p) {
      return base::Contains(p.second.shadow_host_name_attributes, field_name) ||
             base::Contains(p.second.shadow_host_id_attributes, field_name);
    };
    it = base::ranges::find_if(field_set, ShadowHostHasTargetName);
    if (it != end) {
      label_source =
          base::Contains(it->second.shadow_host_name_attributes, field_name)
              ? AssignedLabelSource::kShadowHostName
              : AssignedLabelSource::kShadowHostId;
    }
  } else {
    label_source = AssignedLabelSource::kName;
  }
  return it != end ? it->first : nullptr;
}

void EmitDevtoolsIssueForLabelWithoutControl(
    WebLabelElement label,
    bool label_for_matches_names_attribute) {
  if (!HasAttribute<kFor>(label)) {
    // Label has neither for attribute nor a control element was found.
    label.GetDocument().GetFrame()->AddGenericIssue(
        blink::mojom::GenericIssueErrorType::
            kFormLabelHasNeitherForNorNestedInput,
        label.GetDevToolsNodeId());
  } else if (!label_for_matches_names_attribute) {
    // Label has for attribute but no labellable element whose id OR name
    // matches it.
    // This issue is not emitted in case an element has a name that matches it,
    // instead we emit kFormLabelForNameError to educate developers that labels
    // should be linked to element ids.
    label.GetDocument().GetFrame()->AddGenericIssue(
        GenericIssueErrorType::kFormLabelForMatchesNonExistingIdError,
        label.GetDevToolsNodeId(), GetWebString<kFor>());
  } else {
    // Add a DevTools issue informing the developer that the `label`'s for-
    // attribute is pointing to the name of a field, even though the ID should
    // be used.
    label.GetDocument().GetFrame()->AddGenericIssue(
        GenericIssueErrorType::kFormLabelForNameError,
        label.GetDevToolsNodeId(), GetWebString<kFor>());
  }
}

// Considers all <label> descendents of `root`, looks at their corresponding
// control and matches them to the fields in `field_set`. The corresponding
// control is either a descendent of the label or an input specified by id in
// the label's for-attribute.
// In case no corresponding control exists, but a for-attribute is specified,
// we look for fields with matching name as a fallback. Moreover, the ids and
// names of shadow root ancestors of the fields are considered as a fallback.
void MatchLabelsAndFields(
    const WebDocument& root,
    const base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                         CompareByRendererId>& field_set) {
  WebElementCollection labels =
      root.GetElementsByHTMLTagName(GetWebString<kLabel>());
  DCHECK(!labels.IsNull());

  for (WebElement item = labels.FirstItem(); !item.IsNull();
       item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    WebElement control = label.CorrespondingControl();
    FormFieldData* field_data = nullptr;
    auto label_source = AssignedLabelSource::kId;

    if (control.IsNull()) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      field_data = SearchForFormControlByName(GetAttribute<kFor>(label).Utf16(),
                                              field_set, label_source);
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableDevtoolsIssues)) {
        EmitDevtoolsIssueForLabelWithoutControl(
            label, label_source == AssignedLabelSource::kName);
      }
    } else if (control.IsFormControlElement()) {
      WebFormControlElement form_control = control.To<WebFormControlElement>();
      if (form_control.FormControlTypeForAutofill() ==
          GetWebString<kHidden>()) {
        continue;
      }
      // Typical case: look up |field_data| in |field_set|.
      auto iter = field_set.find(GetFieldRendererId(form_control));
      if (iter == field_set.end())
        continue;
      field_data = iter->first;
    }

    // Skip `label` if we could not find an associated form control.
    if (!field_data)
      continue;

    std::u16string label_text = FindChildText(label);
    if (label_text.empty()) {
      if (HasAttribute<kFor>(label)) {
        continue;
      }
      DCHECK(!control.IsNull() && control.IsFormControlElement());
      // An associated form control was found, but the `label` does not have a
      // for-attribute, so the form control must be a descendant of the `label`.
      // Since `FindChildText()` stops at autofillable elements, the
      // `label_text` can be empty if the "text" is declared behind the <input>.
      // For example:
      // <label>
      //  <input>
      //  text
      // </label>
      // Thus, consider text behind the <input> as a fallback.
      // Since associated labels are counted as `kFor`, the source is ignored.
      FormFieldData::LabelSource irrelevant_source;
      if (!InferLabelFromNext(control.To<WebFormControlElement>(), label_text,
                              irrelevant_source)) {
        continue;
      }
    }

    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (!field_data->label.empty())
      field_data->label += u" ";
    field_data->label += label_text;
    field_data->label_source = FormFieldData::LabelSource::kFor;
    base::UmaHistogramEnumeration(kAssignedLabelSourceHistogram, label_source);
  }
}

//  Emits a devtools issue if two or more inputs tags have the same id
//  attribute.
void MaybeEmitDuplicateIdForInputIssue(
    const WebVector<WebFormControlElement>& control_elements) {
  base::flat_map<WebString, int> id_count;

  for (const WebFormControlElement& element : control_elements) {
    if (IsAutofillableElement(element) && !element.GetIdAttribute().IsEmpty()) {
      id_count[element.GetIdAttribute()]++;
    }
  }

  for (const WebFormControlElement& element : control_elements) {
    if (IsAutofillableElement(element) &&
        id_count[element.GetIdAttribute()] > 1) {
      element.GetDocument().GetFrame()->AddGenericIssue(
          GenericIssueErrorType::kFormDuplicateIdForInputError,
          element.GetDevToolsNodeId(), GetWebString<kId>());
    }
  }
}

void MaybeEmitInputWithEmptyIdAndNameIssue(
    const WebFormControlElement& element) {
  if (GetAttribute<kName>(element).IsEmpty() &&
      element.GetIdAttribute().IsEmpty()) {
    element.GetDocument().GetFrame()->AddGenericIssue(
        GenericIssueErrorType::kFormEmptyIdAndNameAttributesForInputError,
        element.GetDevToolsNodeId());
  }
}

bool HasAutocompleteAttribute(const WebElement& element) {
  return HasAttribute<kAutocomplete>(element);
}

void MaybeEmitInputAssignedAutocompleteValueToIdOrNameAttributesIssue(
    const WebFormControlElement& element) {
  if (HasAutocompleteAttribute(element)) {
    return;
  }

  auto ParsedHtmlAttributeValueToAutocompleteHasFieldType =
      [](const std::string& attribute_value) {
        absl::optional<AutocompleteParsingResult>
            parsed_attribute_to_autocomplete =
                ParseAutocompleteAttribute(attribute_value);
        if (!parsed_attribute_to_autocomplete) {
          return false;
        }

        return parsed_attribute_to_autocomplete->field_type !=
                   HtmlFieldType::kUnspecified &&
               parsed_attribute_to_autocomplete->field_type !=
                   HtmlFieldType::kUnrecognized;
      };

  bool name_attr_matches_autocomplete =
      ParsedHtmlAttributeValueToAutocompleteHasFieldType(
          GetAttribute<kName>(element).Utf8());
  bool id_attr_matches_autocomplete =
      ParsedHtmlAttributeValueToAutocompleteHasFieldType(
          element.GetIdAttribute().Utf8());

  if (name_attr_matches_autocomplete || id_attr_matches_autocomplete) {
    WebString violatingAttr = id_attr_matches_autocomplete
                                  ? GetWebString<kId>()
                                  : GetWebString<kName>();
    element.GetDocument().GetFrame()->AddGenericIssue(
        GenericIssueErrorType::
            kFormInputAssignedAutocompleteValueToIdOrNameAttributeError,
        element.GetDevToolsNodeId(),
        id_attr_matches_autocomplete ? GetWebString<kId>()
                                     : GetWebString<kName>());
    return;
  }
}

// Populates the |form|'s
//  * FormData::fields
//  * FormData::child_frames
// using the DOM elements in |control_elements| and |iframe_elements|.
//
// Optionally, |optional_field| is set to the FormFieldData that corresponds to
// |form_control_element|.
//
// |field_data_manager| and |extract_mask| are only passed to
// WebFormControlElementToFormField().
bool OwnedOrUnownedFormToFormData(
    const WebFrame* frame,
    const blink::WebFormElement& form_element,
    const blink::WebFormControlElement* form_control_element,
    const WebVector<WebFormControlElement>& control_elements,
    const std::vector<blink::WebElement>& iframe_elements,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* optional_field) {
  DCHECK(form);
  DCHECK(form->fields.empty());
  DCHECK(form->child_frames.empty());
  DCHECK(!optional_field || form_control_element);

  if (base::FeatureList::IsEnabled(features::kAutofillEnableDevtoolsIssues)) {
    MaybeEmitDuplicateIdForInputIssue(control_elements);
  }

  // Extracts fields from |control_elements| into `form->fields` and sets
  // `form->child_frames[i].predecessor` to the field index of the last field
  // that precedes the |i|th child frame.
  //
  // If `control_elements[i]` is autofillable, `fields_extracted[i]` is set to
  // true and the corresponding FormFieldData is appended to `form->fields`.
  //
  // After each iteration, `iframe_elements[next_iframe]` is the first iframe
  // that comes after `control_elements[i]`.
  //
  // After the loop,
  // - `form->fields` is completely populated;
  // - `form->child_frames` has the correct size and
  //   `form->child_frames[i].predecessor` is set to the correct value, but
  //   `form->child_frames[i].token` is not initialized yet.
  form->fields.reserve(control_elements.size());
  form->child_frames.resize(iframe_elements.size());

  std::vector<bool> fields_extracted(control_elements.size(), false);
  std::vector<ShadowFieldData> shadow_fields;
  for (size_t i = 0, next_iframe = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& control_element = control_elements[i];

    if (!IsAutofillableElement(control_element))
      continue;

    form->fields.emplace_back();
    shadow_fields.emplace_back();
    WebFormControlElementToFormField(
        form_element, control_element, field_data_manager, extract_mask,
        &form->fields.back(), &shadow_fields.back());
    fields_extracted[i] = true;

    if (base::FeatureList::IsEnabled(features::kAutofillEnableDevtoolsIssues)) {
      MaybeEmitInputWithEmptyIdAndNameIssue(control_element);
      MaybeEmitInputAssignedAutocompleteValueToIdOrNameAttributesIssue(
          control_element);
    }

    // Finds the last frame that precedes |control_element|.
    while (next_iframe < iframe_elements.size() &&
           !IsDOMPredecessor(control_element, iframe_elements[next_iframe],
                             form_element)) {
      ++next_iframe;
    }
    // The |next_frame|th frame precedes `control_element` and thus the last
    // added FormFieldData. The |k|th frames for |k| > |next_frame| may also
    // precede that FormFieldData. If they do not,
    // `form->child_frames[i].predecessor` will be updated in a later
    // iteration.
    for (size_t k = next_iframe; k < iframe_elements.size(); ++k) {
      form->child_frames[k].predecessor = form->fields.size() - 1;
    }

    if (form->fields.size() > kMaxExtractableFields) {
      form->child_frames.clear();
      form->fields.clear();
      return false;
    }
    DCHECK_LE(form->fields.size(), control_elements.size());
  }

  // Extracts field labels from the <label for="..."> tags.
  // This is done by iterating through all <label>s and looking them up in the
  // `field_set` built below.
  // Iterating through the fields and looking at their `WebElement::Labels()`
  // unfortunately doesn't scale, as each call corresponds to a DOM traverse.
  std::vector<std::pair<FormFieldData*, ShadowFieldData>> items;
  DCHECK_EQ(form->fields.size(), shadow_fields.size());
  for (size_t i = 0; i < form->fields.size(); i++)
    items.emplace_back(&form->fields[i], std::move(shadow_fields[i]));
  base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                 CompareByRendererId>
      field_set(std::move(items));

  // All `control_elements` share the same document. By providing it as the
  // `root` of `MatchLabelsAndFields()` all label tags are considered. This is
  // necessary to support label-for inference in unowned forms and in owned
  // forms utilizing the form-attribute.
  if (!control_elements.empty()) {
    MatchLabelsAndFields(control_elements[0].GetDocument(), field_set);
  }

  // Infers field labels from other tags or <labels> without for="...".
  bool found_field = false;
  DCHECK_EQ(control_elements.size(), fields_extracted.size());
  DCHECK_EQ(form->fields.size(),
            base::as_unsigned(base::ranges::count(fields_extracted, true)));
  for (size_t element_index = 0, field_index = 0;
       element_index < control_elements.size(); ++element_index) {
    if (!fields_extracted[element_index])
      continue;

    const WebFormControlElement& control_element =
        control_elements[element_index];
    FormFieldData& field = form->fields[field_index++];
    if (field.label.empty())
      InferLabelForElement(control_element, field.label, field.label_source);
    // At this point, label-for and heuristic label inference has happened and
    // `field.label_source` is set appropriately. In case no label was found,
    // it is set to kUnknown.
    base::UmaHistogramEnumeration("Autofill.LabelInference.InferredLabelSource",
                                  field.label_source);
    field.label = std::move(field.label).substr(0, kMaxStringLength);

    if (optional_field && *form_control_element == control_element) {
      *optional_field = field;
      found_field = true;
    }
  }

  // The form_control_element was not found in control_elements. This can
  // happen if elements are dynamically removed from the form while it is
  // being processed. See http://crbug.com/849870
  if (optional_field && !found_field) {
    form->fields.clear();
    form->child_frames.clear();
    return false;
  }

  // Extracts the frame tokens of |iframe_elements|.
  DCHECK_EQ(form->child_frames.size(), iframe_elements.size());
  for (size_t i = 0; i < iframe_elements.size(); ++i) {
    WebFrame* iframe = WebFrame::FromFrameOwnerElement(iframe_elements[i]);
    if (iframe && iframe->IsWebLocalFrame()) {
      form->child_frames[i].token = LocalFrameToken(
          iframe->ToWebLocalFrame()->GetLocalFrameToken().value());
    } else if (iframe && iframe->IsWebRemoteFrame()) {
      form->child_frames[i].token = RemoteFrameToken(
          iframe->ToWebRemoteFrame()->GetRemoteFrameToken().value());
    }
  }
  base::EraseIf(form->child_frames, [](const auto& child_frame) {
    return absl::visit([](const auto& token) { return token.is_empty(); },
                       child_frame.token);
  });

  if (form->child_frames.size() > kMaxExtractableChildFrames) {
    form->child_frames.clear();
  }

  const bool success = (!form->fields.empty() || !form->child_frames.empty()) &&
                       form->fields.size() < kMaxExtractableFields;
  if (!success) {
    form->fields.clear();
    form->child_frames.clear();
  }
  return success;
}

// Check if a script modified username is suitable for Password Manager to
// remember.
bool ScriptModifiedUsernameAcceptable(
    const std::u16string& value,
    const std::u16string& typed_value,
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
      lowercase.find(typed_lowercase) != std::u16string::npos) {
    return true;
  }

  // If the page-generated value comes from user typed or autofilled values in
  // other fields, that's also likely OK.
  return field_data_manager->FindMachedValue(value);
}

// Trim the vector before sending it to the browser process to ensure we
// don't send too much data through the IPC.
void TrimStringVectorForIPC(std::vector<std::u16string>* strings) {
  // Limit the size of the vector.
  if (strings->size() > kMaxListSize)
    strings->resize(kMaxListSize);

  // Limit the size of the strings in the vector.
  for (auto& string : *strings) {
    if (string.length() > kMaxStringLength)
      string.resize(kMaxStringLength);
  }
}

// Build a map from entries in |form_control_renderer_ids| to their indices,
// for more efficient lookup.
base::flat_map<FieldRendererId, size_t> BuildRendererIdToIndex(
    const std::vector<FieldRendererId>& form_control_renderer_ids) {
  std::vector<std::pair<FieldRendererId, size_t>> items;
  items.reserve(form_control_renderer_ids.size());
  for (size_t i = 0; i < form_control_renderer_ids.size(); i++)
    items.emplace_back(form_control_renderer_ids[i], i);
  return base::flat_map<FieldRendererId, size_t>(std::move(items));
}

std::string GetAutocompleteAttribute(const WebElement& element) {
  std::string autocomplete_attribute =
      GetAttribute<kAutocomplete>(element).Utf8();
  if (autocomplete_attribute.size() > kMaxStringLength) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process.  However, send over a default string to indicate that the
    // attribute was present.
    return "x-max-data-length-exceeded";
  }
  return autocomplete_attribute;
}

void ValidateAutocompleteAttributeForElement(const WebElement& element) {
  std::string autocomplete_attribute = GetAutocompleteAttribute(element);
  if (HasAutocompleteAttribute(element) && autocomplete_attribute.empty()) {
    element.GetDocument().GetFrame()->AddGenericIssue(
        blink::mojom::GenericIssueErrorType::
            kFormAutocompleteAttributeEmptyError,
        element.GetDevToolsNodeId(), GetWebString<kAutocomplete>());
  }

  const WebInputElement input_element = element.DynamicTo<WebInputElement>();

  if (IsAutocompleteTypeWrongButWellIntended(autocomplete_attribute)) {
    element.GetDocument().GetFrame()->AddGenericIssue(
        blink::mojom::GenericIssueErrorType::
            kFormInputHasWrongButWellIntendedAutocompleteValueError,
        element.GetDevToolsNodeId(), GetWebString<kAutocomplete>());
  }
}

}  // namespace

// TODO(crbug.com/1335257): This check is very similar to IsWebElementVisible()
// (see the documentation there for the subtle differences: zoom factor and
// scroll size). We can probably merge them but should do a Finch experiment
// about it.
bool IsVisibleIframe(const WebElement& element) {
  DCHECK(element.HasHTMLTagName(GetWebString<kIframe>()));
  // It is common for not-humanly-visible elements to have very small yet
  // positive bounds. The threshold of 10 pixels is chosen rather arbitrarily.
  constexpr int kMinPixelSize = 10;
  gfx::Rect bounds = element.BoundsInWidget();
  return IsWebElementFocusableForAutofill(element) &&
         bounds.width() > kMinPixelSize && bounds.height() > kMinPixelSize;
}

bool IsAdIframe(const WebElement& element) {
  DCHECK(element.HasHTMLTagName(GetWebString<kIframe>()));
  WebFrame* iframe = WebFrame::FromFrameOwnerElement(element);
  return iframe && iframe->IsAdFrame();
}

// A necessary condition for an iframe to be added to FormData::child_frames.
//
// We also extract invisible iframes for the following reason. An iframe may be
// invisible at page load (for example, when it contains parts of a credit card
// form and the user hasn't chosen a payment method yet). Autofill is not
// notified when the iframe becomes visible. That is, Autofill may have not
// re-extracted the main frame's form by the time the iframe has become visible
// and the user has focused a field in that iframe. This outdated form is
// missing the link in FormData::child_frames between the parent form and the
// iframe's form, which prevents Autofill from filling across frames.
//
// The current implementation extracts visible ad frames. Assuming IsAdIframe()
// has no false positives, we could omit the IsVisibleIframe() disjunct. We
// could even take this further and disable Autofill in ad frames.
//
// For further details, see crbug.com/1117028#c8 and crbug.com/1245631.
bool IsRelevantChildFrame(const WebElement& element) {
  DCHECK(element.HasHTMLTagName(GetWebString<kIframe>()));
  return IsVisibleIframe(element) || !IsAdIframe(element);
}

WebFormElement GetClosestAncestorFormElement(WebNode n) {
  while (!n.IsNull()) {
    if (HasTagName<kForm>(n)) {
      return n.To<WebFormElement>();
    }
    n = n.ParentNode();
  }
  return WebFormElement();
}

bool IsDOMPredecessor(const blink::WebNode& x,
                      const blink::WebNode& y,
                      const blink::WebNode& ancestor_hint) {
  DCHECK(x.GetDocument() == y.GetDocument());
  DCHECK(ancestor_hint.IsNull() ||
         x.GetDocument() == ancestor_hint.GetDocument());
  // Extends the `path` up to `end` (exclusive) or the document root.
  // Paths are backwards: the last element is the top-most node.
  auto BuildPath = [](std::vector<blink::WebNode> path,
                      const blink::WebNode& end) {
    DCHECK(!path.empty());
    path.reserve(path.size() + 16);
    blink::WebNode parent;
    while (!(parent = path.back().ParentNode()).IsNull() && parent != end)
      path.push_back(parent);
    return path;
  };
  // Returns true iff `lhs` is strictly to the left of `rhs`, provided both
  // nodes are siblings.
  auto IsLeftSiblingOf = [](const blink::WebNode& lhs,
                            const blink::WebNode& rhs) {
    DCHECK(lhs.ParentNode() == rhs.ParentNode());
    for (blink::WebNode n = rhs; !n.IsNull(); n = n.NextSibling()) {
      if (n == lhs)
        return false;
    }
    return true;
  };
  // Both paths are successors of either `ancestor_hint` or the document root.
  // If their parents aren't the same, we extend the paths to the document root.
  std::vector<blink::WebNode> x_path = BuildPath({x}, ancestor_hint);
  std::vector<blink::WebNode> y_path = BuildPath({y}, ancestor_hint);
  if (x_path.back().ParentNode() != y_path.back().ParentNode()) {
    x_path = BuildPath(std::move(x_path), blink::WebNode());
    y_path = BuildPath(std::move(y_path), blink::WebNode());
  }
  auto x_it = x_path.rbegin();
  auto y_it = y_path.rbegin();
  // Find the first different nodes in the paths. If such nodes exist, they are
  // siblings and their sibling order determines |x| and |y|'s relationship.
  while (x_it != x_path.rend() && y_it != y_path.rend()) {
    if (*x_it != *y_it)
      return IsLeftSiblingOf(*x_it, *y_it);
    ++x_it;
    ++y_it;
  }
  // If the paths don't differ in a node, the shorter path indicates a
  // predecessor since DOM traversal is in-order.
  return x_it == x_path.rend() && y_it != y_path.rend();
}

void GetDataListSuggestions(const WebInputElement& element,
                            std::vector<std::u16string>* values,
                            std::vector<std::u16string>* labels) {
  for (const auto& option : element.FilteredDataListOptions()) {
    values->push_back(option.Value().Utf16());
    if (option.Value() != option.Label())
      labels->push_back(option.Label().Utf16());
    else
      labels->push_back(std::u16string());
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

bool IsSomeControlElementVisible(
    const blink::WebDocument& document,
    const std::set<FieldRendererId>& control_elements) {
  // Returns true iff at least one element from |fields| is visible and there
  // exists an element in |control_elements| with the same field renderer id.
  // The average case time complexity is O(N log M), where N is the number of
  // elements in |fields| and M is the number of elements in
  // |control_elements|.
  auto ContainsVisibleField =
      [&](const WebVector<WebFormControlElement>& fields) {
        return base::ranges::any_of(
            fields, [&](const WebFormControlElement& field) {
              return IsWebElementFocusableForAutofill(field) &&
                     base::Contains(control_elements,
                                    GetFieldRendererId(field));
            });
      };
  return !document.IsNull() &&
         (base::ranges::any_of(document.Forms(), ContainsVisibleField,
                               &WebFormElement::GetFormControlElements) ||
          ContainsVisibleField(document.UnassociatedFormControls()));
}

GURL GetCanonicalActionForForm(const WebFormElement& form) {
  WebString action = form.Action();
  if (action.IsNull())
    action = WebString("");  // missing 'action' attribute implies current URL.
  GURL full_action(form.GetDocument().CompleteURL(action));
  return StripAuthAndParams(full_action);
}

GURL GetDocumentUrlWithoutAuth(const WebDocument& document) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  GURL full_origin(document.Url());
  return full_origin.ReplaceComponents(rep);
}

bool IsMonthInput(const WebInputElement& element) {
  return !element.IsNull() && element.FormControlTypeForAutofill() == "month";
}

// All text fields, including password fields, should be extracted.
bool IsTextInput(const WebInputElement& element) {
  return !element.IsNull() && element.IsTextField();
}

bool IsSelectOrSelectMenuElement(const WebFormControlElement& element) {
  return IsSelectElement(element) || IsSelectMenuElement(element);
}

bool IsSelectElement(const WebFormControlElement& element) {
  return !element.IsNull() &&
         element.FormControlTypeForAutofill() == "select-one";
}

bool IsSelectMenuElement(const WebFormControlElement& element) {
  return !element.IsNull() &&
         element.FormControlTypeForAutofill() == "selectmenu";
}

bool IsTextAreaElement(const WebFormControlElement& element) {
  return !element.IsNull() &&
         element.FormControlTypeForAutofill() == "textarea";
}

bool IsTextAreaElementOrTextInput(const WebFormControlElement& element) {
  return IsTextAreaElement(element) ||
         IsTextInput(element.DynamicTo<WebInputElement>());
}

bool IsCheckableElement(const WebFormControlElement& element) {
  WebInputElement input_element = element.DynamicTo<WebInputElement>();
  return !input_element.IsNull() &&
         (input_element.IsCheckbox() || input_element.IsRadioButton());
}

bool IsCheckableElement(const WebElement& element) {
  return IsCheckableElement(element.DynamicTo<WebInputElement>());
}

bool IsAutofillableInputElement(const WebInputElement& element) {
  return IsTextInput(element) || IsMonthInput(element) ||
         IsCheckableElement(element);
}

bool IsAutofillableElement(const WebFormControlElement& element) {
  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  return IsAutofillableInputElement(input_element) ||
         IsSelectElement(element) || IsTextAreaElement(element) ||
         (IsSelectMenuElement(element) &&
          base::FeatureList::IsEnabled(features::kAutofillEnableSelectMenu));
}

bool IsWebauthnTaggedElement(const WebFormControlElement& element) {
  const absl::optional<AutocompleteParsingResult> parsing_result =
      ParseAutocompleteAttribute(GetAutocompleteAttribute(element));
  return parsing_result.has_value() && parsing_result->webauthn;
}

bool IsElementEditable(const WebInputElement& element) {
  return element.IsEnabled() && !element.IsReadOnly();
}

bool IsWebElementFocusableForAutofill(const WebElement& element) {
  return element.IsFocusable() ||
         // The <selectmenu> shadow root is not focusable.
         (IsSelectMenuElement(element.DynamicTo<WebFormControlElement>()) &&
          element.To<WebSelectMenuElement>().HasFocusableChild());
}

bool IsWebElementVisible(const blink::WebElement& element) {
  auto HasMinSize = [](auto size) {
    constexpr int kMinPixelSize = 10;
    return size.width() >= kMinPixelSize && size.height() >= kMinPixelSize;
  };
  return !element.IsNull() && IsWebElementFocusableForAutofill(element) &&
         (IsCheckableElement(element) || HasMinSize(element.GetClientSize()) ||
          HasMinSize(element.GetScrollSize()));
}

std::u16string GetFormIdentifier(const WebFormElement& form) {
  std::u16string identifier = form.GetName().Utf16();
  if (identifier.empty())
    identifier = form.GetIdAttribute().Utf16();
  return identifier;
}

FormRendererId GetFormRendererId(const blink::WebFormElement& form) {
  return form.IsNull() ? FormRendererId()
                       : FormRendererId(form.UniqueRendererFormId());
}

FieldRendererId GetFieldRendererId(const blink::WebFormControlElement& field) {
  DCHECK(!field.IsNull());
  return FieldRendererId(field.UniqueRendererFormControlId());
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
  for (const auto& element : control_elements) {
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
    const WebFormElement& form_element,
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormFieldData* field,
    ShadowFieldData* shadow_data) {
  DCHECK(field);
  DCHECK(!element.IsNull());
  DCHECK(element.GetDocument().GetFrame());

  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  // Save both id and name attributes, if present. If there is only one of them,
  // it will be saved to |name|. See HTMLFormControlElement::nameForAutofill.
  field->name = element.NameForAutofill().Utf16();
  field->id_attribute = element.GetIdAttribute().Utf16();
  field->name_attribute = GetAttribute<kName>(element).Utf16();
  field->unique_renderer_id = GetFieldRendererId(element);
  field->host_form_id = GetFormRendererId(form_element);
  field->form_control_ax_id = element.GetAxId();
  field->form_control_type = element.FormControlTypeForAutofill().Utf8();
  field->max_length =
      IsTextInput(input_element) ? input_element.MaxLength() : 0;
  field->autocomplete_attribute = GetAutocompleteAttribute(element);
  field->parsed_autocomplete =
      ParseAutocompleteAttribute(field->autocomplete_attribute);

  if (base::FeatureList::IsEnabled(features::kAutofillEnableDevtoolsIssues)) {
    ValidateAutocompleteAttributeForElement(element);
  }

  if (base::EqualsCaseInsensitiveASCII(GetAttribute<kRole>(element).Utf16(),
                                       "presentation")) {
    field->role = FormFieldData::RoleAttribute::kPresentation;
  }

  field->placeholder = GetAttribute<kPlaceholder>(element).Utf16();
  if (HasAttribute<kClass>(element)) {
    field->css_classes = GetAttribute<kClass>(element).Utf16();
  }

  const FieldRendererId renderer_id(element.UniqueRendererFormControlId());
  if (field_data_manager && field_data_manager->HasFieldData(renderer_id)) {
    field->properties_mask =
        field_data_manager->GetFieldPropertiesMask(renderer_id);
  }

  field->aria_label = GetAriaLabel(element.GetDocument(), element);
  field->aria_description = GetAriaDescription(element.GetDocument(), element);

  // Traverse up through shadow hosts to see if we can gather missing
  // attributes.
  size_t levels_up = kMaxShadowLevelsUp;
  for (WebElement host = element.OwnerShadowHost();
       !host.IsNull() && levels_up > 0 && !form_element.IsNull() &&
       form_element.OwnerShadowHost() != host;
       host = host.OwnerShadowHost(), --levels_up) {
    std::u16string shadow_host_id = host.GetIdAttribute().Utf16();
    if (shadow_data && !shadow_host_id.empty())
      shadow_data->shadow_host_id_attributes.push_back(shadow_host_id);
    std::u16string shadow_host_name = GetAttribute<kName>(host).Utf16();
    if (shadow_data && !shadow_host_name.empty())
      shadow_data->shadow_host_name_attributes.push_back(shadow_host_name);

    if (field->id_attribute.empty())
      field->id_attribute = host.GetIdAttribute().Utf16();
    if (field->name_attribute.empty())
      field->name_attribute = GetAttribute<kName>(host).Utf16();
    if (field->name.empty()) {
      field->name = field->name_attribute.empty() ? field->id_attribute
                                                  : field->name_attribute;
    }
    if (base::FeatureList::IsEnabled(features::kAutofillEnableDevtoolsIssues)) {
      ValidateAutocompleteAttributeForElement(element);
    }
    if (field->autocomplete_attribute.empty()) {
      field->autocomplete_attribute = GetAutocompleteAttribute(host);
      field->parsed_autocomplete =
          ParseAutocompleteAttribute(field->autocomplete_attribute);
    }
    if (field->css_classes.empty() && HasAttribute<kClass>(host)) {
      field->css_classes = GetAttribute<kClass>(host).Utf16();
    }
    if (field->aria_label.empty())
      field->aria_label = GetAriaLabel(host.GetDocument(), host);
    if (field->aria_description.empty())
      field->aria_description = GetAriaDescription(host.GetDocument(), host);
  }

  if (!IsAutofillableElement(element))
    return;

  if (IsAutofillableInputElement(input_element) || IsTextAreaElement(element) ||
      IsSelectOrSelectMenuElement(element)) {
    // The browser doesn't need to differentiate between preview and autofill.
    field->is_autofilled = element.IsAutofilled();
    field->is_focusable = IsWebElementFocusableForAutofill(element);
    field->is_visible = IsWebElementVisible(element);
    field->should_autocomplete =
        element.AutoComplete() &&
        !(field->parsed_autocomplete.has_value() &&
          field->parsed_autocomplete.value().field_type ==
              HtmlFieldType::kOneTimeCode);
    field->text_direction = GetTextDirectionForElement(element);
    field->is_enabled = element.IsEnabled();
    field->is_readonly = element.IsReadOnly();
  }

  if (IsAutofillableInputElement(input_element)) {
    SetCheckStatus(field, IsCheckableElement(input_element),
                   input_element.IsChecked());
  } else if (IsTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else if (extract_mask & EXTRACT_OPTIONS) {
    // Set option strings on the field if available.
    DCHECK(IsSelectOrSelectMenuElement(element));
    WebVector<WebElement> element_list_items =
        GetListItemsForSelectOrSelectMenu(element);
    FilterOptionElementsAndGetOptionStrings(element_list_items,
                                            &field->options);
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
    if (WebInputElement input = element.DynamicTo<WebInputElement>();
        !input.IsNull()) {
      GetDataListSuggestions(input, &field->datalist_values,
                             &field->datalist_labels);
    }
  }

  if (!(extract_mask & EXTRACT_VALUE))
    return;

  std::u16string value = element.Value().Utf16();

  if (IsSelectOrSelectMenuElement(element) &&
      (extract_mask & EXTRACT_OPTION_TEXT)) {
    // Convert the |element| value to text if requested.
    WebVector<WebElement> list_items =
        GetListItemsForSelectOrSelectMenu(element);
    for (const auto& list_item : list_items) {
      if (HasTagName<kOption>(list_item)) {
        const WebOptionElement option_element =
            list_item.To<WebOptionElement>();
        if (option_element.Value().Utf16() == value) {
          value = option_element.GetText().Utf16();
          break;
        }
      }
    }
  }

  // Constrain the maximum data length to prevent a malicious site from DOS'ing
  // the browser: http://crbug.com/49332
  field->value = std::move(value).substr(0, kMaxStringLength);

  // If the field was autofilled or the user typed into it, check the value
  // stored in |field_data_manager| against the value property of the DOM
  // element. If they differ, then the scripts on the website modified the
  // value afterwards. Store the original value as the |typed_value|, unless
  // this is one of recognised situations when the site-modified value is more
  // useful for filling.
  if (field_data_manager &&
      field->properties_mask & (FieldPropertiesFlags::kUserTyped |
                                FieldPropertiesFlags::kAutofilled)) {
    const std::u16string user_input =
        field_data_manager->GetUserInput(GetFieldRendererId(element));

    // The typed value is preserved for all passwords. It is also preserved for
    // potential usernames, as long as the |value| is not deemed acceptable.
    if (field->form_control_type == "password" ||
        !ScriptModifiedUsernameAcceptable(field->value, user_input,
                                          field_data_manager)) {
      field->user_input = user_input;
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
  form->unique_renderer_id = GetFormRendererId(form_element);
  form->action = GetCanonicalActionForForm(form_element);
  form->is_action_empty =
      form_element.Action().IsNull() || form_element.Action().IsEmpty();
  form->main_frame_origin = url::Origin();
  // If the completed URL is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = blink::WebStringToGURL(form_element.Action());

  std::vector<WebElement> owned_iframes;
  WebElementCollection iframes =
      form_element.GetElementsByHTMLTagName(GetWebString<kIframe>());
  for (WebElement iframe = iframes.FirstItem(); !iframe.IsNull();
       iframe = iframes.NextItem()) {
    if (GetClosestAncestorFormElement(iframe) == form_element &&
        IsRelevantChildFrame(iframe)) {
      owned_iframes.push_back(iframe);
    }
  }

  return OwnedOrUnownedFormToFormData(
      frame, form_element, &form_control_element,
      form_element.GetFormControlElements(), owned_iframes, field_data_manager,
      extract_mask, form, field);
}

WebFormElement GetOwningForm(const WebFormControlElement& form_control) {
  WebFormElement associated_form = form_control.Form();
  if (!associated_form.IsNull()) {
    return associated_form;
  }

  // If we are in a shadow DOM, then look to see if the host(s) are inside a
  // form element we can use.
  size_t levels_up = kMaxShadowLevelsUp;
  for (WebElement host = form_control.OwnerShadowHost();
       !host.IsNull() && levels_up > 0;
       host = host.OwnerShadowHost(), --levels_up) {
    for (WebNode parent = host; !parent.IsNull();
         parent = parent.ParentNode()) {
      if (parent.IsElementNode()) {
        WebElement parentElement = parent.To<WebElement>();
        if (HasTagName<kForm>(parentElement)) {
          return parentElement.To<WebFormElement>();
        }
      }
    }
  }
  return WebFormElement();
}

std::vector<WebFormControlElement> GetUnownedFormFieldElements(
    const WebDocument& document) {
  return document.UnassociatedFormControls().ReleaseVector();
}

std::vector<WebFormControlElement> GetUnownedAutofillableFormFieldElements(
    const WebDocument& document) {
  return ExtractAutofillableElementsFromSet(
      GetUnownedFormFieldElements(document));
}

std::vector<WebElement> GetUnownedIframeElements(const WebDocument& document) {
  std::vector<WebElement> unowned_iframes;
  WebElementCollection iframes =
      document.GetElementsByHTMLTagName(GetWebString<kIframe>());
  for (WebElement iframe = iframes.FirstItem(); !iframe.IsNull();
       iframe = iframes.NextItem()) {
    if (GetClosestAncestorFormElement(iframe).IsNull() &&
        IsRelevantChildFrame(iframe)) {
      unowned_iframes.push_back(iframe);
    }
  }
  return unowned_iframes;
}

bool UnownedFormElementsToFormData(
    const std::vector<blink::WebFormControlElement>& control_elements,
    const std::vector<blink::WebElement>& iframe_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  blink::WebLocalFrame* frame = document.GetFrame();
  if (!frame)
    return false;

  form->unique_renderer_id = FormRendererId();
  form->main_frame_origin = url::Origin();

  form->is_form_tag = false;

  return OwnedOrUnownedFormToFormData(
      frame, WebFormElement(), element, control_elements, iframe_elements,
      field_data_manager, extract_mask, form, field);
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
  WebFormElement form_element = GetOwningForm(element);

  if (form_element.IsNull()) {
    // No associated form, try the synthetic form for unowned form elements.
    WebDocument document = element.GetDocument();
    std::vector<WebFormControlElement> control_elements =
        GetUnownedAutofillableFormFieldElements(document);
    std::vector<WebElement> iframe_elements =
        GetUnownedIframeElements(document);
    return UnownedFormElementsToFormData(control_elements, iframe_elements,
                                         &element, document, field_data_manager,
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

std::vector<WebFormControlElement> FillOrPreviewForm(
    const FormData& form,
    const WebFormControlElement& initiating_element,
    mojom::AutofillActionPersistence action_persistence) {
  DCHECK(!initiating_element.IsNull());

  WebFormElement form_element = GetOwningForm(initiating_element);
  std::vector<WebFormControlElement> control_elements =
      form_element.IsNull() ? GetUnownedAutofillableFormFieldElements(
                                  initiating_element.GetDocument())
                            : ExtractAutofillableElementsInForm(form_element);

  if (!IsElementInControlElementSet(initiating_element, control_elements)) {
    return {};
  }
  const bool num_elements_matches_num_fields =
      control_elements.size() == form.fields.size();
  UMA_HISTOGRAM_BOOLEAN("Autofill.NumElementsMatchesNumFields",
                        num_elements_matches_num_fields);

  // This is the focused element that led to the filling. It might not exist in
  // scenarios like refills where no element is focused, but if it is then it
  // needs special treatment. See intended behavior comment below.
  WebFormControlElement* initially_focused_element = nullptr;

  // This container stores the pairs of autofillable WebFormControlElement* and
  // the corresponding FormFieldData* of `form.fields` that are used to fill
  // this element.
  std::vector<std::pair<WebFormControlElement*, const FormFieldData*>>
      autofillable_elements_index_pairs;

  std::vector<WebFormControlElement> matching_fields;
  matching_fields.reserve(control_elements.size());

  // Prepare for binary search.
  SortByFieldRendererIds(control_elements);

  // If this is a preview, prevent already autofilled fields from being
  // highlighted.
  if (action_persistence == mojom::AutofillActionPersistence::kPreview &&
      base::FeatureList::IsEnabled(
          features::kAutofillHighlightOnlyChangedValuesInPreviewMode)) {
    for (auto& element : control_elements) {
      element.SetPreventHighlightingOfAutofilledFields(true);
    }
  }

  auto fill_or_preview =
      action_persistence == mojom::AutofillActionPersistence::kPreview
          ? &PreviewFormField
          : &FillFormField;

  // The intended behaviour is:
  // * Autofill the currently focused element.
  // * Send the blur event.
  // * For each other element, focus -> autofill -> blur.
  // * Send the focus event for the initially focused element.
  for (const FormFieldData& field : form.fields) {
    auto it = SearchInSortedVector(field, control_elements);
    if (it == control_elements.end()) {
      continue;
    }
    WebFormControlElement& element = *it;
    element.SetAutofillSection(WebString::FromUTF8(field.section.ToString()));

    if (ShouldSkipFillField(
            field, element,
            initiating_element.DynamicTo<WebFormControlElement>())) {
      continue;
    }

    // Autofill the initiating element.
    bool is_initiating_element = (element == initiating_element);
    if (is_initiating_element) {
      if (action_persistence == mojom::AutofillActionPersistence::kFill &&
          element.Focused()) {
        initially_focused_element = &element;
      }

      matching_fields.push_back(element);
      // In preview mode, only fill the field if it changes the fields value.
      // With this, the WebAutofillState is not changed from kAutofilled to
      // kPreviewed. This prevents the highlighting to change.
      if (action_persistence == mojom::AutofillActionPersistence::kFill ||
          field.value != element.Value().Utf16() ||
          !base::FeatureList::IsEnabled(
              features::kAutofillHighlightOnlyChangedValuesInPreviewMode)) {
        fill_or_preview(field, is_initiating_element, &element);
      }
      continue;
    }
    CHECK(element != initiating_element);
    // Storing the indexes of non-initiating elements to be autofilled after
    // triggering the blur event for the initiating element.
    autofillable_elements_index_pairs.emplace_back(&element, &field);
  }

  // If there is no other field to be autofilled, sending the blur event and
  // then the focus event for the initiating element does not make sense.
  if (autofillable_elements_index_pairs.empty()) {
    return matching_fields;
  }

  // A blur event is emitted for the focused element if it is the initiating
  // element before all other elements are autofilled.
  if (initially_focused_element) {
    initially_focused_element->DispatchBlurEvent();
  }

  // Autofill the non-initiating elements.
  for (const auto& [filled_element, field_data] :
       autofillable_elements_index_pairs) {
    matching_fields.push_back(*filled_element);
    fill_or_preview(*field_data, false, filled_element);
  }

  // A focus event is emitted for the initiating element after autofilling is
  // completed. It is not intended to work for the preview filling.
  if (initially_focused_element) {
    initially_focused_element->DispatchFocusEvent();
  }

  return matching_fields;
}

void UndoForm(const FormData& form,
              const WebFormControlElement& form_control_element) {
  WebFormElement form_element = GetOwningForm(form_control_element);
  if (form_element.IsNull() && form_control_element.IsNull()) {
    return;
  }
  std::vector<WebFormControlElement> control_elements =
      form_element.IsNull() ? GetUnownedAutofillableFormFieldElements(
                                  form_control_element.GetDocument())
                            : ExtractAutofillableElementsInForm(form_element);
  if (!IsElementInControlElementSet(form_control_element, control_elements)) {
    return;
  }

  const bool num_elements_matches_num_fields =
      control_elements.size() == form.fields.size();
  UMA_HISTOGRAM_BOOLEAN("Autofill.NumElementsMatchesNumFields",
                        num_elements_matches_num_fields);

  // The intended behaviour is:
  // * Undo the currently focused element.
  // * Send the blur event.
  // * For each other element, focus -> undo -> blur.
  // * Send the focus event for the initially focused element.
  WebFormControlElement* initially_focused_element = nullptr;

  // This container stores the pairs of autofillable WebFormControlElement* and
  // the corresponding indexes of |data.fields| that are used to fill this
  // element.
  std::vector<std::pair<WebFormControlElement*, size_t>>
      undo_autofill_elements_index_pairs;

  // Prepare for binary search.
  SortByFieldRendererIds(control_elements);
  for (size_t i = 0; i < form.fields.size(); ++i) {
    auto it = SearchInSortedVector(form.fields[i], control_elements);
    if (it == control_elements.end()) {
      continue;
    }
    // Undo autofill on the initiating element.
    WebFormControlElement& element = *it;
    bool is_initiating_element = (element == form_control_element);
    if (is_initiating_element) {
      if (element.Focused()) {
        initially_focused_element = &element;
      }
      UndoFormField(form.fields[i], is_initiating_element, &element);
      continue;
    }

    // If a field is not currently autofilled in blink there's no current
    // autofill value to undo. Otherwise, if the FormFieldData's is_autofilled
    // state is true, this means the current field is not included in the undo
    // operation and has been filled by another fill operation.
    if (element.GetAutofillState() == WebAutofillState::kAutofilled) {
      // Storing the indexes of non-initiating elements to be undone after
      // triggering the blur event for the initiating element.
      undo_autofill_elements_index_pairs.emplace_back(&element, i);
    }
  }

  // If there is no other field where autofill must be undone, sending the blur
  // event and then the focus event for the initiating element does not make
  // sense.
  if (undo_autofill_elements_index_pairs.empty()) {
    return;
  }

  // A blur event is emitted for the focused element if it is the initiating
  // element before all other elements are autofilled.
  if (initially_focused_element) {
    initially_focused_element->DispatchBlurEvent();
  }

  // Undo autofill on the non-initiating elements.
  for (const auto& [filled_element, index] :
       undo_autofill_elements_index_pairs) {
    UndoFormField(form.fields[index], false, filled_element);
  }

  // A focus event is emitted for the initiating element after undoing autofill
  // is completed.
  if (initially_focused_element) {
    initially_focused_element->DispatchFocusEvent();
  }
}

void ClearPreviewedElements(
    std::vector<blink::WebFormControlElement>& previewed_elements,
    const WebFormControlElement& initiating_element,
    blink::WebAutofillState old_autofill_state) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillHighlightOnlyChangedValuesInPreviewMode)) {
    // If this is a synthetic form, get the unowned form elements. Otherwise,
    // get all element associated with the form of the initiated field.
    std::vector<WebFormControlElement> form_elements =
        initiating_element.Form().IsNull()
            ? GetUnownedFormFieldElements(initiating_element.GetDocument())
            : ExtractAutofillableElementsInForm(initiating_element.Form());

    // Allow the highlighting of already autofilled fields again.
    for (auto& element : form_elements) {
      element.SetPreventHighlightingOfAutofilledFields(false);
    }
  }

  for (WebFormControlElement& control_element : previewed_elements) {
    // We do not add null elements to `previewed_elements_` in AutofillAgent.
    DCHECK(!control_element.IsNull());

    // Clear the suggested value. For the initiating node, also restore the
    // original value.
    WebInputElement input_element =
        control_element.DynamicTo<WebInputElement>();
    if (IsTextInput(input_element) || IsMonthInput(input_element) ||
        IsTextAreaElement(control_element)) {
      control_element.SetSuggestedValue(WebString());
      bool is_initiating_node = (initiating_element == control_element);
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
    } else {
      control_element.SetSuggestedValue(WebString());
      control_element.SetAutofillState(WebAutofillState::kNotFilled);
    }
  }
}

bool IsOwnedByFrame(const WebNode& node, content::RenderFrame* frame) {
  if (node.IsNull() || !frame)
    return false;
  const blink::WebDocument& doc = node.GetDocument();
  blink::WebLocalFrame* node_frame = !doc.IsNull() ? doc.GetFrame() : nullptr;
  blink::WebLocalFrame* expected_frame = frame->GetWebFrame();
  return expected_frame && node_frame &&
         expected_frame->GetLocalFrameToken() ==
             node_frame->GetLocalFrameToken();
}

bool IsWebpageEmpty(const blink::WebLocalFrame* frame) {
  blink::WebDocument document = frame->GetDocument();

  return IsWebElementEmpty(document.Head()) &&
         IsWebElementEmpty(document.Body());
}

bool IsWebElementEmpty(const blink::WebElement& root) {
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
    if (!element.HasHTMLTagName(GetWebString<kScript>()) &&
        !element.HasHTMLTagName(GetWebString<kMeta>()) &&
        !element.HasHTMLTagName(GetWebString<kTitle>())) {
      return false;
    }
  }
  return true;
}

void PreviewSuggestion(const std::u16string& suggestion,
                       const std::u16string& user_input,
                       blink::WebFormControlElement* input_element) {
  size_t selection_start = user_input.length();
  if (IsFeatureSubstringMatchEnabled()) {
    size_t offset = GetTextSelectionStart(suggestion, user_input, false);
    // Zero selection start is for password manager, which can show usernames
    // that do not begin with the user input value.
    selection_start = (offset == std::u16string::npos) ? 0 : offset;
  }

  input_element->SetSelectionRange(selection_start, suggestion.length());
}

std::u16string FindChildText(const WebNode& node) {
  return FindChildTextWithIgnoreList(node, std::set<WebNode>());
}

ButtonTitleList GetButtonTitles(const WebFormElement& web_form,
                                ButtonTitlesCache* button_titles_cache) {
  if (!button_titles_cache) {
    // Button titles scraping is disabled for this form.
    return ButtonTitleList();
  }

  DCHECK(!web_form.IsNull());

  auto [form_position, cache_miss] = button_titles_cache->emplace(
      GetFormRendererId(web_form), ButtonTitleList());
  if (!cache_miss)
    return form_position->second;

  form_position->second = InferButtonTitlesForForm(web_form);
  return form_position->second;
}

std::u16string FindChildTextWithIgnoreListForTesting(
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  return FindChildTextWithIgnoreList(node, divs_to_skip);
}

bool InferLabelForElementForTesting(const WebFormControlElement& element,
                                    std::u16string& label,
                                    FormFieldData::LabelSource& label_source) {
  return InferLabelForElement(element, label, label_source);
}

WebFormElement FindFormByUniqueRendererId(const WebDocument& doc,
                                          FormRendererId form_renderer_id) {
  for (const auto& form : doc.Forms()) {
    if (GetFormRendererId(form) == form_renderer_id)
      return form;
  }
  return WebFormElement();
}

WebFormControlElement FindFormControlElementByUniqueRendererId(
    const WebDocument& doc,
    FieldRendererId queried_form_control,
    absl::optional<FormRendererId> form_to_be_searched /*= absl::nullopt*/) {
  auto FindField = [&](const WebVector<WebFormControlElement>& fields) {
    auto it =
        base::ranges::find(fields, queried_form_control, GetFieldRendererId);
    return it != fields.end() ? *it : WebFormControlElement();
  };

  auto IsCandidate =
      [&form_to_be_searched](const FormRendererId& expected_form_renderer_id) {
        return !form_to_be_searched.has_value() ||
               form_to_be_searched.value() == expected_form_renderer_id;
      };

  if (IsCandidate(FormRendererId())) {
    // Search the unowned form.
    WebFormControlElement e = FindField(doc.UnassociatedFormControls());
    if (form_to_be_searched == FormRendererId() || !e.IsNull())
      return e;
  }
  for (const WebFormElement& form : doc.Forms()) {
    // If the |form_to_be_searched| is specified, skip this form if it is not
    // the right one.
    if (!IsCandidate(GetFormRendererId(form))) {
      continue;
    }
    WebFormControlElement e = FindField(form.GetFormControlElements());
    if (form_to_be_searched == GetFormRendererId(form) || !e.IsNull())
      return e;
  }
  return WebFormControlElement();
}

std::vector<WebFormControlElement> FindFormControlElementsByUniqueRendererId(
    const WebDocument& doc,
    const std::vector<FieldRendererId>& queried_form_controls) {
  std::vector<WebFormControlElement> result(queried_form_controls.size());
  auto renderer_id_to_index_map = BuildRendererIdToIndex(queried_form_controls);

  auto AddToResultIfQueried = [&](const WebFormControlElement& field) {
    auto it = renderer_id_to_index_map.find(GetFieldRendererId(field));
    if (it != renderer_id_to_index_map.end())
      result[it->second] = field;
  };

  for (const auto& form : doc.Forms()) {
    for (const auto& field : form.GetFormControlElements())
      AddToResultIfQueried(field);
  }
  for (const auto& field : doc.UnassociatedFormControls()) {
    AddToResultIfQueried(field);
  }
  return result;
}

std::vector<WebFormControlElement> FindFormControlElementsByUniqueRendererId(
    const WebDocument& doc,
    FormRendererId form_renderer_id,
    const std::vector<FieldRendererId>& queried_form_controls) {
  std::vector<WebFormControlElement> result(queried_form_controls.size());
  WebFormElement form = FindFormByUniqueRendererId(doc, form_renderer_id);
  if (form.IsNull())
    return result;

  auto renderer_id_to_index_map = BuildRendererIdToIndex(queried_form_controls);

  for (const auto& field : form.GetFormControlElements()) {
    auto it = renderer_id_to_index_map.find(GetFieldRendererId(field));
    if (it == renderer_id_to_index_map.end())
      continue;
    result[it->second] = field;
  }
  return result;
}

namespace {

std::vector<WebElement> GetWebElementsFromIdList(const WebDocument& document,
                                                 const WebString& id_list) {
  std::vector<WebElement> web_elements;
  std::u16string id_list_utf16 = id_list.Utf16();
  for (const auto& id : base::SplitStringPiece(
           id_list_utf16, base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    web_elements.push_back(
        document.GetElementById(WebString(id.data(), id.length())));
  }
  return web_elements;
}

// Returns the coalesced child of the elements who's ids are found in
// |id_list|.
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
std::u16string CoalesceTextByIdList(const WebDocument& document,
                                    const WebString& id_list) {
  const std::u16string kSpace = u" ";

  std::u16string text;
  for (const auto& node : GetWebElementsFromIdList(document, id_list)) {
    if (!node.IsNull()) {
      std::u16string child_text = FindChildText(node);
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

void MaybeEmitAriaLabelledByDevtoolsIssue(const WebElement& element,
                                          const WebString& id_list) {
  if (base::ranges::any_of(
          GetWebElementsFromIdList(element.GetDocument(), id_list),
          [](const WebElement& node) { return node.IsNull(); })) {
    element.GetDocument().GetFrame()->AddGenericIssue(
        blink::mojom::GenericIssueErrorType::kFormAriaLabelledByToNonExistingId,
        element.GetDevToolsNodeId(), GetWebString<kAriaLabelledBy>());
  }
}

}  // namespace

std::u16string GetAriaLabel(const blink::WebDocument& document,
                            const WebElement& element) {
  if (HasAttribute<kAriaLabelledBy>(element)) {
    blink::WebString arial_label_attribute =
        GetAttribute<kAriaLabelledBy>(element);
    if (base::FeatureList::IsEnabled(features::kAutofillEnableDevtoolsIssues)) {
      MaybeEmitAriaLabelledByDevtoolsIssue(element, arial_label_attribute);
    }
    std::u16string text = CoalesceTextByIdList(document, arial_label_attribute);
    if (!text.empty())
      return text;
  }

  if (HasAttribute<kAriaLabel>(element)) {
    return GetAttribute<kAriaLabel>(element).Utf16();
  }

  return std::u16string();
}

std::u16string GetAriaDescription(const blink::WebDocument& document,
                                  const WebElement& element) {
  return CoalesceTextByIdList(document,
                              GetAttribute<kAriaDescribedBy>(element));
}

WebNode NextWebNode(const WebNode& current_node, bool forward) {
  if (forward) {
    if (!current_node.FirstChild().IsNull()) {
      return current_node.FirstChild();
    }
    if (!current_node.NextSibling().IsNull()) {
      return current_node.NextSibling();
    }
    WebNode parent = current_node.ParentNode();
    while (!parent.IsNull()) {
      if (!parent.NextSibling().IsNull()) {
        return parent.NextSibling();
      }
      parent = parent.ParentNode();
    }
    return parent;
  } else {
    if (!current_node.PreviousSibling().IsNull()) {
      WebNode previous = current_node.PreviousSibling();
      while (!previous.LastChild().IsNull()) {
        previous = previous.LastChild();
      }
      return previous;
    }
    return current_node.ParentNode();
  }
}

void TraverseDomForFourDigitCombinations(
    const WebDocument& document,
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  re2::RE2 kFourDigitRegex("(?:\\D|^)(\\d{4})(?:\\D|$)");
  base::flat_set<std::string> matches;
  // Iterate through each form control element in the DOM and extract the
  // elements nearby in search of four digit combinations.
  std::vector<WebFormControlElement> form_control_elements;

  for (const WebFormElement& form : document.Forms()) {
    base::ranges::move(form.GetFormControlElements().ReleaseVector(),
                       std::back_inserter(form_control_elements));
  }

  base::ranges::move(document.UnassociatedFormControls().ReleaseVector(),
                     std::back_inserter(form_control_elements));

  auto extract_four_digit_combinations = [&](WebNode node) {
    if (!node.IsTextNode()) {
      return;
    }
    std::string node_text = node.NodeValue().Utf8();
    re2::StringPiece input(node_text);
    std::string match;
    while (matches.size() < kMaxFourDigitCombinationMatches &&
           re2::RE2::FindAndConsume(&input, kFourDigitRegex, &match)) {
      matches.insert(match);
    }
  };

  // Returns whether the traversal reached a form control element.
  auto iterate_and_extract_four_digit_combinations = [&](WebNode node,
                                                         bool forward) {
    for (int i = 0; i < kFormNeighborNodesToTraverse; ++i) {
      if (node.IsNull()) {
        break;
      }
      extract_four_digit_combinations(node);
      node = NextWebNode(node, forward);
      if (auto form_control_element = node.DynamicTo<WebFormControlElement>();
          !form_control_element.IsNull()) {
        // Reached next form control element.
        return true;
      }
    }
    return false;
  };

  bool reached_form_control_before = false;
  for (const WebFormControlElement& element : form_control_elements) {
    // If a forward search ended at a form control, we don't need a backward
    // search for that form control.
    if (!reached_form_control_before) {
      iterate_and_extract_four_digit_combinations(element,
                                                  /*forward=*/false);
    }
    reached_form_control_before =
        iterate_and_extract_four_digit_combinations(element, /*forward=*/true);

    if (matches.size() >= kMaxFourDigitCombinationMatches) {
      break;
    }
  }

  // Check for consecutive numbers as a potential indicator that we've parsed
  // a year <select> element of a credit card form. This indicates that a CVC
  // field is not a standalone CVC element.
  if (matches.size() > 2) {
    auto iter = matches.begin();
    int consecutive_numbers = 0;
    int previous_combination = 0;
    base::StringToInt(*iter, &previous_combination);
    iter++;
    for (; iter != matches.end(); ++iter) {
      int current_combination = 0;
      base::StringToInt(*iter, &current_combination);
      if (current_combination == previous_combination + 1) {
        consecutive_numbers++;
      } else {
        consecutive_numbers = 0;
      }
      if (consecutive_numbers > kMaxConsecutiveInFourDigitCombinationMatches) {
        // Clear all matches as we presume this is not standalone cvc if
        // there is a year input field.
        matches.clear();
        break;
      }
      previous_combination = current_combination;
    }
  }

  std::move(potential_matches)
      .Run(std::vector<std::string>(matches.begin(), matches.end()));
}

}  // namespace autofill::form_util
