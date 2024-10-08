// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/common/password_manager_features.h"
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
using blink::WebString;
using blink::WebVector;

namespace autofill::form_util {

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

using LabelSource = FormFieldData::LabelSource;

// Maximal length of a button's title.
constexpr int kMaxLengthForSingleButtonTitle = 30;
// Maximal length of all button titles.
constexpr int kMaxLengthForAllButtonTitles = 200;

// Number of shadow roots to traverse upwards when looking for relevant forms
// and labels of an input element inside a shadow root.
constexpr size_t kMaxShadowLevelsUp = 2;

// Text features to detect form submission buttons. Features are selected based
// on analysis of real forms and their buttons.
// TODO(crbug.com/41429204): Consider to add more features (e.g. non-English
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
constexpr std::string_view kAnchor = "a";
constexpr std::string_view kAutocomplete = "autocomplete";
constexpr std::string_view kAriaDescribedBy = "aria-describedby";
constexpr std::string_view kAriaLabel = "aria-label";
constexpr std::string_view kAriaLabelledBy = "aria-labelledby";
constexpr std::string_view kBold = "b";
constexpr std::string_view kBreak = "br";
constexpr std::string_view kButton = "button";
constexpr std::string_view kClass = "class";
constexpr std::string_view kColspan = "colspan";
constexpr std::string_view kDefinitionDescriptionTag = "dd";
constexpr std::string_view kDefinitionTermTag = "dt";
constexpr std::string_view kDiv = "div";
constexpr std::string_view kFieldset = "fieldset";
constexpr std::string_view kFont = "font";
constexpr std::string_view kFor = "for";
constexpr std::string_view kForm = "form";
constexpr std::string_view kFormControlSelector = "input, select, textarea";
constexpr std::string_view kId = "id";
constexpr std::string_view kIframe = "iframe";
constexpr std::string_view kImage = "img";
constexpr std::string_view kInput = "input";
constexpr std::string_view kLabel = "label";
constexpr std::string_view kListItem = "li";
constexpr std::string_view kMeta = "meta";
constexpr std::string_view kName = "name";
constexpr std::string_view kNoScript = "noscript";
constexpr std::string_view kOption = "option";
constexpr std::string_view kParagraph = "p";
constexpr std::string_view kPlaceholder = "placeholder";
constexpr std::string_view kRole = "role";
constexpr std::string_view kScript = "script";
constexpr std::string_view kSpan = "span";
#if BUILDFLAG(IS_ANDROID)
constexpr std::string_view kSrc = "src";
#endif
constexpr std::string_view kStrong = "strong";
constexpr std::string_view kSubmit = "submit";
constexpr std::string_view kTable = "table";
constexpr std::string_view kTableCell = "td";
constexpr std::string_view kTableHeader = "th";
constexpr std::string_view kTableRow = "tr";
constexpr std::string_view kTitle = "title";
constexpr std::string_view kType = "type";
constexpr std::string_view kValue = "value";

// Wrapper for frequently used WebString constants.
template <const std::string_view& string>
const WebString& GetWebString() {
  static const base::NoDestructor<WebString> web_string(
      WebString::FromUTF8(string));
  return *web_string;
}

template <const std::string_view& tag_name>
bool HasTagName(const WebElement& element) {
  return element.HasHTMLTagName(GetWebString<tag_name>());
}

template <const std::string_view& tag_name>
bool HasTagName(const WebNode& node) {
  return node.IsElementNode() && HasTagName<tag_name>(node.To<WebElement>());
}

template <const std::string_view& attribute>
bool HasAttribute(const WebElement& element) {
  return element.HasAttribute(GetWebString<attribute>());
}

template <const std::string_view& attribute>
WebString GetAttribute(const WebElement& element) {
  return element.GetAttribute(GetWebString<attribute>());
}

// Returns the form's |name| attribute if non-empty; otherwise the form's |id|
// attribute.
std::u16string GetFormIdentifier(const WebFormElement& form) {
  std::u16string identifier = form.GetName().Utf16();
  if (identifier.empty()) {
    identifier = form.GetIdAttribute().Utf16();
  }
  return identifier;
}

// Helper function to return the next web node of `current_node` in the DOM.
// `forward` determines the direction to traverse in.
WebNode NextWebNode(const WebNode& current_node, bool forward) {
  if (forward) {
    if (current_node.FirstChild()) {
      return current_node.FirstChild();
    }
    if (current_node.NextSibling()) {
      return current_node.NextSibling();
    }
    WebNode parent = current_node.ParentNode();
    while (parent) {
      if (parent.NextSibling()) {
        return parent.NextSibling();
      }
      parent = parent.ParentNode();
    }
    return parent;
  } else {
    if (current_node.PreviousSibling()) {
      WebNode previous = current_node.PreviousSibling();
      while (previous.LastChild()) {
        previous = previous.LastChild();
      }
      return previous;
    }
    return current_node.ParentNode();
  }
}

// All text fields, including password fields, should be extracted.
bool IsTextInput(const WebInputElement& element) {
  return element && element.IsTextField();
}

bool IsSelectElement(const WebFormControlElement& element) {
  return element && element.FormControlTypeForAutofill() ==
                        blink::mojom::FormControlType::kSelectOne;
}

bool IsTextInput(const WebFormControlElement& element) {
  return IsTextInput(element.DynamicTo<WebInputElement>());
}

bool IsMonthInput(const WebFormControlElement& element) {
  return element && element.FormControlTypeForAutofill() ==
                        blink::mojom::FormControlType::kInputMonth;
}

bool IsCheckableElement(const WebFormControlElement& element) {
  using enum blink::mojom::FormControlType;
  // We intentionally use `FormControlType()` instead of
  // `FormControlTypeForAutofill()` because the existing callers do not care if
  // the field has ever been a password field before.
  return element && (element.FormControlType() == kInputCheckbox ||  // nocheck
                     element.FormControlType() == kInputRadio);      // nocheck
}

bool IsCheckableElement(const WebElement& element) {
  return IsCheckableElement(element.DynamicTo<WebInputElement>());
}

// Returns true if |node| is an element and it is a container type that
// InferLabelForElement() can traverse.
bool IsTraversableContainerElement(const WebNode& node) {
  if (!node.IsElementNode()) {
    return false;
  }

  const WebElement element = node.To<WebElement>();
  return HasTagName<kDefinitionDescriptionTag>(element) ||
         HasTagName<kDiv>(element) || HasTagName<kFieldset>(element) ||
         HasTagName<kListItem>(element) || HasTagName<kTableCell>(element) ||
         HasTagName<kTable>(element);
}

// This function checks whether the children of |element|
// are of the type <script>, <meta>, or <title>.
bool IsWebElementEmpty(const WebElement& root) {
  if (!root) {
    return true;
  }

  for (WebNode child = root.FirstChild(); child; child = child.NextSibling()) {
    if (child.IsTextNode() &&
        !base::ContainsOnlyChars(child.NodeValue().Utf8(),
                                 base::kWhitespaceASCII)) {
      return false;
    }

    if (!child.IsElementNode()) {
      continue;
    }

    WebElement element = child.To<WebElement>();
    if (!element.HasHTMLTagName(GetWebString<kScript>()) &&
        !element.HasHTMLTagName(GetWebString<kMeta>()) &&
        !element.HasHTMLTagName(GetWebString<kTitle>())) {
      return false;
    }
  }
  return true;
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
  if (depth <= 0 || !node) {
    return std::u16string();
  }

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
        IsAutofillableElement(element.DynamicTo<WebFormControlElement>())) {
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
  if (node.IsTextNode()) {
    return node.NodeValue().Utf16();
  }

  WebNode child = node.FirstChild();

  const int kChildSearchDepth = 10;
  std::u16string node_text =
      FindChildTextInner(child, kChildSearchDepth, divs_to_skip);
  base::TrimWhitespace(node_text, base::TRIM_ALL, &node_text);
  return node_text;
}

struct InferredLabel {
  // Returns an `InferredLabel` if `label` contains at least one character that
  // is neither whitespace nor "*:-–()" (or "*:" if
  // kAutofillConsiderPhoneNumberSeparatorsValidLabels is enabled).
  static std::optional<InferredLabel> BuildIfValid(
      std::u16string label,
      FormFieldData::LabelSource source);

  std::u16string label;
  FormFieldData::LabelSource source = FormFieldData::LabelSource::kUnknown;

