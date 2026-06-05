// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/markdown_builder.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/containers/adapters.h"
#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace {

using ::optimization_guide::proto::AnnotatedRole;
using ::optimization_guide::proto::ContentAttributeType;
using ::optimization_guide::proto::ContentNode;
using ::optimization_guide::proto::DocumentIdentifier;
using ::optimization_guide::proto::FormControlType;
using ::optimization_guide::proto::RedactionDecision;
using ::optimization_guide::proto::TableRowType;
using ::optimization_guide::proto::TextSize;

bool IsSectionHeader(const ContentNode& node) {
  for (const auto& annotated_role :
       node.content_attributes().annotated_roles()) {
    switch (annotated_role) {
      case AnnotatedRole::ANNOTATED_ROLE_HEADER:
      case AnnotatedRole::ANNOTATED_ROLE_NAV:
      case AnnotatedRole::ANNOTATED_ROLE_SEARCH:
      case AnnotatedRole::ANNOTATED_ROLE_MAIN:
      case AnnotatedRole::ANNOTATED_ROLE_ARTICLE:
      case AnnotatedRole::ANNOTATED_ROLE_SECTION:
      case AnnotatedRole::ANNOTATED_ROLE_ASIDE:
      case AnnotatedRole::ANNOTATED_ROLE_FOOTER:
        return true;
      default:
        break;
    }
  }
  return false;
}



std::string GetEmphasisSyntax(
    const optimization_guide::proto::TextInfo& text_info) {
  int emph_count = 0;
  if (text_info.text_style().has_emphasis()) {
    emph_count += 1;
  }
  switch (text_info.text_style().text_size()) {
    case TextSize::TEXT_SIZE_L:
      emph_count += 1;
      break;
    case TextSize::TEXT_SIZE_XL:
      emph_count += 2;
      break;
    default:
      break;
  }
  if (emph_count == 0) {
    return "";
  }
  return std::string(emph_count, '*');
}

std::string GetHeadingPrefixForTextSize(TextSize text_size) {
  switch (text_size) {
    case TextSize::TEXT_SIZE_XS:
      return "#####";
    case TextSize::TEXT_SIZE_S:
      return "####";
    case TextSize::TEXT_SIZE_L:
      return "##";
    case TextSize::TEXT_SIZE_XL:
      return "#";
    default:
      return "###";
  }
}

std::string GetHeadingPrefix(
    const google::protobuf::RepeatedPtrField<ContentNode>& children) {
  for (const auto& child : children) {
    if (child.content_attributes().attribute_type() ==
        ContentAttributeType::CONTENT_ATTRIBUTE_TEXT) {
      return GetHeadingPrefixForTextSize(
          child.content_attributes().text_data().text_style().text_size());
    }
  }
  return "";
}

int GetNumberOfCellsInRow(const ContentNode& node) {
  int count = 0;
  for (const auto& child : node.children_nodes()) {
    if (child.content_attributes().attribute_type() ==
        ContentAttributeType::CONTENT_ATTRIBUTE_TABLE_CELL) {
      count += 1;
    }
  }
  return count;
}

bool IsPasswordFieldWithNoRedactionInfo(const ContentNode& node) {
  return node.content_attributes().attribute_type() ==
             ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL &&
         node.content_attributes().form_control_data().form_control_type() ==
             FormControlType::FORM_CONTROL_TYPE_INPUT_PASSWORD &&
         !node.content_attributes()
              .form_control_data()
              .has_redaction_decision();
}

bool IsPasswordInput(const ContentNode& node) {
  return (node.content_attributes().attribute_type() ==
              ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL &&
          node.content_attributes().form_control_data().redaction_decision() ==
              RedactionDecision::
                  REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD) ||
         IsPasswordFieldWithNoRedactionInfo(node);
}

bool IsCreditCardFormControl(const ContentNode& node) {
  return node.content_attributes().attribute_type() ==
             ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL &&
         node.content_attributes().form_control_data().redaction_decision() ==
             RedactionDecision::
                 REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD;
}

bool ShouldRedactNode(const ContentNode& node) {
  return IsPasswordInput(node) || IsCreditCardFormControl(node);
}

