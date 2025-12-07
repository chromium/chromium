// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree_builder.h"

#include <optional>
#include <queue>
#include <string>

#include "base/i18n/break_iterator.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "components/pdf/renderer/pdf_accessibility_tree_builder_heuristic.h"
#include "components/strings/grit/components_strings.h"
#include "pdf/accessibility_structs.h"
#include "pdf/page_character_index.h"
#include "pdf/pdf_features.h"
#include "services/strings/grit/services_strings.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"

namespace {

ax::mojom::Role GetRoleForButtonType(chrome_pdf::ButtonType button_type) {
  switch (button_type) {
    case chrome_pdf::ButtonType::kRadioButton:
      return ax::mojom::Role::kRadioButton;
    case chrome_pdf::ButtonType::kCheckBox:
      return ax::mojom::Role::kCheckBox;
    case chrome_pdf::ButtonType::kPushButton:
      return ax::mojom::Role::kButton;
  }
}

std::string GetTextRunCharsAsUTF8(
    const chrome_pdf::AccessibilityTextRunInfo& text_run,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    int char_index) {
  std::string chars_utf8;
  for (uint32_t i = 0; i < text_run.len; ++i) {
    base::WriteUnicodeCharacter(
        static_cast<base_icu::UChar32>(chars[char_index + i].unicode_character),
        &chars_utf8);
  }
  return chars_utf8;
}

std::vector<int32_t> GetTextRunCharOffsets(
    const chrome_pdf::AccessibilityTextRunInfo& text_run,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    int char_index) {
  std::vector<int32_t> char_offsets(text_run.len);
  double offset = 0.0;
  for (uint32_t i = 0; i < text_run.len; ++i) {
    offset += chars[char_index + i].char_width;
    char_offsets[i] = floor(offset);
  }
  return char_offsets;
}

bool IsTextRenderModeFill(const chrome_pdf::AccessibilityTextRenderMode& mode) {
  switch (mode) {
    case chrome_pdf::AccessibilityTextRenderMode::kFill:
    case chrome_pdf::AccessibilityTextRenderMode::kFillStroke:
    case chrome_pdf::AccessibilityTextRenderMode::kFillClip:
    case chrome_pdf::AccessibilityTextRenderMode::kFillStrokeClip:
      return true;
    default:
      return false;
  }
}

bool IsTextRenderModeStroke(
    const chrome_pdf::AccessibilityTextRenderMode& mode) {
  switch (mode) {
    case chrome_pdf::AccessibilityTextRenderMode::kStroke:
    case chrome_pdf::AccessibilityTextRenderMode::kFillStroke:
    case chrome_pdf::AccessibilityTextRenderMode::kStrokeClip:
    case chrome_pdf::AccessibilityTextRenderMode::kFillStrokeClip:
      return true;
    default:
      return false;
  }
}

}  // namespace

namespace pdf {

PdfAccessibilityTreeBuilder::PdfAccessibilityTreeBuilder(
    bool mark_headings_using_heuristic,
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    const chrome_pdf::AccessibilityPageObjects& page_objects,
    const chrome_pdf::AccessibilityPageInfo& page_info,
    uint32_t page_index,
    ui::AXNodeData* root_node,
    blink::WebAXObject* container_obj,
    std::vector<std::unique_ptr<ui::AXNodeData>>* nodes,
    std::map<int32_t, chrome_pdf::PageCharacterIndex>*
        node_id_to_page_char_index,
    std::map<int32_t, PdfAccessibilityTree::AnnotationInfo>*
        node_id_to_annotation_info
    )
    : mark_headings_using_heuristic_(mark_headings_using_heuristic),
      text_runs_(text_runs),
      chars_(chars),
      links_(page_objects.links),
      images_(page_objects.images),
      highlights_(page_objects.highlights),
      text_fields_(page_objects.form_fields.text_fields),
      buttons_(page_objects.form_fields.buttons),
      choice_fields_(page_objects.form_fields.choice_fields),
      page_index_(page_index),
      root_node_(root_node),
      container_obj_(container_obj),
      nodes_(nodes),
      node_id_to_page_char_index_(node_id_to_page_char_index),
      node_id_to_annotation_info_(node_id_to_annotation_info)
{
  page_node_ = CreateAndAppendNode(ax::mojom::Role::kRegion,
                                   ax::mojom::Restriction::kReadOnly);
  page_node_->AddStringAttribute(
      ax::mojom::StringAttribute::kName,
      l10n_util::GetPluralStringFUTF8(IDS_PDF_PAGE_INDEX, page_index + 1));
  page_node_->AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                               true);
  page_node_->relative_bounds.bounds = gfx::RectF(page_info.bounds);
  root_node_->relative_bounds.bounds.Union(page_node_->relative_bounds.bounds);
  root_node_->child_ids.push_back(page_node_->id);