 private:
  InferredLabel(std::u16string label, FormFieldData::LabelSource source);
};

// Shared function for InferLabelFromPrevious() and InferLabelFromNext().
std::optional<InferredLabel> InferLabelFromSibling(
    const WebFormControlElement& element,
    bool forward) {
  std::u16string inferred_label;
  WebNode sibling = element;
  while (true) {
    sibling = forward ? sibling.NextSibling() : sibling.PreviousSibling();
    if (!sibling) {
      break;
    }

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
    if (auto r = InferredLabel::BuildIfValid(inferred_label,
                                             LabelSource::kCombined)) {
      return r;
    }

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    if (HasTagName<kImage>(sibling) || HasTagName<kBreak>(sibling)) {
      continue;
    }

    // We only expect <p> and <label> tags to contain the full label text.
    bool has_label_tag = HasTagName<kLabel>(sibling);
    if (HasTagName<kParagraph>(sibling) || has_label_tag) {
      return InferredLabel::BuildIfValid(
          FindChildText(sibling),
          has_label_tag ? LabelSource::kLabelTag : LabelSource::kPTag);
    }

    break;
  }
  return InferredLabel::BuildIfValid(inferred_label, LabelSource::kCombined);
}

// Helper function to add a button's |title| to the |list|.
void AddButtonTitleToList(std::u16string title,
                          mojom::ButtonTitleType button_type,
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
                                    mojom::ButtonTitleType button_type,
                                    bool extract_value_attribute,
                                    ButtonTitleList* list) {
  for (WebElement item = elements.FirstItem(); item;
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

// Returns a list of elements whose id matches one of the ids found in
// `id_list`.
std::vector<WebElement> GetWebElementsFromIdList(const WebDocument& document,
                                                 const WebString& id_list) {
  std::vector<WebElement> web_elements;
  std::u16string id_list_utf16 = id_list.Utf16();
  for (std::u16string_view id : base::SplitStringPiece(
           id_list_utf16, base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    web_elements.push_back(document.GetElementById(WebString(id)));
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
    if (node) {
      std::u16string child_text = FindChildText(node);
      if (!child_text.empty()) {
        if (!text.empty()) {
          text.append(kSpace);
        }
        text.append(child_text);
      }
    }
  }
  base::TrimWhitespace(text, base::TRIM_ALL, &text);
  return text;
}

// Returns the ARIA label text of the elements denoted by the aria-labelledby
// attribute of |element| or the value of the aria-label attribute of
// |element|, with priority given to the aria-labelledby attribute.
std::u16string GetAriaLabel(const WebDocument& document,
                            const WebElement& element) {
  if (HasAttribute<kAriaLabelledBy>(element)) {
    WebString aria_label_attribute = GetAttribute<kAriaLabelledBy>(element);
    std::u16string text = CoalesceTextByIdList(document, aria_label_attribute);
    if (!text.empty()) {
      return text;
    }
  }

  if (HasAttribute<kAriaLabel>(element)) {
    return GetAttribute<kAriaLabel>(element).Utf16();
  }

  return std::u16string();
}

// Returns the ARIA label text of the elements denoted by the aria-describedby
// attribute of |element|.
std::u16string GetAriaDescription(const WebDocument& document,
                                  const WebElement& element) {
  return CoalesceTextByIdList(document,
                              GetAttribute<kAriaDescribedBy>(element));
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a previous sibling of |element|,
// e.g. Some Text <input ...>
// or   Some <span>Text</span> <input ...>
// or   <p>Some Text</p><input ...>
// or   <label>Some Text</label> <input ...>
// or   Some Text <img><input ...>
// or   <b>Some Text</b><br/> <input ...>.
std::optional<InferredLabel> InferLabelFromPrevious(
    const WebFormControlElement& element) {
  return InferLabelFromSibling(element, /*forward=*/false);
}

// Same as InferLabelFromPrevious(), but in the other direction.
// Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
std::optional<InferredLabel> InferLabelFromNext(
    const WebFormControlElement& element) {
  return InferLabelFromSibling(element, /*forward=*/true);
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// the placeholder text. e.g. <input placeholder="foo">
std::optional<InferredLabel> InferLabelFromPlaceholder(
    const WebFormControlElement& element) {
  if (HasAttribute<kPlaceholder>(element)) {
    return InferredLabel::BuildIfValid(
        GetAttribute<kPlaceholder>(element).Utf16(), LabelSource::kPlaceHolder);
  }
  return std::nullopt;
}

std::optional<InferredLabel> InferLabelFromAriaLabel(
    const WebFormControlElement& element) {
  return InferredLabel::BuildIfValid(
      GetAriaLabel(element.GetDocument(), element), LabelSource::kAriaLabel);
}

// Detects a label declared after the `element`, which is visually positioned
// above the element (usually using CSS). Such labels often act as
// placeholders. E.g.
// <div>
//  <input>
//  <span>Placeholder</span>
// </div>
// We want to consider placeholders which are either positioned over the input
// element or placed on the top left (or top right in RTL languages) of the
// input element (they need to overlap a bit). We want to disregard elements
// that are primarily below the input element (even if they overlap) because
// that place is often used to indicate incorrect inputs.
std::optional<InferredLabel> InferLabelFromOverlayingSuccessor(
    const WebFormControlElement& element) {
  WebNode next = element.NextSibling();
  while (next && !next.IsElementNode()) {
    next = next.NextSibling();
  }
  if (next) {
    gfx::Rect element_bounds = element.BoundsInWidget();
    gfx::Rect next_bounds = next.To<WebElement>().BoundsInWidget();
    // Reduce size by 1 pixel in all dimensions to resolve intersection due to
    // rounding errors.
    next_bounds.Inset(1);
    // We don't rely on element_bounds.Contains(next_bounds) because some
    // websites render the label partially above the input element.
    // We check the following conditions: 1) horizontally we want the `next`
    // element to be contained by `element`
    //    to consider `next` a label:
    //    |<----- element ----->|
    //     |<----- next ------>|
    // 2) vertically we often see three cases:
    //              (a)
    //             -----
    //               ^       (b)
    //   --------    |      -----
    //      ^       next      ^
    //      |        |        |
    //      |        v        |      (c) (not a placeholder)
    //   element   -----     next   -----
    //      |                 |       ^
    //      |                 |       |
    //      v                 v      next
    //   --------           -----     |
    //                                v
    //                              -----
    // a) a label is presented on the top left corner of an input element,
    //    possibly even exceeding it a bit.
    // b) a label is presented inside the input element.
    // c) an error message is presented at the bottom of an input element.
    if (!next_bounds.IsEmpty() &&
        // `next` needs to overlap `element` to be even considered.
        element_bounds.Intersects(next_bounds) &&
        // `next` must be horizontally contained.
        next_bounds.x() >= element_bounds.x() &&
        next_bounds.right() <= element_bounds.right() &&
        // bottom of `next` does not exceed the bounds of `element` because that
        // may represent an error label (case c above). The top of `next` may,
        // however exceed the `element` (case a above), so that condition is not
        // tested.
        !(next_bounds.bottom() > element_bounds.bottom())) {
      return InferredLabel::BuildIfValid(FindChildText(next),
                                         LabelSource::kOverlayingLabel);
    }
  }
  return std::nullopt;
}

// Helper for |InferLabelForElement()| that infers a label, from
// the value attribute when it is present and user has not typed in (if
// element's value attribute is same as the element's value).
std::optional<InferredLabel> InferLabelFromValueAttribute(
    const WebFormControlElement& element) {
  if (HasAttribute<kValue>(element) &&
      GetAttribute<kValue>(element) == element.Value()) {
    return InferredLabel::BuildIfValid(GetAttribute<kValue>(element).Utf16(),
                                       LabelSource::kValue);
  }
  return std::nullopt;
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td><td><input ...></td></tr>
// or   <tr><th>Some Text</th><td><input ...></td></tr>
// or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
// or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
// `cell` represents the <td> tag containing the input element.
std::optional<InferredLabel> InferLabelFromTableColumn(const WebNode& cell) {
  DCHECK(HasTagName<kTableCell>(cell));
  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  std::optional<InferredLabel> r;
  WebNode previous = cell.PreviousSibling();
  while (!r && previous) {
    if (HasTagName<kTableCell>(previous) ||
        HasTagName<kTableHeader>(previous)) {
      r = InferredLabel::BuildIfValid(FindChildText(previous),
                                      LabelSource::kTdTag);
    }
    previous = previous.PreviousSibling();
  }
  return r;
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
std::optional<InferredLabel> InferLabelFromTableRow(const WebNode& cell) {
  DCHECK(HasTagName<kTableCell>(cell));

  // Count the cell holding the input element.
  size_t cell_count = CalculateTableCellColumnSpan(cell.To<WebElement>());
  size_t cell_position = 0;
  size_t cell_position_end = cell_count - 1;

  // Count cells to the left to figure out |element|'s cell's position.
  for (WebNode cell_it = cell.PreviousSibling(); cell_it;
       cell_it = cell_it.PreviousSibling()) {
    if (HasTagName<kTableCell>(cell_it)) {
      cell_position += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Count cells to the right.
  for (WebNode cell_it = cell.NextSibling(); cell_it;
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
  while (parent && !HasTagName<kTableRow>(parent)) {
    parent = parent.ParentNode();
  }
  if (!parent) {
    return std::nullopt;
  }

  // Now find the previous row.
  WebNode row_it = parent.PreviousSibling();
  while (row_it && !HasTagName<kTableRow>(row_it)) {
    row_it = row_it.PreviousSibling();
  }

  // If there exists a previous row, check its cells and size. If they align
  // with the current row, infer the label from the cell above.
  if (row_it) {
    WebNode matching_cell;
    size_t prev_row_count = 0;
    WebNode prev_row_it = row_it.FirstChild();
    while (prev_row_it) {
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
    if ((cell_count == prev_row_count) && matching_cell) {
      if (auto r = InferredLabel::BuildIfValid(FindChildText(matching_cell),
                                               LabelSource::kTdTag)) {
        return r;
      }
    }
  }

  // If there is no previous row, or if the previous row and current row do not
  // align, check all previous siblings, skipping non-element nodes, until we
  // find a non-empty text block.
  WebNode previous = parent.PreviousSibling();
  std::optional<InferredLabel> r;
  while (!r && previous) {
    if (HasTagName<kTableRow>(previous)) {
      r = InferredLabel::BuildIfValid(FindChildText(previous),
                                      LabelSource::kTdTag);
    }
    previous = previous.PreviousSibling();
  }
  return r;
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
std::optional<InferredLabel> InferLabelFromDivTable(
    const WebFormControlElement& element) {
  WebNode node = element.ParentNode();
  bool looking_for_parent = true;
  std::set<WebNode> divs_to_skip;

  // Search the sibling and parent <div>s until we find a candidate label.
  std::optional<InferredLabel> r;
  while (!r && node) {
    if (HasTagName<kDiv>(node)) {
      r = InferredLabel::BuildIfValid(
          looking_for_parent ? FindChildTextWithIgnoreList(node, divs_to_skip)
                             : FindChildText(node),
          LabelSource::kDivTable);

      // Avoid sibling DIVs that contain autofillable fields.
      if (!looking_for_parent && r) {
        WebElement result_element =
            node.QuerySelector(GetWebString<kFormControlSelector>());
        if (result_element) {
          r = std::nullopt;
          divs_to_skip.insert(node);
        }
      }

      looking_for_parent = false;
    } else if (!looking_for_parent) {
      // Infer a label from text nodes and unassigned <label> siblings.
      if (node.IsTextNode() ||
          (HasTagName<kLabel>(node) &&
           !node.To<WebLabelElement>().CorrespondingControl())) {
        r = InferredLabel::BuildIfValid(FindChildText(node),
                                        LabelSource::kDivTable);
      }
    } else if (IsTraversableContainerElement(node)) {
      // If the element is in a non-div container, its label most likely is too.
      break;
    }

    if (!node.PreviousSibling()) {
      // If there are no more siblings, continue walking up the tree.
      looking_for_parent = true;
    }

    node = looking_for_parent ? node.ParentNode() : node.PreviousSibling();
  }
  return r;
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// a surrounding definition list,
// e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
// e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
std::optional<InferredLabel> InferLabelFromDefinitionList(const WebNode& dd) {
  DCHECK(HasTagName<kDefinitionDescriptionTag>(dd));

  // Skip by any intervening text nodes.
  WebNode previous = dd.PreviousSibling();
  while (previous && previous.IsTextNode()) {
    previous = previous.PreviousSibling();
  }

  if (!previous || !HasTagName<kDefinitionTermTag>(previous)) {
    return std::nullopt;
  }
  return InferredLabel::BuildIfValid(FindChildText(previous),
                                     LabelSource::kDdTag);
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// the first surrounding <label>, <div>, <td>, <dd> or <li> tag (if any).
// See `FindChildText()`, `InferLabelFromDivTable()`,
// `InferLabelFromTableColumn()`, `InferLabelFromTableRow()` and
// `InferLabelFromDefinitionList()` for examples how a label is extracted from
// the different tags.
std::optional<InferredLabel> InferLabelFromAncestors(
    const WebFormControlElement& element) {
  std::set<std::string> seen_tag_names;
  WebNode parent = element;
  while ((parent = parent.ParentNode())) {
    if (!parent.IsElementNode())
      continue;

    std::string tag_name = parent.To<WebElement>().TagName().Utf8();
    if (base::Contains(seen_tag_names, tag_name))
      continue;
    seen_tag_names.insert(tag_name);

    std::optional<InferredLabel> r;
    if (tag_name == "LABEL") {
      r = InferredLabel::BuildIfValid(FindChildText(parent),
                                      LabelSource::kLabelTag);
    } else if (tag_name == "DIV") {
      r = InferLabelFromDivTable(element);
    } else if (tag_name == "TD") {
      r = InferLabelFromTableColumn(parent);
      if (!r) {
        r = InferLabelFromTableRow(parent);
      }
    } else if (tag_name == "DD") {
      r = InferLabelFromDefinitionList(parent);
    } else if (tag_name == "LI") {
      r = InferredLabel::BuildIfValid(FindChildText(parent),
                                      LabelSource::kLiTag);
    } else if (tag_name == "FIELDSET") {
      break;
    }
    if (r) {
      return r;
    }
  }
  return std::nullopt;
}
// Infers corresponding label for `element` from surrounding context in the DOM,
// e.g. the contents of the preceding <p> tag or text element. Returns an empty
// string if it could not find a label for `element`.
std::optional<InferredLabel> InferLabelForElement(
    const WebFormControlElement& element) {
  if (IsCheckableElement(element)) {
    if (auto r = InferLabelFromNext(element)) {
      return r;
    }
  }
  if (auto r = InferLabelFromPrevious(element)) {
    return r;
  }
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAlwaysParsePlaceholders)) {
    if (auto r = InferLabelFromPlaceholder(element)) {
      return r;
    }
  }
  if (auto r = InferLabelFromOverlayingSuccessor(element)) {
    return r;
  }
  // If we didn't find a placeholder, check for aria-label text.
  if (auto r = InferLabelFromAriaLabel(element)) {
    return r;
  }
  // If we didn't find a label, check the `element`'s ancestors.
  if (auto r = InferLabelFromAncestors(element)) {
    return r;
  }
  // If we didn't find a label, check the value attr used as the placeholder.
  if (auto r = InferLabelFromValueAttribute(element)) {
    return r;
  }
  return std::nullopt;
}

void InferLabelForElements(
    base::span<const WebFormControlElement> control_elements,
    std::vector<FormFieldData>& fields) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Autofill.TimingPrecise.InferLabelForElement");
  CHECK_EQ(control_elements.size(), fields.size());
  for (size_t i = 0; i < control_elements.size(); ++i) {
    if (fields[i].label().empty()) {
      if (auto label = InferLabelForElement(control_elements[i])) {
        fields[i].set_label(std::move(label->label));
        fields[i].set_label_source(label->source);
      }
    }
    fields[i].set_label(
        std::move(fields[i].label()).substr(0, kMaxStringLength));
  }
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
  for (WebElement item = input_elements.FirstItem(); item;
       item = input_elements.NextItem()) {
    DCHECK(item.IsFormControlElement());
    WebFormControlElement control_element = item.To<WebFormControlElement>();
    blink::mojom::FormControlType type =
        control_element.FormControlTypeForAutofill();
    bool is_submit_type = type == blink::mojom::FormControlType::kInputSubmit ||
                          type == blink::mojom::FormControlType::kButtonSubmit;
    bool is_button_type = type == blink::mojom::FormControlType::kInputButton ||
                          type == blink::mojom::FormControlType::kButtonButton;
    if (!is_submit_type && !is_button_type) {
      continue;
    }
    std::u16string title = control_element.Value().Utf16();
    AddButtonTitleToList(
        std::move(title),
        is_submit_type ? mojom::ButtonTitleType::INPUT_ELEMENT_SUBMIT_TYPE
                       : mojom::ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE,
        is_submit_type ? &highest_priority_buttons
                       : &moderate_priority_buttons);
  }

  WebElementCollection button_elements =
      web_form.GetElementsByHTMLTagName(GetWebString<kButton>());
  for (WebElement item = button_elements.FirstItem(); item;
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
    AddButtonTitleToList(
        std::move(title),
        is_submit_type ? mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE
                       : mojom::ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE,
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
      mojom::ButtonTitleType::HYPERLINK,
      /*extract_value_attribute=*/true, &least_priority_buttons);
  FindElementsWithButtonFeatures(
      web_form.GetElementsByHTMLTagName(GetWebString<kDiv>()),
      mojom::ButtonTitleType::DIV,
      /*extract_value_attribute=*/false, &least_priority_buttons);
  FindElementsWithButtonFeatures(
      web_form.GetElementsByHTMLTagName(GetWebString<kSpan>()),
      mojom::ButtonTitleType::SPAN,
      /*extract_value_attribute=*/false, &least_priority_buttons);
  RemoveDuplicatesAndLimitTotalLength(&least_priority_buttons);
  return least_priority_buttons;
}

bool ShouldSkipFillField(const FormFieldData::FillData& field,
                         const WebFormControlElement& element) {
  enum class SkipReason {
    kUnfillable = 0,
    // kNoValueToFill = 1,
    kPreviouslyAutofilled = 2,
    kUserEditedText = 3,
    kUserEditedSelect = 4,
    kMaxValue = kUserEditedSelect
  };
  constexpr char kSkipReasonHistogram[] = "Autofill.RendererFillSkipReason";
  // Skip all checkable or non-modifiable elements, except select fields because
  // some synthetic select element use a hidden select element.
  if (!element.IsConnected() || !IsAutofillableElement(element) ||
      !element.IsEnabled() || element.IsReadOnly() ||
      IsCheckableElement(element) ||
      (!IsWebElementFocusableForAutofill(element) &&
       !IsSelectElement(element))) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kUnfillable);
    return true;
  }
  if (element.Focused() || field.force_override) {
    return false;
  }
  // Skip filling previously autofilled fields unless autofill is instructed to
  // override it.
  if (element.IsAutofilled()) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kPreviouslyAutofilled);
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
  const std::u16string current_element_value = element.Value().Utf16();
  if ((element.DynamicTo<WebInputElement>() || IsTextAreaElement(element)) &&
      element.UserHasEditedTheField() &&
      !SanitizedFieldIsEmpty(current_element_value) &&
      !HasAttributeWithValue(GetWebString<kValue>(), current_element_value) &&
      !HasAttributeWithValue(GetWebString<kPlaceholder>(),
                             current_element_value)) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kUserEditedText);
    return true;
  }
  // Check if we should autofill/preview/clear a select element or leave it.
  if (IsSelectElement(element) && element.UserHasEditedTheField() &&
      !SanitizedFieldIsEmpty(current_element_value)) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kUserEditedSelect);
    return true;
  }
  return false;
}

// Sets the |field|'s value to the value in |data|, and specifies the section
// for filled fields.  Also sets the "autofilled" attribute,
// causing the background to be blue.
void FillFormField(const FormFieldData::FillData& data,
                   bool is_initiating_node,
                   WebFormControlElement& field,
                   FieldDataManager& field_data_manager) {
  // Fill fields for text input, textarea and select fields.
  // Filling not supported for checkboxes and radio buttons.
  if (!IsTextInput(field) && !IsMonthInput(field) &&
      !IsTextAreaElement(field) && !IsSelectElement(field)) {
    return;
  }
  WebAutofillState new_autofill_state = data.is_autofilled
                                            ? WebAutofillState::kAutofilled
                                            : WebAutofillState::kNotFilled;

  if (IsTextInput(field)) {
    field_data_manager.UpdateFieldDataMap(
        GetFieldRendererId(field), data.value.substr(0, field.MaxLength()),
        FieldPropertiesFlags::kAutofilled);
  }

  field.SetAutofillValue(WebString::FromUTF16(data.value), new_autofill_state);
  // Changing the field's value might trigger JavaScript, which is capable
  // of destroying the frame.
  if (!field.GetDocument().GetFrame()) {
    return;
  }

  if (!is_initiating_node || IsSelectElement(field)) {
    return;
  }
  auto length = base::checked_cast<unsigned>(field.Value().length());
  field.SetSelectionRange(length, length);
  // selectionchange event is capable of destroying the frame.
  if (!field.GetDocument().GetFrame()) {
    return;
  }
  // Clear the current IME composition (the underline), if there is one.
  field.GetDocument().GetFrame()->UnmarkText();
}

// Sets the |field|'s "suggested" (non JS visible) value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be blue.
void PreviewFormField(const FormFieldData::FillData& data,
                      WebFormControlElement& field,
                      FieldDataManager& field_data_manager) {
  // Preview text input, textarea and select fields.
  // Preview not supported for checkboxes and radio buttons.
  if (!IsTextInput(field) && !IsMonthInput(field) &&
      !IsTextAreaElement(field) && !IsSelectElement(field)) {
    return;
  }

  field.SetSuggestedValue(WebString::FromUTF16(data.value));
  WebAutofillState new_autofill_state = data.is_autofilled
                                            ? WebAutofillState::kPreviewed
                                            : WebAutofillState::kNotFilled;
  field.SetAutofillState(new_autofill_state);
}

// A less-than comparator for FormFieldData's pointer by their FieldRendererId.
// It also supports direct comparison of a FieldRendererId with a FormFieldData
// pointer.
struct CompareByRendererId {
  using is_transparent = void;
  bool operator()(const std::pair<FormFieldData*, ShadowFieldData>& f,
                  const std::pair<FormFieldData*, ShadowFieldData>& g) const {
    DCHECK(f.first && g.first);
    return f.first->renderer_id() < g.first->renderer_id();
  }
  bool operator()(const FieldRendererId f,
                  const std::pair<FormFieldData*, ShadowFieldData>& g) const {
    DCHECK(g.first);
    return f < g.first->renderer_id();
  }
  bool operator()(const std::pair<FormFieldData*, ShadowFieldData>& f,
                  FieldRendererId g) const {
    DCHECK(f.first);
    return f.first->renderer_id() < g;
  }
};

// Searches |fields| for a unique field with name |field_name|. If there is
// none or more than one field with that name, the fields' shadow hosts' name
// and id attributes are tested, and the first match is returned. Returns
// nullptr if no match was found.
FormFieldData* SearchForFormControlByName(
    const std::u16string& field_name,
    base::span<const std::pair<FormFieldData*, ShadowFieldData>> fields,
    LabelSource& label_source) {
  if (field_name.empty())
    return nullptr;

  auto get_field_name = [](const auto& p) { return p.first->name(); };
  auto it = base::ranges::find(fields, field_name, get_field_name);
  auto end = fields.end();
  if (it == end ||
      base::ranges::find(it + 1, end, field_name, get_field_name) != end) {
    auto ShadowHostHasTargetName = [&](const auto& p) {
      return base::Contains(p.second.shadow_host_name_attributes, field_name) ||
             base::Contains(p.second.shadow_host_id_attributes, field_name);
    };
    it = base::ranges::find_if(fields, ShadowHostHasTargetName);
    if (it != end) {
      label_source =
          base::Contains(it->second.shadow_host_name_attributes, field_name)
              ? LabelSource::kForShadowHostName
              : LabelSource::kForShadowHostId;
    }
  } else {
    label_source = LabelSource::kForName;
  }
  return it != end ? it->first : nullptr;
}

// Considers all <label> descendents of `root`, looks at their corresponding
// control and matches them to the fields in `fields`. The corresponding
// control is either a descendent of the label or an input specified by id in
// the label's for-attribute.
// In case no corresponding control exists, but a for-attribute is specified,
// we look for fields with matching name as a fallback. Moreover, the ids and
// names of shadow root ancestors of the fields are considered as a fallback.
void MatchLabelsAndFields(const WebDocument& root,
                          base::span<FormFieldData> fields,
                          std::vector<ShadowFieldData> shadow_fields) {
  CHECK_EQ(fields.size(), shadow_fields.size());

  if (fields.empty()) {
    // Performance optimization: If there are no fields, the below is a no-op.
    return;
  }

  base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                 CompareByRendererId>
      field_set = [&] {
        std::vector<std::pair<FormFieldData*, ShadowFieldData>> items;
        for (size_t i = 0; i < fields.size(); i++) {
          items.emplace_back(&fields[i], std::move(shadow_fields[i]));
        }
        return items;
      }();

  WebElementCollection labels =
      root.GetElementsByHTMLTagName(GetWebString<kLabel>());
  DCHECK(labels);

  for (WebElement item = labels.FirstItem(); item; item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    WebElement control = label.CorrespondingControl();
    FormFieldData* field_data = nullptr;
    LabelSource label_source = LabelSource::kForId;

    if (!control) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      field_data = SearchForFormControlByName(GetAttribute<kFor>(label).Utf16(),
                                              field_set, label_source);
    } else if (control.IsFormControlElement()) {
      WebFormControlElement form_control = control.To<WebFormControlElement>();
      if (form_control.FormControlTypeForAutofill() ==
          blink::mojom::FormControlType::kInputHidden) {
        continue;
      }
      // Typical case: look up `field_data` in `field_set`.
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
      DCHECK(control && control.IsFormControlElement());
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
      if (auto inferred_label =
              InferLabelFromNext(control.To<WebFormControlElement>())) {
        label_text = inferred_label->label;
      }
      if (label_text.empty()) {
        continue;
      }
    }
    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (!field_data->label().empty()) {
      field_data->set_label(field_data->label() + u" ");
    }
    field_data->set_label(field_data->label() + std::move(label_text));
    field_data->set_label_source(label_source);
  }
}

bool IsAdIframe(const WebElement& element) {
  DCHECK(element.HasHTMLTagName(GetWebString<kIframe>()));
  WebFrame* iframe = WebFrame::FromFrameOwnerElement(element);
  return iframe && iframe->IsAdFrame();
}

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
// TODO(crbug.com/40846971): Can input fields or iframes actually overflow?
bool IsWebElementVisible(const WebElement& element) {
  auto HasMinSize = [](auto size) {
    constexpr int kMinPixelSize = 10;
    return size.width() >= kMinPixelSize && size.height() >= kMinPixelSize;
  };
  return element && IsWebElementFocusableForAutofill(element) &&
         (IsCheckableElement(element) || HasMinSize(element.GetClientSize()) ||
          HasMinSize(element.GetScrollSize()));
}

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
WebFormElement GetClosestAncestorFormElement(WebNode n) {
  while (n) {
    if (HasTagName<kForm>(n)) {
      return n.To<WebFormElement>();
    }
    n = n.ParentNode();
  }
  return WebFormElement();
}

// Returns true if a DOM traversal (pre-order, depth-first) visits `x` before
// `y`.
// As a performance improvement, `ancestor_hint` can be set to a suspected
// ancestor of `x` and `y`. Otherwise, `ancestor_hint` can be arbitrary.
//
// This function is a simplified/specialized version of Blink's private
// Node::compareDocumentPosition().
bool IsDOMPredecessor(const WebNode& x,
                      const WebNode& y,
                      const WebNode& ancestor_hint) {
  DCHECK(x.GetDocument() == y.GetDocument());
  DCHECK(!ancestor_hint || x.GetDocument() == ancestor_hint.GetDocument());
  // Extends the `path` up to `end` (exclusive) or the document root.
  // Paths are backwards: the last element is the top-most node.
  auto BuildPath = [](std::vector<WebNode> path, const WebNode& end) {
    DCHECK(!path.empty());
    path.reserve(path.size() + 16);
    WebNode parent;
    while ((parent = path.back().ParentNode()) && parent != end) {
      path.push_back(parent);
    }
    return path;
  };
  // Returns true iff `lhs` is strictly to the left of `rhs`, provided both
  // nodes are siblings.
  auto IsLeftSiblingOf = [](const WebNode& lhs, const WebNode& rhs) {
    DCHECK(lhs.ParentNode() == rhs.ParentNode());
    for (WebNode n = rhs; n; n = n.NextSibling()) {
      if (n == lhs) {
        return false;
      }
    }
    return true;
  };
  // Both paths are successors of either `ancestor_hint` or the document root.
  // If their parents aren't the same, we extend the paths to the document root.
  std::vector<WebNode> x_path = BuildPath({x}, ancestor_hint);
  std::vector<WebNode> y_path = BuildPath({y}, ancestor_hint);
  if (x_path.back().ParentNode() != y_path.back().ParentNode()) {
    x_path = BuildPath(std::move(x_path), WebNode());
    y_path = BuildPath(std::move(y_path), WebNode());
  }
  auto x_it = x_path.rbegin();
  auto y_it = y_path.rbegin();
  // Find the first different nodes in the paths. If such nodes exist, they are
  // siblings and their sibling order determines |x| and |y|'s relationship.
  while (x_it != x_path.rend() && y_it != y_path.rend()) {
    if (*x_it != *y_it) {
      return IsLeftSiblingOf(*x_it, *y_it);
    }
    ++x_it;
    ++y_it;
  }
  // If the paths don't differ in a node, the shorter path indicates a
  // predecessor since DOM traversal is in-order.
  return x_it == x_path.rend() && y_it != y_path.rend();
}

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
//   - "position: absolute; {left,top,right,bottom}: -100px"
//   - "opacity: 0.0"
//   - "clip: rect(0,0,0,0)"
//
// TODO(crbug.com/40846971): This check is very similar to IsWebElementVisible()
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
  return !IsAdIframe(element) ||
         (!base::FeatureList::IsEnabled(
              features::kAutofillExtractOnlyNonAdFrames) &&
          IsVisibleIframe(element));
}

// Returns the <iframe> elements that are associated with `form_element`.
// An iframe is associated with `form_element` iff
// - if `form_element` is non-null:
//   `form_element` is the iframe's closest <form> ancestor
// - if `form_element` is null:
//   the iframe has no <form> ancestor.
std::vector<WebElement> GetIframeElements(const WebDocument& document,
                                          const WebFormElement& form_element) {
  std::vector<WebElement> relevant_iframes;
  WebElementCollection iframes =
      document.GetElementsByHTMLTagName(GetWebString<kIframe>());
  for (WebElement iframe = iframes.FirstItem(); iframe;
       iframe = iframes.NextItem()) {
    if (GetClosestAncestorFormElement(iframe) == form_element &&
        IsRelevantChildFrame(iframe)) {
      relevant_iframes.push_back(iframe);
    }
  }
  return relevant_iframes;
}

// Returns if a script-modified username or credit card number is suitable to
// store in Password Manager/Autofill given `typed_value`.
bool IsScriptModifiedValueAcceptable(
    const std::u16string& value,
    const std::u16string& typed_value,
    const FieldDataManager& field_data_manager) {
  // The minimal size of a field value that will be substring-matched.
  constexpr size_t kMinMatchSize = 3u;
  const auto lowercase = base::i18n::ToLower(value);
  const auto typed_lowercase = base::i18n::ToLower(typed_value);
  // If the page-generated value is just a completion of the typed value, that's
  // likely acceptable.
  if (lowercase.starts_with(typed_lowercase)) {
    return true;
  }
  if (typed_lowercase.size() >= kMinMatchSize &&
      lowercase.find(typed_lowercase) != std::u16string::npos) {
    return true;
  }

  // If the page-generated value comes from user typed or autofilled values in
  // other fields, that's also likely OK.
  return field_data_manager.FindMatchedValue(value);
}

// Returns the maximum length value that Autofill may fill into the field. There
// are two special cases:
// - It is 0 for fields that do not support free text input (e.g., <select> and
//   <input type=month>).
// - It is the maximum 32 bit number for fields that support text values (e.g.,
//   <input type=text> or <textarea>) but have no maxlength attribute set.
//   The choice of 32 (as opposed to 64) is intentional: it allows us to still
//   do arithmetic with FormFieldData::max_length without having to worry about
//   integer overflows everywhere.
uint64_t GetMaxLength(const WebFormControlElement& element) {
  if (IsTextInput(element) || element.FormControlTypeForAutofill() ==
                                  blink::mojom::FormControlType::kTextArea) {
    auto max_length = element.MaxLength();
    static_assert(uint64_t{std::numeric_limits<decltype(max_length)>::max()} <=
                  FormFieldData::kDefaultMaxLength);
    return max_length < 0 ? FormFieldData::kDefaultMaxLength : max_length;
  }
  return 0;
}

// Returns the SelectOptions for the given <select> element.
//
// For example,
//   <select>
//     <option value=Foo>Bar</option>
//     <option value=Foo>Foo</option>
//   </select>
// returns {{.value = "Foo", .text = "Bar"}, {.value = "Foo", .text = "Foo"}}.
// For more details, see the documentation of `SelectOption`.
std::vector<SelectOption> GetSelectOptions(
    const WebSelectElement& select_element) {
  WebVector<WebElement> option_elements = select_element.GetListItems();

  // Constrain the maximum list length to prevent a malicious site from DOS'ing
  // the browser, without entirely breaking autocomplete for some extreme
  // legitimate sites: http://crbug.com/49332 and http://crbug.com/363094
  if (option_elements.size() > kMaxListSize) {
    return {};
  }

  auto to_string = [](WebString s) {
    return s.Utf16().substr(0, kMaxStringLength);
  };
  std::vector<SelectOption> options;
  options.reserve(option_elements.size());
  for (const auto& maybe_option_element : option_elements) {
    if (auto option_element =
            maybe_option_element.DynamicTo<WebOptionElement>()) {
      std::u16string text = to_string(option_element.GetText());
      if (text.empty()) {
        text = GetAriaLabel(option_element.GetDocument(), option_element)
                   .substr(0, kMaxStringLength);
      }
      options.push_back({.value = to_string(option_element.Value()),
                         .text = std::move(text)});
    }
  }
  return options;
}

// Returns the SelectOptions for the <datalist> associated with the given
// <input> element. The browser may display these options to the user as
// Autofill suggestions.
//
// For example,
//   <input datalist=l>
//   <datalist id=l>
//     <option value=Foo>Bar</option>
//     <option value=Foo>Foo</option>
//   </datalist>
// returns {{.value = "Foo", .text = "Bar"}, {.value = "Foo", .text = ""}}.
// It is intentional that the `value` takes precedence over the `text` because
// datalist values are user-visible.
std::vector<SelectOption> GetDataListOptions(const WebInputElement& element) {
  auto to_string = [](WebString s) {
    return s.Utf16().substr(0, kMaxStringLength);
  };
  WebVector<WebOptionElement> option_elements =
      element.FilteredDataListOptions();
  std::vector<SelectOption> options;
  options.reserve(std::max(option_elements.size(), kMaxListSize));
  for (const WebOptionElement& option_element : option_elements) {
    if (options.size() > kMaxListSize) {
      break;
    }
    options.push_back(
        {.value = to_string(option_element.Value()),
         .text = to_string(option_element.Value() != option_element.Label()
                               ? option_element.Label()
                               : WebString())});
  }
  return options;
}

// Returns whether `node` has a shadow-tree-including ancestor that is a
// `<form>`.
bool HasFormAncestor(WebNode node) {
  node = node.ParentOrShadowHostNode();
  while (node) {
    if (HasTagName<kForm>(node)) {
      return true;
    }
    node = node.ParentOrShadowHostNode();
  }
  return false;
}

// Returns all connected form control elements
// - owned by `form_element` if `!form_element.IsNull()`;
// - owned by no form otherwise.
std::vector<WebFormControlElement> GetOwnedFormControls(
    const WebDocument& document,
    const WebFormElement& form_element) {
  std::vector<WebFormControlElement> form_controls;
  if (form_element) {
    form_controls =
        form_element.GetFormControlElements().ReleaseVector();  // nocheck
  } else {
    form_controls =
        document.UnassociatedFormControls().ReleaseVector();  // nocheck
    // A form control element may be unassociated inside its Shadow DOM, but
    // owned (in the Autofill sense) by a <form> containing the shadow host.
    std::erase_if(form_controls, [](const WebFormControlElement& e) {
      return e.OwnerShadowHost() && HasFormAncestor(e);
    });
  }
  std::erase_if(form_controls, std::not_fn(&WebNode::IsConnected));
  return form_controls;
}

// Fills out a FormField object from a given autofillable WebFormControlElement.
// |extract_options|: See the enum ExtractOption above for details. Field
// properties will be copied from |field_data_manager|, if the argument is not
// null and has entry for |element| (see properties in FieldPropertiesFlags).
void WebFormControlElementToFormField(
    const WebFormElement& form_element,
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    DenseSet<ExtractOption> extract_options,
    FormFieldData* field,
    ShadowFieldData* shadow_data) {
  DCHECK(field);
  DCHECK(element);
  DCHECK(element.GetDocument().GetFrame());
  DCHECK(element.IsConnected());
  DCHECK(IsAutofillableElement(element));

  const FieldRendererId renderer_id = GetFieldRendererId(element);
  // Save both id and name attributes, if present. If there is only one of them,
  // it will be saved to |name|. See HTMLFormControlElement::nameForAutofill.
  field->set_name(element.NameForAutofill().Utf16());
  field->set_id_attribute(element.GetIdAttribute().Utf16());
  field->set_name_attribute(GetAttribute<kName>(element).Utf16());
  field->set_renderer_id(renderer_id);
  field->set_host_form_id(GetFormRendererId(form_element));
  field->set_form_control_ax_id(element.GetAxId());
  field->set_form_control_type(
      ToAutofillFormControlType(element.FormControlTypeForAutofill()));
  field->set_max_length(GetMaxLength(element));
  field->set_autocomplete_attribute(GetAutocompleteAttribute(element));
  field->set_parsed_autocomplete(
      ParseAutocompleteAttribute(field->autocomplete_attribute()));

  if (base::EqualsCaseInsensitiveASCII(GetAttribute<kRole>(element).Utf16(),
                                       "presentation")) {
    field->set_role(FormFieldData::RoleAttribute::kPresentation);
  }

  field->set_placeholder(GetAttribute<kPlaceholder>(element).Utf16());
  if (HasAttribute<kClass>(element)) {
    field->set_css_classes(GetAttribute<kClass>(element).Utf16());
  }

  if (field_data_manager && field_data_manager->HasFieldData(renderer_id)) {
    field->set_properties_mask(
        field_data_manager->GetFieldPropertiesMask(renderer_id));
  }

  field->set_aria_label(GetAriaLabel(element.GetDocument(), element));
  field->set_aria_description(
      GetAriaDescription(element.GetDocument(), element));

  const bool kAutofillDetectFieldVisibilityEnabled =
      base::FeatureList::IsEnabled(features::kAutofillDetectFieldVisibility);

  // Traverse up through shadow hosts to see if we can gather missing
  // attributes.
  // TODO(crbug.com/40204601): Make sure this works for all shadow DOM cases,
  // including cases in which the owning form is multiple (shadow DOM) levels
  // apart from the form control element. Also check whether we cannot simplify
  // some of the shadow DOM traversals here.
  size_t levels_up = kMaxShadowLevelsUp;
  for (WebElement host = element.OwnerShadowHost();
       host && levels_up > 0 && form_element &&
       form_element.OwnerShadowHost() != host;
       host = host.OwnerShadowHost(), --levels_up) {
    std::u16string shadow_host_id = host.GetIdAttribute().Utf16();
    if (shadow_data && !shadow_host_id.empty()) {
      shadow_data->shadow_host_id_attributes.push_back(shadow_host_id);
    }
    std::u16string shadow_host_name = GetAttribute<kName>(host).Utf16();
    if (shadow_data && !shadow_host_name.empty()) {
      shadow_data->shadow_host_name_attributes.push_back(shadow_host_name);
    }

    if (field->id_attribute().empty()) {
      field->set_id_attribute(host.GetIdAttribute().Utf16());
    }
    if (field->name_attribute().empty()) {
      field->set_name_attribute(GetAttribute<kName>(host).Utf16());
    }
    if (field->name().empty()) {
      field->set_name(field->name_attribute().empty()
                          ? field->id_attribute()
                          : field->name_attribute());
    }
    if (field->autocomplete_attribute().empty()) {
      field->set_autocomplete_attribute(GetAutocompleteAttribute(host));
      field->set_parsed_autocomplete(
          ParseAutocompleteAttribute(field->autocomplete_attribute()));
    }
    if (field->css_classes().empty() && HasAttribute<kClass>(host)) {
      field->set_css_classes(GetAttribute<kClass>(host).Utf16());
    }
    if (field->aria_label().empty()) {
      field->set_aria_label(GetAriaLabel(host.GetDocument(), host));
    }
    if (field->aria_description().empty()) {
      field->set_aria_description(GetAriaDescription(host.GetDocument(), host));
    }
  }

  // The browser doesn't need to differentiate between preview and autofill.
  field->set_is_autofilled(element.IsAutofilled());
  field->set_is_user_edited(element.UserHasEditedTheField());
  field->set_is_focusable(IsWebElementFocusableForAutofill(element));
  field->set_is_visible(kAutofillDetectFieldVisibilityEnabled
                            ? IsWebElementVisible(element)
                            : field->is_focusable());
  field->set_should_autocomplete(
      element.AutoComplete() &&
      !(field->parsed_autocomplete().has_value() &&
        field->parsed_autocomplete().value().field_type ==
            HtmlFieldType::kOneTimeCode));
  field->set_text_direction(GetTextDirectionForElement(element));
  field->set_is_enabled(element.IsEnabled());
  field->set_is_readonly(element.IsReadOnly());

  if (auto input_element = element.DynamicTo<WebInputElement>()) {
    SetCheckStatus(field, IsCheckableElement(input_element),
                   input_element.IsChecked());
    if (extract_options.contains(ExtractOption::kDatalist)) {
      field->set_datalist_options(GetDataListOptions(input_element));
    }
  } else if (IsTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else {
    // Set option strings on the field if available.
    DCHECK(IsSelectElement(element));
    field->set_options(GetSelectOptions(element.To<WebSelectElement>()));
  }
  if (extract_options.contains(ExtractOption::kBounds)) {
    if (auto* local_frame = element.GetDocument().GetFrame()) {
      if (auto* render_frame =
              content::RenderFrame::FromWebFrame(local_frame)) {
        field->set_bounds(gfx::RectF(
            render_frame->ConvertViewportToWindow(element.BoundsInWidget())));
      }
    }
  }

  field->set_value(element.Value().Utf16().substr(0, kMaxStringLength));
  field->set_selected_text(
      element.SelectedText().Utf16().substr(0, kMaxSelectedTextLength));
  field->set_allows_writing_suggestions(element.WritingSuggestions());

  if (field_data_manager) {
    MaybeUpdateUserInput(*field, GetFieldRendererId(element),
                         *field_data_manager);
  }
}

#if BUILDFLAG(IS_ANDROID)
// Checks whether an `element` looks like a captcha based on
// heuristics. The heuristics cannot be perfect and therefore is a subject to
// change, e.g. adding a list of domains of captcha providers to be compared
// with 'src' attribute.
bool IsLikelyCaptchaIframe(const WebElement& element) {
  if (!IsWebElementVisible(element)) {
    return false;
  }
  static constexpr std::string_view kCaptcha = "captcha";
  return GetAttribute<kSrc>(element).Find(kCaptcha) != std::string::npos ||
         GetAttribute<kTitle>(element).Find(kCaptcha) != std::string::npos ||
         GetAttribute<kId>(element).Find(kCaptcha) != std::string::npos ||
         GetAttribute<kName>(element).Find(kCaptcha) != std::string::npos;
}
#endif

std::optional<FormData> ExtractFormDataWithFieldsAndFrames(
    const WebDocument& document,
    const WebFormElement& form_element,
    const FieldDataManager& field_data_manager,
    DenseSet<ExtractOption> extract_options) {
  if (form_element && !form_element.IsConnected()) {
    return std::nullopt;
  }

  std::vector<WebFormControlElement> control_elements =
      GetOwnedAutofillableFormControls(document, form_element);
  std::vector<WebElement> iframe_elements =
      GetIframeElements(document, form_element);

  // Extracts fields from `control_elements` into `fields` and sets
  // `child_frames[i].predecessor` to the field index of the last field that
  // precedes the `i`th child frame.
  //
  // After each iteration, `iframe_elements[next_iframe]` is the first iframe
  // that comes after `control_elements[i]`.
  //
  // After the loop,
  // - `fields` is completely populated;
  // - `child_frames` has the correct size and `child_frames[i].predecessor` is
  //   set to the correct value, but `child_frames[i].token` is not initialized
  //   yet.
  std::vector<FormFieldData> fields;
  std::vector<ShadowFieldData> shadow_fields;
  std::vector<FrameTokenWithPredecessor> child_frames;
  fields.reserve(control_elements.size());
  shadow_fields.reserve(control_elements.size());
  child_frames.resize(iframe_elements.size());
  size_t next_iframe = 0;
  for (const WebFormControlElement& control_element : control_elements) {
    DCHECK(control_element.IsConnected());
    DCHECK(IsAutofillableElement(control_element));

    fields.emplace_back();
    shadow_fields.emplace_back();
    WebFormControlElementToFormField(form_element, control_element,
                                     &field_data_manager, extract_options,
                                     &fields.back(), &shadow_fields.back());

    // Finds the last frame that precedes |control_element|.
    while (next_iframe < iframe_elements.size() &&
           !IsDOMPredecessor(control_element, iframe_elements[next_iframe],
                             form_element)) {
      ++next_iframe;
    }
    // The `next_frame`th frame precedes `control_element` and thus `fields[i]`,
    // where `i` is the index of `control_element`. The frames after that, i.e.,
    // the `k`th frames for `k > next_frame`, may also precede `fields[i]`; in
    // case they do not, `child_frames[k].predecessor` will be updated in a
    // later iteration.
    for (size_t k = next_iframe; k < iframe_elements.size(); ++k) {
      child_frames[k].predecessor = fields.size() - 1;
    }
    if (fields.size() > kMaxExtractableFields) {
      return std::nullopt;
    }
  }

  // Extracts field labels from the <label for="..."> tags.
  // This is done by iterating through all <label>s and looking them up in the
  // `field_set` built below.
  // Iterating through the fields and looking at their `WebElement::Labels()`
  // unfortunately doesn't scale, as each call corresponds to a DOM traversal.
  MatchLabelsAndFields(document, fields, std::move(shadow_fields));

  // Infers field labels from other tags or <labels> without for="...".
  InferLabelForElements(control_elements, fields);

  // Extracts the frame tokens of |iframe_elements|.
  DCHECK_EQ(child_frames.size(), iframe_elements.size());
  for (size_t i = 0; i < iframe_elements.size(); ++i) {
    WebFrame* iframe = WebFrame::FromFrameOwnerElement(iframe_elements[i]);
    if (iframe && iframe->IsWebLocalFrame()) {
      child_frames[i].token = LocalFrameToken(
          iframe->ToWebLocalFrame()->GetLocalFrameToken().value());
    } else if (iframe && iframe->IsWebRemoteFrame()) {
      child_frames[i].token = RemoteFrameToken(
          iframe->ToWebRemoteFrame()->GetRemoteFrameToken().value());
    }
  }
  std::erase_if(child_frames, [](const auto& child_frame) {
    return absl::visit([](const auto& token) { return token.is_empty(); },
                       child_frame.token);
  });
  if (child_frames.size() > kMaxExtractableChildFrames) {
    child_frames.clear();
  }
  const bool success = (!fields.empty() || !child_frames.empty()) &&
                       fields.size() < kMaxExtractableFields;
  if (!success) {
    return std::nullopt;
  }

  base::UmaHistogramCounts100(!form_element
                                  ? "Autofill.ExtractFormUnowned.FieldCount"
                                  : "Autofill.ExtractFormOwned.FieldCount",
                              fields.size());
  FormData form;
  if (!form_element) {
    DCHECK(form.renderer_id().is_null());
    DCHECK(form.main_frame_origin().opaque());
    form.set_is_action_empty(true);
  } else {
    form.set_name(GetFormIdentifier(form_element));
    form.set_id_attribute(form_element.GetIdAttribute().Utf16());
    form.set_name_attribute(GetAttribute<kName>(form_element).Utf16());
    form.set_renderer_id(GetFormRendererId(form_element));
    form.set_action(GetCanonicalActionForForm(form_element));
    if (!form.action().is_valid()) {
      form.set_action(blink::WebStringToGURL(form_element.Action()));
    }
    form.set_is_action_empty(form_element.Action().IsNull() ||
                             form_element.Action().IsEmpty());
  }
  form.set_fields(std::move(fields));
  form.set_child_frames(std::move(child_frames));
  // `likely_contains_captcha` is only needed for Android for the autosubmission
  // after filling credentials from TTF bottom sheet.
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordSuggestionBottomSheetV2)) {
    form.set_likely_contains_captcha(
        std::ranges::any_of(iframe_elements, IsLikelyCaptchaIframe));
  }
#endif
  return form;
}

}  // namespace

InferredLabel::InferredLabel(std::u16string label, LabelSource source)
    : label(std::move(label)), source(source) {}

// static
std::optional<InferredLabel> InferredLabel::BuildIfValid(std::u16string label,
                                                         LabelSource source) {
  // List of characters a label can't be entirely made of (this list can grow).
  const std::u16string_view invalid_chars =
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u"*:"
          : u"*:-\u2013()";  // U+2013 is the En Dash "–".
  auto is_valid_label_character = [&invalid_chars](char16_t c) {
    return !base::Contains(invalid_chars, c) &&
           !base::Contains(std::u16string_view(base::kWhitespaceUTF16), c);
  };
  if (std::ranges::any_of(label, is_valid_label_character)) {
    base::TrimWhitespace(label, base::TRIM_ALL, &label);
    return InferredLabel{std::move(label), source};
  }
  return std::nullopt;
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

std::optional<FormData> ExtractFormData(
    const WebDocument& document,
    const WebFormElement& form_element,
    const FieldDataManager& field_data_manager,
    const CallTimerState& timer_state,
    DenseSet<ExtractOption> extract_options) {
  ScopedCallTimer timer("ExtractFormData", timer_state);
  return ExtractFormDataWithFieldsAndFrames(
      document, form_element, field_data_manager, extract_options);
}

GURL GetCanonicalActionForForm(const WebFormElement& form) {
  WebString action = form.Action();
  if (action.IsNull()) {
    action = WebString("");  // missing 'action' attribute implies current URL.
  }
  GURL full_action(form.GetDocument().CompleteURL(action));
  return StripAuthAndParams(full_action);
}

bool IsTextAreaElement(const WebFormControlElement& element) {
  return element && element.FormControlTypeForAutofill() ==
                        blink::mojom::FormControlType::kTextArea;
}

bool IsTextAreaElementOrTextInput(const WebFormControlElement& element) {
  return IsTextAreaElement(element) || IsTextInput(element);
}

bool IsAutofillableElement(const WebFormControlElement& element) {
  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  return IsTextInput(input_element) || IsMonthInput(input_element) ||
         IsCheckableElement(input_element) || IsSelectElement(element) ||
         IsTextAreaElement(element);
}

FormControlType ToAutofillFormControlType(blink::mojom::FormControlType type) {
  switch (type) {
    case blink::mojom::FormControlType::kInputCheckbox:
      return FormControlType::kInputCheckbox;
    case blink::mojom::FormControlType::kInputEmail:
      return FormControlType::kInputEmail;
    case blink::mojom::FormControlType::kInputMonth:
      return FormControlType::kInputMonth;
    case blink::mojom::FormControlType::kInputNumber:
      return FormControlType::kInputNumber;
    case blink::mojom::FormControlType::kInputPassword:
      return FormControlType::kInputPassword;
    case blink::mojom::FormControlType::kInputRadio:
      return FormControlType::kInputRadio;
    case blink::mojom::FormControlType::kInputSearch:
      return FormControlType::kInputSearch;
    case blink::mojom::FormControlType::kInputTelephone:
      return FormControlType::kInputTelephone;
    case blink::mojom::FormControlType::kInputText:
      return FormControlType::kInputText;
    case blink::mojom::FormControlType::kInputUrl:
      return FormControlType::kInputUrl;
    case blink::mojom::FormControlType::kSelectOne:
      return FormControlType::kSelectOne;
    case blink::mojom::FormControlType::kSelectMultiple:
      return FormControlType::kSelectMultiple;
    case blink::mojom::FormControlType::kTextArea:
      return FormControlType::kTextArea;
    default:
      NOTREACHED();
  }
}

bool IsCheckable(FormControlType form_control_type) {
  switch (form_control_type) {
    case FormControlType::kInputCheckbox:
    case FormControlType::kInputRadio:
      return true;
    case FormControlType::kContentEditable:
    case FormControlType::kInputEmail:
    case FormControlType::kInputMonth:
    case FormControlType::kInputNumber:
    case FormControlType::kInputPassword:
    case FormControlType::kInputSearch:
    case FormControlType::kInputTelephone:
    case FormControlType::kInputText:
    case FormControlType::kInputUrl:
    case FormControlType::kSelectOne:
    case FormControlType::kSelectMultiple:
    case FormControlType::kTextArea:
      return false;
  }
  NOTREACHED();
}

bool IsWebauthnTaggedElement(const WebFormControlElement& element) {
  const std::optional<AutocompleteParsingResult> parsing_result =
      ParseAutocompleteAttribute(GetAutocompleteAttribute(element));
  return parsing_result.has_value() && parsing_result->webauthn;
}

bool IsElementEditable(const WebInputElement& element) {
  return element.IsEnabled() && !element.IsReadOnly();
}

bool IsWebElementFocusableForAutofill(const WebElement& element) {
  return element.IsFocusable();
}

FormRendererId GetFormRendererId(const WebElement& e) {
  // This function is intended only for WebFormElements and for contenteditables
  // that aren't WebFormControlElement. However, an element that used to be
  // contenteditable may dynamically change to a non-contenteditable. Therefore,
  // instead of checking that `e` is a WebFormControlElement or contenteditable,
  // we just that `e` is not a WebFormControlElement to protect against
  // confusions between Get{Form,Field}RendererId().
  CHECK(!e.DynamicTo<WebFormControlElement>());

  if (!e) {
    return FormRendererId();
  }
  return FormRendererId(e.GetDomNodeId());
}

FieldRendererId GetFieldRendererId(const WebElement& e) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. However, an element that used
  // to be contenteditable may dynamically change to a non-contenteditable.
  // Therefore, instead of checking that `e` is a WebFormControlElement or
  // contenteditable, we just that `e` is not a WebFormElement to protect
  // against confusions between Get{Form,Field}RendererId().
  CHECK(!e.DynamicTo<WebFormElement>());
  return FieldRendererId(e.GetDomNodeId());
}

base::i18n::TextDirection GetTextDirectionForElement(
    const WebFormControlElement& element) {
  // Use 'text-align: left|right' if set or 'direction' otherwise.
  // See https://crbug.com/482339
  switch (element.AlignmentForFormData()) {
    case WebFormControlElement::Alignment::kLeft:
      return base::i18n::LEFT_TO_RIGHT;
    case WebFormControlElement::Alignment::kRight:
      return base::i18n::RIGHT_TO_LEFT;
    case WebFormControlElement::Alignment::kNotSet:
      return element.DirectionForFormData();
  }
}

std::vector<WebFormControlElement> GetOwnedAutofillableFormControls(
    const WebDocument& document,
    const WebFormElement& form_element) {
  std::vector<WebFormControlElement> elements =
      GetOwnedFormControls(document, form_element);
  std::erase_if(elements, std::not_fn(&IsAutofillableElement));
  return elements;
}

WebFormElement GetOwningForm(const WebFormControlElement& form_control) {
  CHECK(form_control);
  // The owning form is the furthest ancestor form element, if there is one.
  WebFormElement owner;
  // Look for ancestors of the associated form of `form_control` inside the
  // same tree.
  for (WebNode same_dom_ancestor = form_control.Form();  // nocheck
       same_dom_ancestor; same_dom_ancestor = same_dom_ancestor.ParentNode()) {
    if (auto form = same_dom_ancestor.DynamicTo<WebFormElement>()) {
      owner = form;
    }
  }

  // If `form_control` is inside Shadow DOM, also consider ancestors of
  // `form_control`.
  for (WebNode ancestor = form_control.OwnerShadowHost(); ancestor;
       ancestor = ancestor.ParentOrShadowHostNode()) {
    if (auto form = ancestor.DynamicTo<WebFormElement>()) {
      owner = form;
    }
  }
  return owner;
}

std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
FindFormAndFieldForFormControlElement(
    const WebFormControlElement& element,
    const FieldDataManager& field_data_manager,
    const CallTimerState& timer_state,
    DenseSet<ExtractOption> extract_options) {
  DCHECK(element);

  if (!element.IsConnected() || !IsAutofillableElement(element)) {
    return std::nullopt;
  }

  WebDocument document = element.GetDocument();
  WebFormElement owning_form = GetOwningForm(element);
  std::optional<FormData> form = ExtractFormData(
      document, owning_form, field_data_manager, timer_state, extract_options);
  const bool extract_form_data_succeeded = form.has_value();

  if (!form) {
    // If we couldn't extract the form, ignore the fields other than `element`.
    // This gives Autocomplete and other handlers the chance to handle it.
    FormFieldData field;
    WebFormControlElementToFormField(owning_form, element, nullptr,
                                     extract_options, &field,
                                     /*shadow_data=*/nullptr);
    form.emplace();
    form->set_fields({std::move(field)});
  }

  if (auto it = base::ranges::find(form->fields(), GetFieldRendererId(element),
                                   &FormFieldData::renderer_id);
      it != form->fields().end()) {
    return std::make_optional(std::make_pair(std::move(*form), raw_ref(*it)));
  }

  // This is not reachable if the following holds:
  // `base::Contains(GetOwnedFormControls(GetOwningForm(element)), element)`.
  // This does not hold if `element` is an unowned element in a shadow DOM and
  // kAutofillIncludeShadowDomInUnassociatedListedElements is disabled. Then
  // `GetOwningForm(element)` returns the unowned form, but
  // `GetOwnedFormControls()` does not include the field.
  // See crbug.com/347059988 for more details.
  GURL url;
  if (WebDocument doc = element.GetDocument()) {
    url = doc.Url();
  }
  auto get_id = [](const WebElement& e) {
    return e ? e.GetIdAttribute().Utf8() : "";
  };
  auto is_top_level = [](const WebFormElement form) {
    WebNode n = form;
    while (n && (n = n.ParentOrShadowHostNode())) {
      if (n.DynamicTo<WebFormElement>()) {
        return false;
      }
    }
    return true;
  };
  auto has_nested_form = [](const WebFormElement form,
                            WebFormControlElement elem) {
    for (WebNode n = elem; n && n != form; n = n.ParentOrShadowHostNode()) {
      if (n.DynamicTo<WebFormElement>()) {
        return true;
      }
    }
    return false;
  };
  auto get_form_size = [&document](const WebFormElement& form) {
    return document
               ? static_cast<int>(GetOwnedFormControls(document, form).size())
               : -1;
  };
  WebFormElement assoc_form_element = element.Form();  // nocheck

  // clang-format off
  SCOPED_CRASH_KEY_STRING64("Autofill", "url", url.spec());
  SCOPED_CRASH_KEY_BOOL("Autofill", "ExtractFormData_succeeded", extract_form_data_succeeded);
  SCOPED_CRASH_KEY_NUMBER("Autofill", "extracted_form_size", form->fields().size());

  SCOPED_CRASH_KEY_STRING64("Autofill", "elem_tag_name", element.TagName().Utf8());
  SCOPED_CRASH_KEY_STRING64("Autofill", "elem_id", get_id(element));
  SCOPED_CRASH_KEY_STRING64("Autofill", "elem_form_attr", element.GetAttribute("form").Utf8());
  SCOPED_CRASH_KEY_NUMBER("Autofill", "elem_form_control_type", base::to_underlying(element.FormControlType()));  // nocheck

  SCOPED_CRASH_KEY_BOOL("Autofill", "elem_autofillable", IsAutofillableElement(element));
  SCOPED_CRASH_KEY_BOOL("Autofill", "elem_document", !!document);
  SCOPED_CRASH_KEY_BOOL("Autofill", "elem_connected", element.IsConnected());
  SCOPED_CRASH_KEY_BOOL("Autofill", "elem_in_shadow_dom", !!element.OwnerShadowHost());

#define SCOPED_CRASH_KEYS_FOR_FORM(prefix, f)                                                                              \
  SCOPED_CRASH_KEY_BOOL("Autofill", #prefix "_form_non_null", !!f);                                                                \
  SCOPED_CRASH_KEY_BOOL("Autofill", #prefix "_form_connected", f && f.IsConnected());                                    \
  SCOPED_CRASH_KEY_BOOL("Autofill", #prefix "_form_in_shadow_dom", f && !!f.OwnerShadowHost());                          \
  SCOPED_CRASH_KEY_BOOL("Autofill", #prefix "_form_in_same_dom", f && element.OwnerShadowHost() == f.OwnerShadowHost()); \
  SCOPED_CRASH_KEY_BOOL("Autofill", #prefix "_form_is_top_level", is_top_level(f));                                      \
  SCOPED_CRASH_KEY_BOOL("Autofill", #prefix "_form_has_nested_form", has_nested_form(f, element));                       \
  SCOPED_CRASH_KEY_NUMBER("Autofill", #prefix "_form_size", get_form_size(f));                                           \
  SCOPED_CRASH_KEY_STRING64("Autofill", #prefix "_form_id", get_id(f));
  SCOPED_CRASH_KEYS_FOR_FORM(assoc, assoc_form_element);
  SCOPED_CRASH_KEYS_FOR_FORM(owng, owning_form);
#undef FORM_CRASH_KEYS
  // clang-format on
  NOTREACHED(base::NotFatalUntil::M132);
  return std::nullopt;
}

std::optional<FormData> FindFormForContentEditable(
    const WebElement& content_editable) {
  if (content_editable.DynamicTo<WebFormElement>() ||
      content_editable.DynamicTo<WebFormControlElement>() ||
      !content_editable.IsContentEditable() ||
      content_editable != content_editable.RootEditableElement() ||
      !content_editable.IsConnected()) {
    return std::nullopt;
  }

  std::vector<FormFieldData> fields(1);
  FormFieldData& field = fields.back();
  WebDocument document = content_editable.GetDocument();
  field.set_id_attribute(content_editable.GetIdAttribute().Utf16());
  field.set_name_attribute(GetAttribute<kName>(content_editable).Utf16());
  field.set_name(!field.id_attribute().empty() ? field.id_attribute()
                                               : field.name_attribute());
  field.set_renderer_id(GetFieldRendererId(content_editable));
  field.set_host_form_id(GetFormRendererId(content_editable));
  field.set_form_control_type(FormControlType::kContentEditable);
  field.set_autocomplete_attribute(GetAutocompleteAttribute(content_editable));
  field.set_parsed_autocomplete(
      ParseAutocompleteAttribute(field.autocomplete_attribute()));
  if (auto* local_frame = document.GetFrame()) {
    if (auto* render_frame = content::RenderFrame::FromWebFrame(local_frame)) {
      field.set_bounds(gfx::RectF(render_frame->ConvertViewportToWindow(
          content_editable.BoundsInWidget())));
    }
  }
  if (base::EqualsCaseInsensitiveASCII(
          GetAttribute<kRole>(content_editable).Utf16(), "presentation")) {
    field.set_role(FormFieldData::RoleAttribute::kPresentation);
  }
  if (HasAttribute<kClass>(content_editable)) {
    field.set_css_classes(GetAttribute<kClass>(content_editable).Utf16());
  }
  field.set_aria_label(GetAriaLabel(document, content_editable));
  field.set_aria_description(GetAriaDescription(document, content_editable));
  // TextContentAbridged() includes hidden elements and does not add linebreaks.
  // If this is not sufficient in the future, consider calling
  // HTMLElement::innerText(), which returns the text "as rendered" (i.e., it
  // inserts whitespace at the right places and it ignores "display:none"
  // subtrees), but is significantly more expensive because it triggers a
  // layout.
  field.set_value(
      content_editable.TextContentAbridged(kMaxStringLength).Utf16());
  DCHECK_LE(field.value().length(), kMaxStringLength);
  field.set_selected_text(content_editable.SelectedText().Utf16().substr(
      0, kMaxSelectedTextLength));
  field.set_allows_writing_suggestions(content_editable.WritingSuggestions());

  FormData form;
  form.set_renderer_id(GetFormRendererId(content_editable));
  form.set_id_attribute(content_editable.GetIdAttribute().Utf16());
  form.set_name_attribute(GetAttribute<kName>(content_editable).Utf16());
  form.set_name(!form.id_attribute().empty() ? form.id_attribute()
                                             : form.name_attribute());
  form.set_is_action_empty(true);
  form.set_fields(std::move(fields));
  return form;
}

std::vector<std::pair<FieldRef, WebAutofillState>> ApplyFieldsAction(
    const WebDocument& document,
    base::span<const FormFieldData::FillData> fields,
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    FieldDataManager& field_data_manager) {
  // This container stores the FormFieldData::FillData* of `form.fields` that
  // will be filled into their corresponding blink elements.
  std::vector<std::pair<FieldRef, WebAutofillState>> filled_fields;
  filled_fields.reserve(fields.size());

  struct Field {
    explicit operator bool() const {
      DCHECK_EQ(!data, !element);
      return data;
    }

    raw_ptr<const FormFieldData::FillData> data = nullptr;
    WebFormControlElement element;
  };

  // We first collect the focused (if one exists) and the unfocused autofillable
  // fields, and the autofill them in the following order:
  //
  // 1. Autofill the focused field.
  // 2. Send a blur event for the initially focused field.
  // 3. For each unfocused field, focus -> autofill -> blur.
  // 4. Send a focus event for the initially focused field.
  //
  // We currently do not emit other events like keydown/keyup or paste and
  // beforeinput/textInput/input.
  Field focused_field;
  std::vector<Field> unfocused_fields;
  unfocused_fields.reserve(fields.size());

  // Step 0: Find the focused and the unfocused fields to fill.
  for (const FormFieldData::FillData& field : fields) {
    WebFormControlElement element =
        GetFormControlByRendererId(field.renderer_id);
    if (!element) {
      continue;
    }
    if ((action_type == mojom::FormActionType::kFill &&
         ShouldSkipFillField(field, element)) ||
        (action_type == mojom::FormActionType::kUndo &&
         !element.IsAutofilled())) {
      continue;
    }
    if (element.Focused()) {
      focused_field = {&field, element};
    } else {
      unfocused_fields.emplace_back(&field, element);
    }
  }

  // Step 1: Autofill the initiating element.
  if (focused_field) {
    // In preview mode, only fill the field if it changes the fields value.
    // With this, the WebAutofillState is not changed from kAutofilled to
    // kPreviewed. This prevents the highlighting to change.
    filled_fields.emplace_back(focused_field.element,
                               focused_field.element.GetAutofillState());
    if (action_persistence == mojom::ActionPersistence::kFill) {
      FillFormField(*focused_field.data, /*is_initiating_node=*/true,
                    focused_field.element, field_data_manager);
    } else {
      PreviewFormField(*focused_field.data, focused_field.element,
                       field_data_manager);
    }
  }

  // If there is no other field to be autofilled, sending the blur event and
  // then the focus event for the initiating element does not make sense.
  if (unfocused_fields.empty()) {
    return filled_fields;
  }

  // Step 2: A blur event is emitted for the focused element if it is the
  // initiating element before all other elements are autofilled.
  if (action_persistence == mojom::ActionPersistence::kFill && focused_field) {
    focused_field.element.DispatchBlurEvent();
  }

  // Step 3: Autofill the non-initiating elements.
  // WebFormControlElement::SetAutofillValue fires the focus and blur
  // events.
  for (Field& field : unfocused_fields) {
    filled_fields.emplace_back(field.element, field.element.GetAutofillState());
    if (action_persistence == mojom::ActionPersistence::kFill) {
      FillFormField(*field.data, /*is_initiating_node=*/false, field.element,
                    field_data_manager);
    } else {
      PreviewFormField(*field.data, field.element, field_data_manager);
    }
  }

  // Step 4: A focus event is emitted for the initiating element after
  // autofilling is completed. It is not intended to work for preview.
  if (action_persistence == mojom::ActionPersistence::kFill && focused_field) {
    focused_field.element.DispatchFocusEvent();
  }

  return filled_fields;
}

void ClearPreviewedElements(
    base::span<std::pair<WebFormControlElement, WebAutofillState>>
        previewed_elements) {
  for (auto& [control_element, prior_autofill_state] : previewed_elements) {
    // We do not add null elements to `previewed_elements_` in AutofillAgent.
    DCHECK(control_element);
    control_element.SetSuggestedValue(WebString());
    control_element.SetAutofillState(prior_autofill_state);
  }
}

bool IsOwnedByFrame(const WebNode& node, content::RenderFrame* frame) {
  if (!node || !frame) {
    return false;
  }
  const WebDocument& doc = node.GetDocument();
  WebLocalFrame* node_frame = doc ? doc.GetFrame() : nullptr;
  WebLocalFrame* expected_frame = frame->GetWebFrame();
  return expected_frame && node_frame &&
         expected_frame->GetLocalFrameToken() ==
             node_frame->GetLocalFrameToken();
}

bool MaybeWasOwnedByFrame(const WebNode& node, content::RenderFrame* frame) {
  if (!node || !frame) {
    return true;
  }
  const WebDocument& doc = node.GetDocument();
  WebLocalFrame* node_frame = doc ? doc.GetFrame() : nullptr;
  WebLocalFrame* expected_frame = frame->GetWebFrame();
  return !expected_frame || !node_frame ||
         expected_frame->GetLocalFrameToken() ==
             node_frame->GetLocalFrameToken();
}

bool IsWebpageEmpty(const WebLocalFrame* frame) {
  WebDocument document = frame->GetDocument();
  return IsWebElementEmpty(document.Head()) &&
         IsWebElementEmpty(document.Body());
}

std::u16string FindChildText(const WebNode& node) {
  return FindChildTextWithIgnoreList(node, std::set<WebNode>());
}

ButtonTitleList GetButtonTitles(const WebFormElement& web_form,
                                ButtonTitlesCache* button_titles_cache) {
  // It makes no sense to collect button titles for a synthetic forms built
  // from unowned fields, as it's time-consuming and leads to scraping
  // many irrelevant elements.
  if (!web_form) {
    return {};
  }

  if (!button_titles_cache) {
    // Button titles scraping is disabled for this form.
    return InferButtonTitlesForForm(web_form);
  }

  auto [form_position, cache_miss] = button_titles_cache->emplace(
      GetFormRendererId(web_form), ButtonTitleList());
  if (!cache_miss)
    return form_position->second;

  form_position->second = InferButtonTitlesForForm(web_form);
  return form_position->second;
}

WebFormElement GetFormByRendererId(FormRendererId form_renderer_id) {
  if (!form_renderer_id) {
    return WebFormElement();
  }
  WebNode node = WebNode::FromDomNodeId(form_renderer_id.value());
  WebFormElement form = node.DynamicTo<WebFormElement>();
  return form && form.IsConnected() && form.GetDocument().GetFrame()
             ? form
             : WebFormElement();
}

WebFormControlElement GetFormControlByRendererId(
    FieldRendererId queried_form_control) {
  if (!queried_form_control) {
    return WebFormControlElement();
  }
  WebNode node = WebNode::FromDomNodeId(queried_form_control.value());
  WebFormControlElement form_control = node.DynamicTo<WebFormControlElement>();
  return form_control && form_control.IsConnected() &&
                 form_control.GetDocument().GetFrame()
             ? form_control
             : WebFormControlElement();
}

WebElement GetContentEditableByRendererId(FieldRendererId field_renderer_id) {
  WebElement field =
      WebNode::FromDomNodeId(*field_renderer_id).DynamicTo<WebElement>();
  return field && field.IsContentEditable() ? field : WebElement();
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

  for (const WebFormElement& form : document.GetTopLevelForms()) {
    base::ranges::move(GetOwnedFormControls(document, form),
                       std::back_inserter(form_control_elements));
  }

  base::ranges::move(GetOwnedFormControls(document, WebFormElement()),
                     std::back_inserter(form_control_elements));

  auto extract_four_digit_combinations = [&](WebNode node) {
    if (!node.IsTextNode()) {
      return;
    }
    std::string node_text = node.NodeValue().Utf8();
    std::string_view input(node_text);
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
      if (!node) {
        break;
      }
      extract_four_digit_combinations(node);
      node = NextWebNode(node, forward);
      if (auto form_control_element = node.DynamicTo<WebFormControlElement>()) {
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

void MaybeUpdateUserInput(FormFieldData& field,
                          FieldRendererId element_id,
                          const FieldDataManager& field_data_manager) {
  // If the field was autofilled or the user typed into it, check the value
  // stored in `field_data_manager` against the value property of the DOM
  // `element`. If they differ, then the scripts on the website modified the
  // value afterwards. Store the original value as the `user_input`, unless
  // this is one of recognised situations when the site-modified value is more
  // useful for filling.
  if (FieldPropertiesMask properties_mask =
          field_data_manager.HasFieldData(element_id)
              ? field_data_manager.GetFieldPropertiesMask(element_id)
              : FieldPropertiesMask();
      properties_mask &
      (FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled)) {
    // The user input is preserved for all passwords. It is also preserved for
    // other fields, as long as `value` is not acceptable.
    std::u16string user_input = field_data_manager.GetUserInput(element_id);
    if (field.form_control_type() == FormControlType::kInputPassword ||
        !IsScriptModifiedValueAcceptable(field.value(), user_input,
                                         field_data_manager)) {
      field.set_user_input(std::move(user_input).substr(0, kMaxStringLength));
    }
  }
}

std::u16string GetAriaLabelForTesting(  // IN-TEST
    const WebDocument& document,
    const WebElement& element) {
  return GetAriaLabel(document, element);
}

std::u16string GetAriaDescriptionForTesting(  // IN-TEST
    const WebDocument& document,
    const WebElement& element) {
  return GetAriaDescription(document, element);
}

void InferLabelForElementsForTesting(  // IN-TEST
    base::span<const blink::WebFormControlElement> control_elements,
    std::vector<FormFieldData>& fields) {
  InferLabelForElements(control_elements, fields);
}

std::vector<blink::WebFormControlElement>
GetOwnedFormControlsForTesting(  // IN-TEST
    const blink::WebDocument& document,
    const blink::WebFormElement& form_element) {
  return GetOwnedFormControls(document, form_element);
}

WebNode NextWebNodeForTesting(  // IN-TEST
    const WebNode& current_node,
    bool forward) {
  return NextWebNode(current_node, forward);
}

std::u16string FindChildTextWithIgnoreListForTesting(  // IN-TEST
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  return FindChildTextWithIgnoreList(node, divs_to_skip);
}

bool IsWebElementVisibleForTesting(const WebElement& element) {  // IN-TEST
  return IsWebElementVisible(element);
}

bool IsVisibleIframeForTesting(  // IN-TEST
    const WebElement& iframe_element) {
  return IsVisibleIframe(iframe_element);
}

WebFormElement GetClosestAncestorFormElementForTesting(WebNode n) {  // IN-TEST
  return GetClosestAncestorFormElement(n);
}

bool IsDOMPredecessorForTesting(const WebNode& x,  // IN-TEST
                                const WebNode& y,
                                const WebNode& ancestor_hint) {
  return IsDOMPredecessor(x, y, ancestor_hint);
}

uint64_t GetMaxLengthForTesting(  // IN-TEST
    const WebFormControlElement& element) {
  return GetMaxLength(element);
}

void WebFormControlElementToFormFieldForTesting(  // IN-TEST
    const WebFormElement& form_element,
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    DenseSet<ExtractOption> extract_options,
    FormFieldData* field) {
  WebFormControlElementToFormField(form_element, element, field_data_manager,
                                   extract_options, field,
                                   /*shadow_data=*/nullptr);
}

std::vector<SelectOption> GetDataListOptionsForTesting(  // IN-TEST
    const WebInputElement& element) {
  return GetDataListOptions(element);
}

}  // namespace autofill::form_util