std::string EscapeLinkIdentifiers(const std::string& input) {
  std::string result;
  result.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '{' && i + 1 < input.size() && input[i + 1] == '#') {
      size_t j = i + 2;
      while (j < input.size() &&
             std::isdigit(static_cast<unsigned char>(input[j]))) {
        j++;
      }
      if (j < input.size() && input[j] == '}' && j > i + 2) {
        result += "\\{#";
        result += input.substr(i + 2, j - (i + 2));
        result += "\\}";
        i = j;
        continue;
      }
    }
    result += input[i];
  }
  return result;
}

void WalkForHashes(const ContentNode& node,
                   std::unordered_map<std::string, int>& url_to_hash,
                   std::unordered_set<int>& used_hashes) {
  if (!node.has_content_attributes()) {
    return;
  }

  if (node.content_attributes().attribute_type() ==
          ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR &&
      node.content_attributes().anchor_data().has_url()) {
    std::string url = node.content_attributes().anchor_data().url();
    if (!url_to_hash.contains(url)) {
      int hash = base::PersistentHash(url) % 10000;
      while (used_hashes.contains(hash)) {
        hash = (hash + 1) % 10000;
      }
      url_to_hash[url] = hash;
      used_hashes.insert(hash);
    }
  }

  for (const auto& child : node.children_nodes()) {
    WalkForHashes(child, url_to_hash, used_hashes);
  }
}

}  // namespace