  if (!text_runs.empty()) {
    text_run_start_indices_.reserve(text_runs.size());
    text_run_start_indices_.push_back(0);
    for (size_t i = 0; i < text_runs.size() - 1; ++i) {
      text_run_start_indices_.push_back(text_run_start_indices_[i] +
                                        text_runs[i].len);
    }
  }
}

PdfAccessibilityTreeBuilder::~PdfAccessibilityTreeBuilder() = default;

void PdfAccessibilityTreeBuilder::BuildPageTree() {
  // Build tree using heuristics.
  // TODO(crbug.com/40707542): Add structure tree mode for tagged PDFs.
  PdfAccessibilityTreeBuilderHeuristic(*this).BuildPageTree();
}

void PdfAccessibilityTreeBuilder::AddWordStartsAndEnds(
    ui::AXNodeData* inline_text_box) {
  std::u16string text =
      inline_text_box->GetString16Attribute(ax::mojom::StringAttribute::kName);
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return;
  }

  std::vector<int32_t> word_starts;
  std::vector<int32_t> word_ends;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      word_starts.push_back(iter.prev());
      word_ends.push_back(iter.pos());
    }
  }
  inline_text_box->AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                       word_starts);
  inline_text_box->AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                       word_ends);
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateAndAppendNode(
    ax::mojom::Role role,
    ax::mojom::Restriction restriction) {
  std::unique_ptr<ui::AXNodeData> node = std::make_unique<ui::AXNodeData>();
  node->id = container_obj_->GenerateAXID();
  node->role = role;
  node->SetRestriction(restriction);

  // All nodes have coordinates relative to the root node.
  if (root_node_) {
    node->relative_bounds.offset_container_id = root_node_->id;
  }

  ui::AXNodeData* node_ptr = node.get();
  nodes_->push_back(std::move(node));

  return node_ptr;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateStaticTextNode() {
  ui::AXNodeData* static_text_node = CreateAndAppendNode(
      ax::mojom::Role::kStaticText, ax::mojom::Restriction::kReadOnly);
  static_text_node->SetNameFrom(ax::mojom::NameFrom::kContents);
  return static_text_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateStaticTextNode(
    const chrome_pdf::PageCharacterIndex& page_char_index) {
  ui::AXNodeData* static_text_node = CreateStaticTextNode();
  node_id_to_page_char_index_->emplace(static_text_node->id, page_char_index);
  return static_text_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateInlineTextBoxNode(
    const chrome_pdf::AccessibilityTextRunInfo& text_run,
    const chrome_pdf::PageCharacterIndex& page_char_index) {
  ui::AXNodeData* inline_text_box_node = CreateAndAppendNode(
      ax::mojom::Role::kInlineTextBox, ax::mojom::Restriction::kReadOnly);
  inline_text_box_node->SetNameFrom(ax::mojom::NameFrom::kContents);

  std::string chars__utf8 =
      GetTextRunCharsAsUTF8(text_run, *chars_, page_char_index.char_index);
  inline_text_box_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                           chars__utf8);
  inline_text_box_node->AddIntAttribute(
      ax::mojom::IntAttribute::kTextDirection,
      static_cast<uint32_t>(text_run.direction));
  inline_text_box_node->AddStringAttribute(
      ax::mojom::StringAttribute::kFontFamily, text_run.style.font_name);
  inline_text_box_node->AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                          text_run.style.font_size);
  inline_text_box_node->AddFloatAttribute(
      ax::mojom::FloatAttribute::kFontWeight, text_run.style.font_weight);
  if (text_run.style.is_italic) {
    inline_text_box_node->AddTextStyle(ax::mojom::TextStyle::kItalic);
  }
  if (text_run.style.is_bold) {
    inline_text_box_node->AddTextStyle(ax::mojom::TextStyle::kBold);
  }
  if (IsTextRenderModeFill(text_run.style.render_mode)) {
    inline_text_box_node->AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                          text_run.style.fill_color);
  } else if (IsTextRenderModeStroke(text_run.style.render_mode)) {
    inline_text_box_node->AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                          text_run.style.stroke_color);
  }

  inline_text_box_node->relative_bounds.bounds =
      text_run.bounds + page_node_->relative_bounds.bounds.OffsetFromOrigin();
  std::vector<int32_t> char_offsets =
      GetTextRunCharOffsets(text_run, *chars_, page_char_index.char_index);
  inline_text_box_node->AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, char_offsets);
  AddWordStartsAndEnds(inline_text_box_node);
  node_id_to_page_char_index_->emplace(inline_text_box_node->id,
                                       page_char_index);
  return inline_text_box_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateLinkNode(
    const chrome_pdf::AccessibilityLinkInfo& link) {
  ui::AXNodeData* link_node = CreateAndAppendNode(
      ax::mojom::Role::kLink, ax::mojom::Restriction::kReadOnly);

  link_node->AddStringAttribute(ax::mojom::StringAttribute::kUrl, link.url);
  link_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                std::string());
  link_node->relative_bounds.bounds = link.bounds;
  link_node->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kJump);
  node_id_to_annotation_info_->emplace(
      link_node->id,
      PdfAccessibilityTree::AnnotationInfo(page_index_, link.index_in_page));

  return link_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateImageNode(
    const chrome_pdf::AccessibilityImageInfo& image) {
  ui::AXNodeData* image_node = CreateAndAppendNode(
      ax::mojom::Role::kImage, ax::mojom::Restriction::kReadOnly);

  if (image.alt_text.empty()) {
    image_node->AddStringAttribute(
        ax::mojom::StringAttribute::kName,
        l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION));
  } else {
    image_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                   image.alt_text);
  }
  image_node->relative_bounds.bounds = image.bounds;
  return image_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateHighlightNode(
    const chrome_pdf::AccessibilityHighlightInfo& highlight) {
  ui::AXNodeData* highlight_node =
      CreateAndAppendNode(ax::mojom::Role::kPdfActionableHighlight,
                          ax::mojom::Restriction::kReadOnly);

  highlight_node->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_HIGHLIGHT));
  highlight_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     std::string());
  highlight_node->relative_bounds.bounds = highlight.bounds;
  highlight_node->AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                  highlight.color);

  return highlight_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreatePopupNoteNode(
    const chrome_pdf::AccessibilityHighlightInfo& highlight) {
  ui::AXNodeData* popup_note_node = CreateAndAppendNode(
      ax::mojom::Role::kNote, ax::mojom::Restriction::kReadOnly);

  popup_note_node->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_POPUP_NOTE));
  popup_note_node->relative_bounds.bounds = highlight.bounds;

  ui::AXNodeData* static_popup_note_text_node = CreateAndAppendNode(
      ax::mojom::Role::kStaticText, ax::mojom::Restriction::kReadOnly);

  static_popup_note_text_node->SetNameFrom(ax::mojom::NameFrom::kContents);
  static_popup_note_text_node->AddStringAttribute(
      ax::mojom::StringAttribute::kName, highlight.note_text);
  static_popup_note_text_node->relative_bounds.bounds = highlight.bounds;

  popup_note_node->child_ids.push_back(static_popup_note_text_node->id);

  return popup_note_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateTextFieldNode(
    const chrome_pdf::AccessibilityTextFieldInfo& text_field) {
  ax::mojom::Restriction restriction = text_field.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* text_field_node =
      CreateAndAppendNode(ax::mojom::Role::kTextField, restriction);

  text_field_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                      text_field.name);
  text_field_node->AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                      text_field.value);
  text_field_node->AddState(ax::mojom::State::kFocusable);
  if (text_field.is_required) {
    text_field_node->AddState(ax::mojom::State::kRequired);
  }
  if (text_field.is_password) {
    text_field_node->AddState(ax::mojom::State::kProtected);
  }
  text_field_node->relative_bounds.bounds = text_field.bounds;
  return text_field_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateButtonNode(
    const chrome_pdf::AccessibilityButtonInfo& button) {
  ax::mojom::Restriction restriction = button.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* button_node =
      CreateAndAppendNode(GetRoleForButtonType(button.type), restriction);
  button_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                  button.name);
  button_node->AddState(ax::mojom::State::kFocusable);

  if (button.type == chrome_pdf::ButtonType::kRadioButton ||
      button.type == chrome_pdf::ButtonType::kCheckBox) {
    ax::mojom::CheckedState checkedState = button.is_checked
                                               ? ax::mojom::CheckedState::kTrue
                                               : ax::mojom::CheckedState::kNone;
    button_node->SetCheckedState(checkedState);
    button_node->AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                    button.value);
    button_node->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                                 button.control_count);
    button_node->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                                 button.control_index + 1);
    button_node->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kCheck);
  } else {
    button_node->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kPress);
  }

  button_node->relative_bounds.bounds = button.bounds;
  return button_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateListboxOptionNode(
    const chrome_pdf::AccessibilityChoiceFieldOptionInfo& choice_field_option,
    ax::mojom::Restriction restriction) {
  ui::AXNodeData* listbox_option_node =
      CreateAndAppendNode(ax::mojom::Role::kListBoxOption, restriction);

  listbox_option_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                          choice_field_option.name);
  listbox_option_node->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                        choice_field_option.is_selected);
  listbox_option_node->AddState(ax::mojom::State::kFocusable);
  listbox_option_node->SetDefaultActionVerb(
      ax::mojom::DefaultActionVerb::kSelect);

  return listbox_option_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateListboxNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
    ui::AXNodeData* control_node) {
  ax::mojom::Restriction restriction = choice_field.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* listbox_node =
      CreateAndAppendNode(ax::mojom::Role::kListBox, restriction);

  if (choice_field.type != chrome_pdf::ChoiceFieldType::kComboBox) {
    listbox_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     choice_field.name);
  }

  ui::AXNodeData* first_selected_option = nullptr;
  for (const chrome_pdf::AccessibilityChoiceFieldOptionInfo& option :
       choice_field.options) {
    ui::AXNodeData* listbox_option_node =
        CreateListboxOptionNode(option, restriction);
    if (!first_selected_option && listbox_option_node->GetBoolAttribute(
                                      ax::mojom::BoolAttribute::kSelected)) {
      first_selected_option = listbox_option_node;
    }
    // TODO(crbug.com/40661774): Add `listbox_option_node` specific bounds
    // here.
    listbox_option_node->relative_bounds.bounds = choice_field.bounds;
    listbox_node->child_ids.push_back(listbox_option_node->id);
  }

  if (control_node && first_selected_option) {
    control_node->AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  first_selected_option->id);
  }

  if (choice_field.is_multi_select) {
    listbox_node->AddState(ax::mojom::State::kMultiselectable);
  }
  listbox_node->AddState(ax::mojom::State::kFocusable);
  listbox_node->relative_bounds.bounds = choice_field.bounds;
  return listbox_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateComboboxInputNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
    ax::mojom::Restriction restriction) {
  ax::mojom::Role input_role = choice_field.has_editable_text_box
                                   ? ax::mojom::Role::kTextFieldWithComboBox
                                   : ax::mojom::Role::kComboBoxMenuButton;
  ui::AXNodeData* combobox_input_node =
      CreateAndAppendNode(input_role, restriction);
  combobox_input_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                          choice_field.name);
  for (const chrome_pdf::AccessibilityChoiceFieldOptionInfo& option :
       choice_field.options) {
    if (option.is_selected) {
      combobox_input_node->AddStringAttribute(
          ax::mojom::StringAttribute::kValue, option.name);
      break;
    }
  }

  combobox_input_node->AddState(ax::mojom::State::kFocusable);
  combobox_input_node->relative_bounds.bounds = choice_field.bounds;
  if (input_role == ax::mojom::Role::kComboBoxMenuButton) {
    combobox_input_node->SetDefaultActionVerb(
        ax::mojom::DefaultActionVerb::kOpen);
  }

  return combobox_input_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateComboboxNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field) {
  ax::mojom::Restriction restriction = choice_field.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* combobox_node =
      CreateAndAppendNode(ax::mojom::Role::kComboBoxGrouping, restriction);
  ui::AXNodeData* input_element =
      CreateComboboxInputNode(choice_field, restriction);
  ui::AXNodeData* list_element = CreateListboxNode(choice_field, input_element);
  input_element->AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                     std::vector<int32_t>{list_element->id});
  combobox_node->child_ids.push_back(input_element->id);
  combobox_node->child_ids.push_back(list_element->id);
  combobox_node->AddState(ax::mojom::State::kFocusable);
  combobox_node->relative_bounds.bounds = choice_field.bounds;
  return combobox_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateChoiceFieldNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field) {
  switch (choice_field.type) {
    case chrome_pdf::ChoiceFieldType::kListBox:
      return CreateListboxNode(choice_field, /*control_node=*/nullptr);
    case chrome_pdf::ChoiceFieldType::kComboBox:
      return CreateComboboxNode(choice_field);
  }
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateOcrWrapperNode(
    const gfx::PointF& position,
    bool start) {
  ui::AXNodeData* wrapper_node = CreateAndAppendNode(
      start ? ax::mojom::Role::kBanner : ax::mojom::Role::kContentInfo,
      ax::mojom::Restriction::kReadOnly);
  wrapper_node->relative_bounds.bounds = gfx::RectF(position, gfx::SizeF(1, 1));

  ui::AXNodeData* text_node = CreateStaticTextNode();
  text_node->SetNameChecked(l10n_util::GetStringUTF8(
      start ? IDS_PDF_OCR_RESULT_BEGIN : IDS_PDF_OCR_RESULT_END));
  text_node->relative_bounds.bounds = wrapper_node->relative_bounds.bounds;
  wrapper_node->child_ids.push_back(text_node->id);
  return wrapper_node;
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace pdf