namespace ttc {

std::unordered_map<std::string, int> MarkdownBuilder::GenerateUrlHashes(
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  std::unordered_map<std::string, int> url_to_hash;
  std::unordered_set<int> used_hashes;
  WalkForHashes(page_content.root_node(), url_to_hash, used_hashes);
  return url_to_hash;
}

MarkdownBuilder::WalkState::WalkState() = default;
MarkdownBuilder::WalkState::~WalkState() = default;

MarkdownBuilder::MarkdownBuilder(
    const optimization_guide::proto::AnnotatedPageContent& page_content,
    const GURL& page_url)
    : page_content_(page_content), page_url_(page_url) {}

MarkdownBuilder::~MarkdownBuilder() = default;

std::string MarkdownBuilder::Build() {
  url_to_hash_ = GenerateUrlHashes(*page_content_);
  std::vector<const ContentNode*> parent_chain;
  WalkContentNodes(parent_chain, page_content_->root_node(),
                   page_content_->main_frame_data().document_identifier());
  return JoinLines();
}

void MarkdownBuilder::WalkContentNodes(
    std::vector<const ContentNode*>& parent_chain,
    const ContentNode& node,
    const DocumentIdentifier& document_identifier) {
  if (!node.has_content_attributes()) {
    return;
  }

  ProcessNodeBeforeSubtree(parent_chain, node, document_identifier);

  parent_chain.push_back(&node);

  DocumentIdentifier doc_id_for_children = document_identifier;
  if (node.content_attributes().attribute_type() ==
          ContentAttributeType::CONTENT_ATTRIBUTE_IFRAME &&
      node.content_attributes().iframe_data().has_frame_data() &&
      node.content_attributes()
          .iframe_data()
          .frame_data()
          .has_document_identifier()) {
    doc_id_for_children = node.content_attributes()
                              .iframe_data()
                              .frame_data()
                              .document_identifier();
  }

  if (!ShouldRedactNode(node)) {
    for (const auto& child : node.children_nodes()) {
      WalkContentNodes(parent_chain, child, doc_id_for_children);
    }
  }

  parent_chain.pop_back();

  ProcessNodeAfterSubtree(node, document_identifier);
}

void MarkdownBuilder::ProcessNodeBeforeSubtree(
    const std::vector<const ContentNode*>& parent_chain,
    const ContentNode& node,
    const DocumentIdentifier& document_identifier) {
  SetUpLine(parent_chain, node);

  AddMarkdownBeforeSubtree(parent_chain, node, document_identifier);

  std::string content;
  if (IsPasswordInput(node)) {
    content = "<redacted password>";
  } else if (IsCreditCardFormControl(node)) {
    content = "<redacted credit card data>";
  } else {
    content = GetContent(parent_chain, node);
  }

  if (!content.empty()) {
    walk_state_.lines.back() += content;
  }
}

void MarkdownBuilder::ProcessNodeAfterSubtree(
    const ContentNode& node,
    const DocumentIdentifier& document_identifier) {
  AddMarkdownAfterSubtree(node);
  CompleteLine(node);
}

void MarkdownBuilder::AddMarkdownBeforeSubtree(
    const std::vector<const ContentNode*>& parent_chain,
    const ContentNode& node,
    const DocumentIdentifier& document_identifier) {
  switch (node.content_attributes().attribute_type()) {
    case ContentAttributeType::CONTENT_ATTRIBUTE_IFRAME:
      walk_state_.lines.back() += "---";
      walk_state_.lines.push_back("");
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_CONTAINER:
      if (IsSectionHeader(node)) {
        walk_state_.lines.back() += "---";
        walk_state_.lines.push_back("");
      }
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_ORDERED_LIST:
      walk_state_.ordered_list_item_index = 1;
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_LIST_ITEM:
      walk_state_.lines.back() += GetListItemPrefix(parent_chain, node);
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_TABLE_ROW:
      walk_state_.lines.back() += "|";
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL:
      switch (
          node.content_attributes().form_control_data().form_control_type()) {
        case FormControlType::FORM_CONTROL_TYPE_INPUT_CHECKBOX:
          walk_state_.lines.back() +=
              node.content_attributes().form_control_data().is_checked()
                  ? "[x]"
                  : "[ ]";
          break;
        case FormControlType::FORM_CONTROL_TYPE_INPUT_RADIO:
          walk_state_.lines.back() +=
              node.content_attributes().form_control_data().is_checked()
                  ? "(x)"
                  : "( )";
          break;
        case FormControlType::FORM_CONTROL_TYPE_SELECT_ONE:
        case FormControlType::FORM_CONTROL_TYPE_SELECT_MULTIPLE:
          walk_state_.lines.back() += "{(";
          break;
        case FormControlType::FORM_CONTROL_TYPE_BUTTON_BUTTON:
        case FormControlType::FORM_CONTROL_TYPE_BUTTON_SUBMIT:
        case FormControlType::FORM_CONTROL_TYPE_BUTTON_RESET:
        case FormControlType::FORM_CONTROL_TYPE_BUTTON_POPOVER:
        case FormControlType::FORM_CONTROL_TYPE_INPUT_BUTTON:
        case FormControlType::FORM_CONTROL_TYPE_INPUT_RESET:
        case FormControlType::FORM_CONTROL_TYPE_INPUT_SUBMIT:
          walk_state_.lines.back() += "[";
          break;
        default:
          walk_state_.lines.back() += "___";
          break;
      }
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR:
      walk_state_.lines.back() += "[";
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_HEADING:
      walk_state_.lines.back() += GetHeadingPrefix(node.children_nodes());
      break;
    default:
      break;
  }
}

void MarkdownBuilder::AddMarkdownAfterSubtree(const ContentNode& node) {
  switch (node.content_attributes().attribute_type()) {
    case ContentAttributeType::CONTENT_ATTRIBUTE_IFRAME:
      walk_state_.lines.push_back("---");
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_CONTAINER:
      if (IsSectionHeader(node)) {
        walk_state_.lines.push_back("---");
      }
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_TABLE_ROW:
      if (node.content_attributes().table_row_data().type() ==
          TableRowType::TABLE_ROW_TYPE_HEADER) {
        int cell_count = GetNumberOfCellsInRow(node);
        std::string separator = "|";
        for (int i = 0; i < cell_count; ++i) {
          separator += " ----- |";
        }
        walk_state_.lines.push_back(separator);
      }
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_TABLE_CELL:
      walk_state_.lines.back() += " |";
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR:
      if (node.content_attributes().anchor_data().has_url()) {
        std::string anchor_url_str =
            node.content_attributes().anchor_data().url();
        GURL anchor_url(anchor_url_str);
        if (anchor_url.is_valid() && !anchor_url.host().empty() &&
            page_url_.is_valid() && anchor_url.host() != page_url_.host()) {
          walk_state_.lines.back() +=
              base::StrCat({" (", anchor_url.host(), ")"});
        }
        auto it = url_to_hash_.find(anchor_url_str);
        if (it != url_to_hash_.end()) {
          walk_state_.lines.back() +=
              "]{#" + base::NumberToString(it->second) + "}";
          break;
        }
      }
      walk_state_.lines.back() += "]";
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL:
      switch (
          node.content_attributes().form_control_data().form_control_type()) {
        case FormControlType::FORM_CONTROL_TYPE_SELECT_ONE:
        case FormControlType::FORM_CONTROL_TYPE_SELECT_MULTIPLE: {
          walk_state_.lines.back() += ")}";
          break;
        }
        case FormControlType::FORM_CONTROL_TYPE_BUTTON_BUTTON:
        case FormControlType::FORM_CONTROL_TYPE_BUTTON_SUBMIT:
        case FormControlType::FORM_CONTROL_TYPE_BUTTON_RESET:
        case FormControlType::FORM_CONTROL_TYPE_BUTTON_POPOVER:
        case FormControlType::FORM_CONTROL_TYPE_INPUT_BUTTON:
        case FormControlType::FORM_CONTROL_TYPE_INPUT_RESET:
        case FormControlType::FORM_CONTROL_TYPE_INPUT_SUBMIT:
          walk_state_.lines.back() += "]";
          break;
        default:
          break;
      }
      if (node.content_attributes().form_control_data().is_required()) {
        walk_state_.lines.back() += "*";
      }
      break;
    default:
      break;
  }
}

std::string MarkdownBuilder::GetContent(
    const std::vector<const ContentNode*>& parent_chain,
    const ContentNode& node) {
  std::string content;
  switch (node.content_attributes().attribute_type()) {
    case ContentAttributeType::CONTENT_ATTRIBUTE_TABLE:
      content = node.content_attributes().table_data().table_name();
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_FORM:
      content = node.content_attributes().form_data().form_name();
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_TEXT:
      content = FormatText(node.content_attributes().text_data(), parent_chain);
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_SVG_ROOT:
      if (node.children_nodes().empty()) {
        content = node.content_attributes().svg_root_data().inner_text();
      }
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_IMAGE:
      content = FormatImage(node.content_attributes().image_data());
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_VIDEO:
      content = FormatVideo(node.content_attributes().video_data());
      break;
    default:
      break;
  }
  if (content.empty() && node.content_attributes().has_label()) {
    content = node.content_attributes().label();
  }
  return content;
}

std::string MarkdownBuilder::FormatText(
    const optimization_guide::proto::TextInfo& text_info,
    const std::vector<const ContentNode*>& parent_chain) {
  std::string text = EscapeLinkIdentifiers(
      base::CollapseWhitespaceASCII(text_info.text_content(), false));
  if (text.empty()) {
    return text;
  }
  if (!parent_chain.empty()) {
    const auto& parent = *parent_chain.back();
    if (IsAnchor(parent.content_attributes()) ||
        IsFormControlInParentChain(parent_chain) ||
        IsHeading(parent.content_attributes())) {
      return text;
    }
  }
  std::string emphasis = GetEmphasisSyntax(text_info);
  return emphasis + text + emphasis;
}

std::string MarkdownBuilder::FormatImage(
    const optimization_guide::proto::ImageInfo& image_info) {
  std::string caption = image_info.image_caption();
  if (caption.empty()) {
    return "";
  }
  return "![" + caption + "]";
}

std::string MarkdownBuilder::FormatVideo(
    const optimization_guide::proto::VideoData& video_data) {
  if (video_data.url().empty()) {
    return "";
  }
  return "![video](" + video_data.url() + ")";
}

std::string MarkdownBuilder::GetListItemPrefix(
    const std::vector<const ContentNode*>& parent_chain,
    const ContentNode& node) {
  for (const auto* it : base::Reversed(parent_chain)) {
    const auto& list_node = *it;
    if (list_node.content_attributes().attribute_type() ==
        ContentAttributeType::CONTENT_ATTRIBUTE_ORDERED_LIST) {
      std::string prefix =
          base::NumberToString(walk_state_.ordered_list_item_index) + ". ";
      walk_state_.ordered_list_item_index++;
      return prefix;
    } else if (list_node.content_attributes().attribute_type() ==
               ContentAttributeType::CONTENT_ATTRIBUTE_UNORDERED_LIST) {
      return "- ";
    }
  }
  return "- ";
}

void MarkdownBuilder::AddSpace() {
  if (!walk_state_.lines.empty() && !walk_state_.lines.back().empty() &&
      !base::EndsWith(walk_state_.lines.back(), " ")) {
    walk_state_.lines.back() += " ";
  }
}

void MarkdownBuilder::SetUpLine(
    const std::vector<const ContentNode*>& parent_chain,
    const ContentNode& node) {
  if (walk_state_.lines.empty()) {
    walk_state_.lines.push_back("");
  }
  switch (node.content_attributes().attribute_type()) {
    case ContentAttributeType::CONTENT_ATTRIBUTE_ROOT:
    case ContentAttributeType::CONTENT_ATTRIBUTE_IFRAME:
    case ContentAttributeType::CONTENT_ATTRIBUTE_HEADING:
    case ContentAttributeType::CONTENT_ATTRIBUTE_PARAGRAPH:
    case ContentAttributeType::CONTENT_ATTRIBUTE_FORM:
    case ContentAttributeType::CONTENT_ATTRIBUTE_TABLE:
    case ContentAttributeType::CONTENT_ATTRIBUTE_LIST_ITEM:
    case ContentAttributeType::CONTENT_ATTRIBUTE_TABLE_ROW:
      walk_state_.lines.push_back("");
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_CONTAINER:
      if (IsSectionHeader(node)) {
        walk_state_.lines.push_back("");
      }
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL:
    case ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR:
      AddSpace();
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_SVG_ROOT:
    case ContentAttributeType::CONTENT_ATTRIBUTE_TEXT:
    case ContentAttributeType::CONTENT_ATTRIBUTE_IMAGE:
      if (!parent_chain.empty()) {
        const auto& parent = *parent_chain.back();
        if (IsAnchor(parent.content_attributes()) ||
            IsFormControlInParentChain(parent_chain)) {
          if (base::EndsWith(walk_state_.lines.back(), "[") ||
              base::EndsWith(walk_state_.lines.back(), "___") ||
              base::EndsWith(walk_state_.lines.back(), "(") ||
              base::EndsWith(walk_state_.lines.back(), "{")) {
            return;
          }
        }
      }
      AddSpace();
      break;
    default:
      break;
  }
}

void MarkdownBuilder::CompleteLine(const ContentNode& node) {
  switch (node.content_attributes().attribute_type()) {
    case ContentAttributeType::CONTENT_ATTRIBUTE_ROOT:
    case ContentAttributeType::CONTENT_ATTRIBUTE_IFRAME:
    case ContentAttributeType::CONTENT_ATTRIBUTE_HEADING:
    case ContentAttributeType::CONTENT_ATTRIBUTE_PARAGRAPH:
    case ContentAttributeType::CONTENT_ATTRIBUTE_FORM:
    case ContentAttributeType::CONTENT_ATTRIBUTE_TABLE:
    case ContentAttributeType::CONTENT_ATTRIBUTE_LIST_ITEM:
    case ContentAttributeType::CONTENT_ATTRIBUTE_TABLE_ROW:
      walk_state_.lines.push_back("");
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_CONTAINER:
      if (IsSectionHeader(node)) {
        walk_state_.lines.push_back("");
      }
      break;
    case ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL:
      switch (
          node.content_attributes().form_control_data().form_control_type()) {
        case FormControlType::FORM_CONTROL_TYPE_INPUT_CHECKBOX:
        case FormControlType::FORM_CONTROL_TYPE_INPUT_RADIO:
          break;
        default:
          walk_state_.lines.push_back("");
          break;
      }
      break;
    default:
      break;
  }
}

bool MarkdownBuilder::IsHeading(
    const optimization_guide::proto::ContentAttributes& attributes) {
  return attributes.attribute_type() ==
         ContentAttributeType::CONTENT_ATTRIBUTE_HEADING;
}

bool MarkdownBuilder::IsTableCell(
    const optimization_guide::proto::ContentAttributes& attributes) {
  return attributes.attribute_type() ==
         ContentAttributeType::CONTENT_ATTRIBUTE_TABLE_CELL;
}

bool MarkdownBuilder::IsAnchor(
    const optimization_guide::proto::ContentAttributes& attributes) {
  return attributes.attribute_type() ==
         ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR;
}

bool MarkdownBuilder::IsFormControlInParentChain(
    const std::vector<const ContentNode*>& parent_chain) {
  for (auto* node : parent_chain) {
    if (node->content_attributes().attribute_type() ==
        ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL) {
      return true;
    }
  }
  return false;
}

std::string MarkdownBuilder::JoinLines() {
  std::stringstream ss;
  for (const auto& line : walk_state_.lines) {
    std::string trimmed = base::CollapseWhitespaceASCII(line, true);
    if (trimmed.empty()) {
      continue;
    }
    if (ss.tellp() != std::streampos(0)) {
      ss << "\n";
    }
    ss << line;
  }
  return ss.str();
}

}  // namespace ttc
