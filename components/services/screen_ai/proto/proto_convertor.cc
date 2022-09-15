// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/proto_convertor.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/ranges/ranges.h"
#include "base/strings/string_util.h"
#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "components/services/screen_ai/proto/view_hierarchy.pb.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace ranges = base::ranges;

namespace {

ui::AXNodeID next_node_id{1};

// Returns the next valid ID that can be used for identifying `AXNode`s in the
// accessibility tree.
ui::AXNodeID GetNextNodeID() {
  return next_node_id++;
}

bool HaveIdenticalFormattingStyle(const chrome_screen_ai::WordBox& word_1,
                                  const chrome_screen_ai::WordBox& word_2) {
  if (word_1.language() != word_2.language())
    return false;

  // The absence of reliable color information makes the two words have unequal
  // style, because it could indicate vastly different colors between them.
  if (word_1.estimate_color_success() != word_2.estimate_color_success())
    return false;
  if (word_1.estimate_color_success() && word_2.estimate_color_success()) {
    if (word_1.foreground_rgb_value() != word_2.foreground_rgb_value())
      return false;
    if (word_1.background_rgb_value() != word_2.background_rgb_value())
      return false;
  }
  if (word_1.direction() != word_2.direction())
    return false;
  if (word_1.content_type() != word_2.content_type())
    return false;
  return true;
}

// Returns whether the provided `predicted_type` is:
// A) set, and
// B) has a confidence that is above our acceptance threshold.
bool SerializePredictedType(
    const chrome_screen_ai::UIComponent::PredictedType& predicted_type,
    ui::AXNodeData& out_data) {
  DCHECK_EQ(out_data.role, ax::mojom::Role::kUnknown);
  switch (predicted_type.type_of_case()) {
    case chrome_screen_ai::UIComponent::PredictedType::kEnumType:
      // TODO(https://crbug.com/1278249): We do not actually need an enum. All
      // predicted types could be strings. We could easily map from a string to
      // an `ax::mojom::Role`. Then, we won't need to keep the enums synced.
      out_data.role = static_cast<ax::mojom::Role>(predicted_type.enum_type());
      break;
    case chrome_screen_ai::UIComponent::PredictedType::kStringType:
      out_data.role = ax::mojom::Role::kGenericContainer;
      out_data.AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                  predicted_type.string_type());
      break;
    case chrome_screen_ai::UIComponent::PredictedType::TYPE_OF_NOT_SET:
      NOTREACHED() << "Malformed proto message: Required field "
                      "`chrome_screen_ai::UIComponent::PredictedType` not set.";
      return false;
  }

  return true;
}

void SerializeBoundingBox(const chrome_screen_ai::Rect& bounding_box,
                          const ui::AXNodeID& container_id,
                          ui::AXNodeData& out_data) {
  out_data.relative_bounds.bounds =
      gfx::RectF(bounding_box.x(), bounding_box.y(), bounding_box.width(),
                 bounding_box.height());
  DCHECK(!out_data.relative_bounds.bounds.IsEmpty());
  if (container_id != ui::kInvalidAXNodeID)
    out_data.relative_bounds.offset_container_id = container_id;
  if (bounding_box.angle()) {
    out_data.relative_bounds.transform = std::make_unique<gfx::Transform>();
    out_data.relative_bounds.transform->Rotate(bounding_box.angle());
  }
}

void SerializeDirection(const chrome_screen_ai::Direction& direction,
                        ui::AXNodeData& out_data) {
  DCHECK(chrome_screen_ai::Direction_IsValid(direction));
  switch (direction) {
    case chrome_screen_ai::DIRECTION_UNSPECIFIED:
    // We assume that LEFT_TO_RIGHT is the default direction.
    case chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT:
      out_data.AddIntAttribute(
          ax::mojom::IntAttribute::kTextDirection,
          static_cast<int32_t>(ax::mojom::WritingDirection::kLtr));
      break;
    case chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT:
      out_data.AddIntAttribute(
          ax::mojom::IntAttribute::kTextDirection,
          static_cast<int32_t>(ax::mojom::WritingDirection::kRtl));
      break;
    case chrome_screen_ai::DIRECTION_TOP_TO_BOTTOM:
      out_data.AddIntAttribute(
          ax::mojom::IntAttribute::kTextDirection,
          static_cast<int32_t>(ax::mojom::WritingDirection::kTtb));
      break;
    case google::protobuf::kint32min:
    case google::protobuf::kint32max:
      // Ordinarily, a default case should have been added to permit future
      // additions to `chrome_screen_ai::Direction`. However, in this
      // case, both the screen_ai library and this code should always be in
      // sync.
      NOTREACHED() << "Unrecognized chrome_screen_ai::Direction value: "
                   << direction;
      break;
  }
}

void SerializeContentType(const chrome_screen_ai::ContentType& content_type,
                          ui::AXNodeData& out_data) {
  DCHECK(chrome_screen_ai::ContentType_IsValid(content_type));
  switch (content_type) {
    case chrome_screen_ai::CONTENT_TYPE_PRINTED_TEXT:
    case chrome_screen_ai::CONTENT_TYPE_HANDWRITTEN_TEXT:
      out_data.role = ax::mojom::Role::kStaticText;
      break;
    case chrome_screen_ai::CONTENT_TYPE_IMAGE:
      out_data.role = ax::mojom::Role::kImage;
      break;
    case chrome_screen_ai::CONTENT_TYPE_LINE_DRAWING:
      out_data.role = ax::mojom::Role::kGraphicsObject;
      break;
    case chrome_screen_ai::CONTENT_TYPE_SEPARATOR:
      out_data.role = ax::mojom::Role::kSplitter;
      break;
    case chrome_screen_ai::CONTENT_TYPE_UNREADABLE_TEXT:
      out_data.role = ax::mojom::Role::kGraphicsObject;
      break;
    case chrome_screen_ai::CONTENT_TYPE_FORMULA:
    case chrome_screen_ai::CONTENT_TYPE_HANDWRITTEN_FORMULA:
      // Note that `Role::kMath` indicates that the formula is not represented
      // as a subtree of MathML elements in the accessibility tree, but as a raw
      // string which may optionally be written in MathML, but could also be
      // written in plain text.
      out_data.role = ax::mojom::Role::kMath;
      break;
    case chrome_screen_ai::CONTENT_TYPE_SIGNATURE:
      // Signatures may be readable, but even when they are not we could still
      // try our best.
      // TODO(accessibility): Explore adding a description attribute informing
      // the user that this is a signature, e.g. via ARIA Annotations.
      out_data.role = ax::mojom::Role::kStaticText;
      break;
    case google::protobuf::kint32min:
    case google::protobuf::kint32max:
      // Ordinarily, a default case should have been added to permit future
      // additions to `chrome_screen_ai::ContentType`. However, in this
      // case, both the screen_ai library and this code should always be in
      // sync.
      NOTREACHED() << "Unrecognized chrome_screen_ai::ContentType value: "
                   << content_type;
      break;
  }
}

void SerializeWordBox(const chrome_screen_ai::WordBox& word_box,
                      ui::AXNodeData& inline_text_box) {
  DCHECK_NE(inline_text_box.id, ui::kInvalidAXNodeID);
  // TODO(nektar): What if the angles of orientation are different, would the
  // following DCHECK unnecessarily? Do we need to apply the related transform,
  // or is the fact that the transform is the same between line and word boxes
  // results in no difference?
  DCHECK(inline_text_box.relative_bounds.bounds.Contains(gfx::RectF(
      word_box.bounding_box().x(), word_box.bounding_box().y(),
      word_box.bounding_box().width(), word_box.bounding_box().height())));

  std::vector<int32_t> character_offsets;
  // TODO(nektar): Handle writing directions other than LEFT_TO_RIGHT.
  int32_t line_offset =
      base::ClampRound(inline_text_box.relative_bounds.bounds.x());
  ranges::transform(word_box.symbols(), std::back_inserter(character_offsets),
                    [line_offset](const chrome_screen_ai::SymbolBox& symbol) {
                      return symbol.bounding_box().x() - line_offset;
                    });

  std::string inner_text =
      inline_text_box.GetStringAttribute(ax::mojom::StringAttribute::kName);
  inner_text += word_box.utf8_string();
  size_t word_length = word_box.utf8_string().length();
  if (word_box.has_space_after()) {
    inner_text += " ";
    ++word_length;
  }
  inline_text_box.SetName(inner_text);

  std::vector<int32_t> word_starts = inline_text_box.GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts);
  std::vector<int32_t> word_ends = inline_text_box.GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordEnds);
  int32_t new_word_start = 0;
  int32_t new_word_end = base::checked_cast<int32_t>(word_length);
  if (!word_ends.empty()) {
    new_word_start += word_ends[word_ends.size() - 1];
    new_word_end += new_word_start;
  }
  word_starts.push_back(new_word_start);
  word_ends.push_back(new_word_end);
  inline_text_box.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                      word_starts);
  inline_text_box.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                      word_ends);
  DCHECK_LE(new_word_start, new_word_end);
  DCHECK_LE(
      new_word_end,
      base::checked_cast<int32_t>(
          inline_text_box.GetStringAttribute(ax::mojom::StringAttribute::kName)
              .length()));

  if (!word_box.language().empty()) {
    DCHECK_EQ(inline_text_box.GetStringAttribute(
                  ax ::mojom::StringAttribute::kLanguage),
              word_box.language())
        << "A `WordBox` has a different language than its enclosing `LineBox`.";
  }

  if (word_box.estimate_color_success()) {
    if (!inline_text_box.HasIntAttribute(
            ax::mojom::IntAttribute::kBackgroundColor)) {
      inline_text_box.AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                      word_box.background_rgb_value());
    } else {
      DCHECK_EQ(inline_text_box.GetIntAttribute(
                    ax::mojom::IntAttribute::kBackgroundColor),
                word_box.background_rgb_value())
          << "A `WordBox` has a different background color than its enclosing "
             "`LineBox`.";
    }
    if (!inline_text_box.HasIntAttribute(ax::mojom::IntAttribute::kColor)) {
      inline_text_box.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                      word_box.foreground_rgb_value());
    } else {
      DCHECK_EQ(
          inline_text_box.GetIntAttribute(ax::mojom::IntAttribute::kColor),
          word_box.foreground_rgb_value())
          << "A `WordBox` has a different foreground color than its enclosing "
             "`LineBox`.";
    }
  }
  SerializeDirection(word_box.direction(), inline_text_box);
}

// Creates an inline text box for every style span in the provided
// `static_text_node`, starting from `start_from_word_index` in the node's
// `word_boxes`. Returns the number of inline text box nodes that have been
// initialized in `node_data`.
size_t SerializeWordBoxes(const google::protobuf::RepeatedPtrField<
                              chrome_screen_ai::WordBox>& word_boxes,
                          const int start_from_word_index,
                          const size_t node_index,
                          ui::AXNodeData& static_text_node,
                          std::vector<ui::AXNodeData>& node_data) {
  if (word_boxes.empty())
    return 0u;
  DCHECK_LT(start_from_word_index, word_boxes.size());
  DCHECK_LT(node_index, node_data.size());
  DCHECK_NE(static_text_node.id, ui::kInvalidAXNodeID);
  ui::AXNodeData& inline_text_box_node = node_data[node_index];
  DCHECK_EQ(inline_text_box_node.role, ax::mojom::Role::kUnknown);
  inline_text_box_node.role = ax::mojom::Role::kInlineTextBox;
  inline_text_box_node.id = GetNextNodeID();
  // TODO(nektar): Find the union of the bounding boxes in this formatting
  // context and set it as the bounding box of `inline_text_box_node`.
  inline_text_box_node.relative_bounds.bounds =
      static_text_node.relative_bounds.bounds;

  std::string language;
  if (static_text_node.GetStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                          &language)) {
    // TODO(nektar): Only set language if different from parent node (i.e. the
    // static text node), in order to minimize memory usage.
    inline_text_box_node.AddStringAttribute(
        ax::mojom::StringAttribute::kLanguage, language);
  }
  static_text_node.child_ids.push_back(inline_text_box_node.id);

  const auto formatting_context_start =
      std::cbegin(word_boxes) + start_from_word_index;
  const auto formatting_context_end =
      ranges::find_if_not(formatting_context_start, ranges::end(word_boxes),
                          [formatting_context_start](const auto& word_box) {
                            return HaveIdenticalFormattingStyle(
                                *formatting_context_start, word_box);
                          });
  for (auto word_iter = formatting_context_start;
       word_iter != formatting_context_end; ++word_iter) {
    SerializeWordBox(*word_iter, inline_text_box_node);
  }
  if (formatting_context_end != std::cend(word_boxes)) {
    return 1u +
           SerializeWordBoxes(
               word_boxes,
               std::distance(std::cbegin(word_boxes), formatting_context_end),
               (node_index + 1u), static_text_node, node_data);
  }
  return 1u;
}

void SerializeUIComponent(const chrome_screen_ai::UIComponent& ui_component,
                          const size_t index,
                          ui::AXNodeData& parent_node,
                          std::vector<ui::AXNodeData>& node_data) {
  DCHECK_LT(index, node_data.size());
  DCHECK_NE(parent_node.id, ui::kInvalidAXNodeID);
  ui::AXNodeData& current_node = node_data[index];
  if (!SerializePredictedType(ui_component.predicted_type(), current_node))
    return;
  current_node.id = GetNextNodeID();
  SerializeBoundingBox(ui_component.bounding_box(), parent_node.id,
                       current_node);
  parent_node.child_ids.push_back(current_node.id);
}

// Returns the number of accessibility nodes that have been initialized in
// `node_data`. A single `line_box` may turn into a number of inline text boxes
// depending on how many formatting contexts it contains. If `line_box` is of a
// non-textual nature, only one node will be initialized.
size_t SerializeLineBox(const chrome_screen_ai::LineBox& line_box,
                        const size_t index,
                        ui::AXNodeData& parent_node,
                        std::vector<ui::AXNodeData>& node_data) {
  DCHECK_LT(index, node_data.size());
  DCHECK_NE(parent_node.id, ui::kInvalidAXNodeID);
  ui::AXNodeData& line_box_node = node_data[index];
  DCHECK_EQ(line_box_node.role, ax::mojom::Role::kUnknown);

  SerializeContentType(line_box.content_type(), line_box_node);
  line_box_node.id = GetNextNodeID();
  SerializeBoundingBox(line_box.bounding_box(), parent_node.id, line_box_node);
  // `ax::mojom::NameFrom` should be set to the correct value based on the
  // role.
  line_box_node.SetName(line_box.utf8_string());
  if (!line_box.language().empty()) {
    // TODO(nektar): Only set language if different from parent node (i.e. the
    // page node), in order to minimize memory usage.
    line_box_node.AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                     line_box.language());
  }
  SerializeDirection(line_box.direction(), line_box_node);
  parent_node.child_ids.push_back(line_box_node.id);

  if (!ui::IsText(line_box_node.role))
    return 1u;
  return 1u + SerializeWordBoxes(line_box.words(),
                                 /* start_from_word_index */ 0, (index + 1u),
                                 line_box_node, node_data);
}

// Adds the subtree of |nodes[node_index_to_add]| to |nodes_order| with
// pre-order traversal.
// The comment at the beginning of |Screen2xSnapshotToViewHierarchy| explains
// more.
void AddSubTree(const std::vector<ui::AXNodeData>& nodes,
                std::map<int, int>& id_to_position,
                std::vector<int>& nodes_order,
                const int node_index_to_add) {
  nodes_order.push_back(node_index_to_add);
  const ui::AXNodeData& node = nodes[node_index_to_add];
  for (const ui::AXNodeID& child_id : node.child_ids)
    AddSubTree(nodes, id_to_position, nodes_order, id_to_position[child_id]);
}

// Converts a Chrome role to a Screen2x role as text.
// TODO(https://crbug.com/1341655): Remove if Screen2x training protos are
// generated directly by Chrome or Screen2x uses the same role texts.
// Screen2x role names are generated by |blink::AXObject::RoleName| and these
// two function should stay in sync.
std::string GetScreen2xRoleFromChromeRole(ax::mojom::Role role) {
  std::string role_name = ui::ToString(role);

  static base::flat_set<ax::mojom::Role> roles_with_similar_name = {
      ax::mojom::Role::kAlert,       ax::mojom::Role::kArticle,
      ax::mojom::Role::kBanner,      ax::mojom::Role::kBlockquote,
      ax::mojom::Role::kButton,      ax::mojom::Role::kCaption,
      ax::mojom::Role::kCell,        ax::mojom::Role::kCode,
      ax::mojom::Role::kComment,     ax::mojom::Role::kComplementary,
      ax::mojom::Role::kDefinition,  ax::mojom::Role::kDialog,
      ax::mojom::Role::kDirectory,   ax::mojom::Role::kDocument,
      ax::mojom::Role::kEmphasis,    ax::mojom::Role::kFeed,
      ax::mojom::Role::kFigure,      ax::mojom::Role::kForm,
      ax::mojom::Role::kGrid,        ax::mojom::Role::kGroup,
      ax::mojom::Role::kHeading,     ax::mojom::Role::kLink,
      ax::mojom::Role::kList,        ax::mojom::Role::kLog,
      ax::mojom::Role::kMain,        ax::mojom::Role::kMarquee,
      ax::mojom::Role::kMath,        ax::mojom::Role::kMenu,
      ax::mojom::Role::kMark,        ax::mojom::Role::kMeter,
      ax::mojom::Role::kNavigation,  ax::mojom::Role::kNone,
      ax::mojom::Role::kNote,        ax::mojom::Role::kParagraph,
      ax::mojom::Role::kRegion,      ax::mojom::Role::kRow,
      ax::mojom::Role::kSearch,      ax::mojom::Role::kSlider,
      ax::mojom::Role::kStatus,      ax::mojom::Role::kStrong,
      ax::mojom::Role::kSubscript,   ax::mojom::Role::kSuggestion,
      ax::mojom::Role::kSuperscript, ax::mojom::Role::kSwitch,
      ax::mojom::Role::kTab,         ax::mojom::Role::kTable,
      ax::mojom::Role::kTerm,        ax::mojom::Role::kTime,
      ax::mojom::Role::kTimer,       ax::mojom::Role::kToolbar,
      ax::mojom::Role::kTooltip,     ax::mojom::Role::kTree,
  };
  if (roles_with_similar_name.find(role) != roles_with_similar_name.end())
    return role_name;

  static base::flat_set<ax::mojom::Role> roles_with_all_lowercase_name = {
      ax::mojom::Role::kAlertDialog,   ax::mojom::Role::kApplication,
      ax::mojom::Role::kCheckBox,      ax::mojom::Role::kColumnHeader,
      ax::mojom::Role::kContentInfo,   ax::mojom::Role::kListBox,
      ax::mojom::Role::kListItem,      ax::mojom::Role::kMenuBar,
      ax::mojom::Role::kMenuItem,      ax::mojom::Role::kMenuItemCheckBox,
      ax::mojom::Role::kMenuItemRadio, ax::mojom::Role::kRadioGroup,
      ax::mojom::Role::kRowGroup,      ax::mojom::Role::kRowHeader,
      ax::mojom::Role::kScrollBar,     ax::mojom::Role::kSearchBox,
      ax::mojom::Role::kSpinButton,    ax::mojom::Role::kTabList,
      ax::mojom::Role::kTabPanel,      ax::mojom::Role::kTreeItem,
  };
  if (roles_with_all_lowercase_name.find(role) !=
      roles_with_all_lowercase_name.end()) {
    return base::ToLowerASCII(role_name);
  }

  static base::flat_map<ax::mojom::Role, std::string>
      roles_with_different_name = {
          // Aria Roles
          {ax::mojom::Role::kComboBoxGrouping, "combobox"},
          {ax::mojom::Role::kComboBoxSelect, "combobox"},
          {ax::mojom::Role::kContentDeletion, "deletion"},
          {ax::mojom::Role::kDocAbstract, "doc-abstract"},
          {ax::mojom::Role::kDocAcknowledgments, "doc-acknowledgments"},
          {ax::mojom::Role::kDocAfterword, "doc-afterword"},
          {ax::mojom::Role::kDocAppendix, "doc-appendix"},
          {ax::mojom::Role::kDocBackLink, "doc-backlink"},
          {ax::mojom::Role::kDocBiblioEntry, "doc-biblioentry"},
          {ax::mojom::Role::kDocBibliography, "doc-bibliography"},
          {ax::mojom::Role::kDocBiblioRef, "doc-biblioref"},
          {ax::mojom::Role::kDocChapter, "doc-chapter"},
          {ax::mojom::Role::kDocColophon, "doc-colophon"},
          {ax::mojom::Role::kDocConclusion, "doc-conclusion"},
          {ax::mojom::Role::kDocCover, "doc-cover"},
          {ax::mojom::Role::kDocCredit, "doc-credit"},
          {ax::mojom::Role::kDocCredits, "doc-credits"},
          {ax::mojom::Role::kDocDedication, "doc-dedication"},
          {ax::mojom::Role::kDocEndnote, "doc-endnote"},
          {ax::mojom::Role::kDocEndnotes, "doc-endnotes"},
          {ax::mojom::Role::kDocEpigraph, "doc-epigraph"},
          {ax::mojom::Role::kDocEpilogue, "doc-epilogue"},
          {ax::mojom::Role::kDocErrata, "doc-errata"},
          {ax::mojom::Role::kDocExample, "doc-example"},
          {ax::mojom::Role::kDocFootnote, "doc-footnote"},
          {ax::mojom::Role::kDocForeword, "doc-foreword"},
          {ax::mojom::Role::kDocGlossary, "doc-glossary"},
          {ax::mojom::Role::kDocGlossRef, "doc-glossref"},
          {ax::mojom::Role::kDocIndex, "doc-index"},
          {ax::mojom::Role::kDocIntroduction, "doc-introduction"},
          {ax::mojom::Role::kDocNoteRef, "doc-noteref"},
          {ax::mojom::Role::kDocNotice, "doc-notice"},
          {ax::mojom::Role::kDocPageBreak, "doc-pagebreak"},
          {ax::mojom::Role::kDocPageFooter, "doc-pagefooter"},
          {ax::mojom::Role::kDocPageHeader, "doc-pageheader"},
          {ax::mojom::Role::kDocPageList, "doc-pagelist"},
          {ax::mojom::Role::kDocPart, "doc-part"},
          {ax::mojom::Role::kDocPreface, "doc-preface"},
          {ax::mojom::Role::kDocPrologue, "doc-prologue"},
          {ax::mojom::Role::kDocPullquote, "doc-pullquote"},
          {ax::mojom::Role::kDocQna, "doc-qna"},
          {ax::mojom::Role::kDocSubtitle, "doc-subtitle"},
          {ax::mojom::Role::kDocTip, "doc-tip"},
          {ax::mojom::Role::kDocToc, "doc-toc"},
          {ax::mojom::Role::kGenericContainer, "generic"},
          {ax::mojom::Role::kGraphicsDocument, "graphics-document"},
          {ax::mojom::Role::kGraphicsObject, "graphics-object"},
          {ax::mojom::Role::kGraphicsSymbol, "graphics-symbol"},
          {ax::mojom::Role::kCell, "gridcell"},
          {ax::mojom::Role::kImage, "img"},
          {ax::mojom::Role::kContentInsertion, "insertion"},
          {ax::mojom::Role::kListBoxOption, "option"},
          {ax::mojom::Role::kProgressIndicator, "progressbar"},
          {ax::mojom::Role::kRadioButton, "radio"},
          {ax::mojom::Role::kSplitter, "separator"},
          {ax::mojom::Role::kTextField, "textbox"},
          {ax::mojom::Role::kTreeGrid, "treegrid"},
          // Reverse Roles
          {ax::mojom::Role::kHeader, "banner"},
          {ax::mojom::Role::kToggleButton, "button"},
          {ax::mojom::Role::kPopUpButton, "combobox"},
          {ax::mojom::Role::kFooter, "contentinfo"},
          {ax::mojom::Role::kMenuListOption, "menuitem"},
          {ax::mojom::Role::kComboBoxMenuButton, "combobox"},
          {ax::mojom::Role::kTextFieldWithComboBox, "combobox"}};

  const auto& item = roles_with_different_name.find(role);
  if (item != roles_with_different_name.end())
    return item->second;

  // Roles that are not in the above tree groups have uppercase first letter
  // names.
  role_name[0] = base::ToUpperASCII(role_name[0]);
  return role_name;
}

// TODO(https://crbug.com/1278249): Consider merging the following functions
// into a template, e.g. using std::is_same.
void AddAttribute(const std::string& name,
                  int value,
                  screenai::UiElement& ui_element) {
  screenai::UiElementAttribute attrib;
  attrib.set_name(name);
  attrib.set_int_value(value);
  ui_element.add_attributes()->Swap(&attrib);
}

void AddAttribute(const std::string& name,
                  const char* value,
                  screenai::UiElement& ui_element) {
  screenai::UiElementAttribute attrib;
  attrib.set_name(name);
  attrib.set_string_value(value);
  ui_element.add_attributes()->Swap(&attrib);
}

void AddAttribute(const std::string& name,
                  const std::string& value,
                  screenai::UiElement& ui_element) {
  screenai::UiElementAttribute attrib;
  attrib.set_name(name);
  attrib.set_string_value(value);
  ui_element.add_attributes()->Swap(&attrib);
}

}  // namespace

namespace screen_ai {

void ResetNodeIDForTesting() {
  next_node_id = 1;
}

// TODO(nektar): Change return value to `std::vector<ui::AXNodeData>` as other
// fields in `AXTreeUpdate` are unused.
ui::AXTreeUpdate ScreenAIVisualAnnotationToAXTreeUpdate(
    const std::string& serialized_proto,
    const gfx::Rect& image_rect) {
  ui::AXTreeUpdate update;

  chrome_screen_ai::VisualAnnotation visual_annotation;
  if (!visual_annotation.ParseFromString(serialized_proto)) {
    NOTREACHED() << "Could not parse Screen AI library output.";
    return update;
  }

  if (features::IsScreenAIUseLayoutExtractionEnabled()) {
    visual_annotation.clear_lines();
  } else {
    visual_annotation.clear_ui_component();
  }

  // TODO(https://crbug.com/1278249): Create an AXTreeSource and create the
  // update using AXTreeSerializer.

  // Each `UIComponent`, `LineBox`, as well as every `WordBox` that results in a
  // different formatting context, will take up one node in the accessibility
  // tree, resulting in hundreds of nodes, making it inefficient to push_back
  // one node at a time. We pre-allocate the needed nodes making node creation
  // an O(n) operation.
  size_t formatting_context_count = 0u;
  for (const chrome_screen_ai::LineBox& line : visual_annotation.lines()) {
    // By design, and same as in Blink, every line creates a separate formatting
    // context regardless as to whether the format styles are identical with
    // previous lines or not.
    ++formatting_context_count;
    DCHECK(!line.words().empty())
        << "Empty lines should have been pruned in the Screen AI library.";
    for (auto iter = std::cbegin(line.words());
         std::next(iter) != std::cend(line.words()); ++iter) {
      if (!HaveIdenticalFormattingStyle(*iter, *std::next(iter)))
        ++formatting_context_count;
    }
  }

  // Each unique `chrome_screen_ai::LineBox::block_id` signifies a different
  // block of text, and so it creates a new static text node in the
  // accessibility tree. Each block has a sorted set of line boxes, everyone of
  // which is turned into one or more inline text box nodes in the accessibility
  // tree. Line boxes are sorted using their
  // `chrome_screen_ai::LineBox::order_within_block` member and are identified
  // by their index in the container of line boxes. Use std::map to sort both
  // text blocks and the line boxes that belong to each one, both operations
  // having an O(n * log(n)) complexity.
  // TODO(accessibility): Create separate paragraphs based on the blocks'
  // spacing.
  // TODO(accessibility): Determine reading order based on visual positioning of
  // text blocks, not on the order of their block IDs.
  std::map<int32_t, std::map<int32_t, int>> blocks_to_lines_map;
  for (int i = 0; i < visual_annotation.lines_size(); ++i) {
    const chrome_screen_ai::LineBox& line = visual_annotation.lines(i);
    blocks_to_lines_map[line.block_id()].emplace(
        std::make_pair(line.order_within_block(), i));
  }

  size_t rootnodes_count = 0u;
  if (!visual_annotation.ui_component().empty())
    ++rootnodes_count;
  if (!visual_annotation.lines().empty())
    ++rootnodes_count;

  std::vector<ui::AXNodeData> nodes(
      rootnodes_count + visual_annotation.ui_component().size() +
      visual_annotation.lines().size() + formatting_context_count);

  size_t index = 0u;

  if (!visual_annotation.ui_component().empty()) {
    ui::AXNodeData& rootnode = nodes[index++];
    rootnode.role = ax::mojom::Role::kDialog;
    rootnode.id = GetNextNodeID();
    rootnode.relative_bounds.bounds = gfx::RectF(image_rect);
    for (const auto& ui_component : visual_annotation.ui_component())
      SerializeUIComponent(ui_component, index++, rootnode, nodes);
  }

  if (!visual_annotation.lines().empty()) {
    // We assume that OCR is performed on a page-by-page basis.
    ui::AXNodeData& page_node = nodes[index++];
    page_node.role = ax::mojom::Role::kRegion;
    page_node.id = GetNextNodeID();
    page_node.AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                               true);
    page_node.relative_bounds.bounds = gfx::RectF(image_rect);
    for (const auto& block_to_lines_pair : blocks_to_lines_map) {
      for (const auto& line_sequence_number_to_index_pair :
           block_to_lines_pair.second) {
        const chrome_screen_ai::LineBox& line_box =
            visual_annotation.lines(line_sequence_number_to_index_pair.second);
        // Every line with a textual accessibility role should turn into one or
        // more inline text boxes, each one  representing a formatting context.
        // If the line is not of a textual role, only one node is initialized
        // having a more specific role such as `ax::mojom::Role::kImage`.
        index += SerializeLineBox(line_box, index, page_node, nodes);
      }
    }
  }

  // Filter out invalid / unrecognized / unused nodes from the update.
  update.nodes.resize(nodes.size());
  const auto end_node_iter = ranges::copy_if(
      nodes, ranges::begin(update.nodes), [](const ui::AXNodeData& node_data) {
        return node_data.role != ax::mojom::Role::kUnknown &&
               node_data.id != ui::kInvalidAXNodeID;
      });
  update.nodes.resize(std::distance(std::begin(update.nodes), end_node_iter));

  // TODO(https://crbug.com/1278249): Add UMA metrics to record the number of
  // annotations, item types, etc.

  return update;
}

std::string Screen2xSnapshotToViewHierarchy(const ui::AXTreeUpdate& snapshot) {
  screenai::ViewHierarchy view_hierarchy;

  // Screen2x requires the nodes to come in PRE-ORDER, and have only positive
  // ids. |nodes_order| will specify the new order of the nodes, i.e.
  // nodes_order[X] will tell which index in |snapshot.nodes| will be the new
  // Xth node in the proto that is sent to Screen2x. Screen2x also requires that
  // the node at position X would have id X.
  std::vector<int> nodes_order;

  // A map for fast access from AXNode.id to position in |snapshot.nodex|.
  std::map<int, int> id_to_position;

  // A map for fast access from AXNode.id of a child node to its parent node.
  std::map<int, int> child_id_to_parent_id;

  // The new id for each node id in |snapshot.nodes|.
  std::map<int, int> new_id;

  int snapshot_width = -1;
  int snapshot_height = -1;
  int root_index = -1;

  for (size_t i = 0; i < snapshot.nodes.size(); i++) {
    const ui::AXNodeData& node = snapshot.nodes[i];

    id_to_position[static_cast<int>(node.id)] = static_cast<int>(i);
    for (const ui::AXNodeID& child_id : node.child_ids)
      child_id_to_parent_id[child_id] = static_cast<int>(node.id);

    // Set root as the first node and take its size as snapshot size.
    if (node.id == snapshot.root_id) {
      root_index = i;
      snapshot_width = node.relative_bounds.bounds.width();
      snapshot_height = node.relative_bounds.bounds.height();
    }
  }

  DCHECK_NE(root_index, -1) << "Root not found.";
  AddSubTree(snapshot.nodes, id_to_position, nodes_order, root_index);

  for (int i = 0; i < static_cast<int>(nodes_order.size()); i++)
    new_id[snapshot.nodes[nodes_order[i]].id] = i;

  for (int node_index : nodes_order) {
    const ui::AXNodeData& node = snapshot.nodes[node_index];
    const ui::AXNodeID& ax_node_id = node.id;
    screenai::UiElement uie;

    // ID.
    uie.set_id(new_id[ax_node_id]);

    // Child IDs.
    for (const ui::AXNodeID& id : node.child_ids)
      uie.add_child_ids(new_id[id]);

    // Attributes.
    // TODO(https://crbug.com/1278249): Get attribute strings from a Google3
    // export, also the experimental ones for the unittest.
    AddAttribute("axnode_id", static_cast<int>(ax_node_id), uie);
    const std::string& display_value =
        node.GetStringAttribute(ax::mojom::StringAttribute::kDisplay);
    if (!display_value.empty())
      AddAttribute("/extras/styles/display", display_value, uie);
    AddAttribute("/extras/styles/visibility",
                 node.IsInvisible() ? "hidden" : "visible", uie);

    // This is a fixed constant for Chrome requests to Screen2x.
    AddAttribute("class_name", "chrome.unicorn", uie);
    AddAttribute("chrome_role", GetScreen2xRoleFromChromeRole(node.role), uie);
    AddAttribute("text",
                 node.GetStringAttribute(ax::mojom::StringAttribute::kName),
                 uie);

    // Type and parent.
    if (node.id == snapshot.root_id) {
      uie.set_type(screenai::UiElementType::ROOT);
      uie.set_parent_id(-1);
    } else {
      uie.set_type(screenai::UiElementType::VIEW);
      uie.set_parent_id(new_id[child_id_to_parent_id[ax_node_id]]);
    }

    // TODO(https://crbug.com/1278249): Bounding box and Bounding Box Pixels
    // do not consider offset container, transforms, device scaling, clipping,
    // offscreen state, etc. This should be fixed the same way the data is
    // created for training Screen2x models.
    // This is most likely wrong for iframes. Offset containers are directly
    // encoded into the accessibility tree; the platform accessibility tree
    // merges all child trees so it looks like one unified tree. We're missing
    // that here if Screen2x trains on the unified accessibility tree. We should
    // either ensure they train only on a single accessibility tree not
    // including iframes or ensure the snapshot grabs all child trees.

    // Bounding Box.
    screenai::BoundingBox* bounding_box = new screenai::BoundingBox;
    bounding_box->set_top(node.relative_bounds.bounds.y() / snapshot_height);
    bounding_box->set_left(node.relative_bounds.bounds.x() / snapshot_width);
    bounding_box->set_bottom(node.relative_bounds.bounds.bottom() /
                             snapshot_height);
    bounding_box->set_right(node.relative_bounds.bounds.right() /
                            snapshot_width);
    uie.set_allocated_bounding_box(bounding_box);

    // Bounding Box Pixels.
    screenai::BoundingBoxPixels* bounding_box_pixels =
        new screenai::BoundingBoxPixels();
    bounding_box_pixels->set_top(node.relative_bounds.bounds.y());
    bounding_box_pixels->set_left(node.relative_bounds.bounds.x());
    bounding_box_pixels->set_bottom(node.relative_bounds.bounds.bottom());
    bounding_box_pixels->set_right(node.relative_bounds.bounds.right());
    uie.set_allocated_bounding_box_pixels(bounding_box_pixels);

    view_hierarchy.add_ui_elements()->Swap(&uie);
  }

  return view_hierarchy.SerializeAsString();
}

const std::map<std::string, ax::mojom::Role>&
GetScreen2xToChromeRoleConversionMapForTesting() {
  static std::map<std::string, ax::mojom::Role> screen2xToChromeRoles;

  if (screen2xToChromeRoles.empty()) {
    for (int i = static_cast<int>(ax::mojom::Role::kMinValue);
         i <= static_cast<int>(ax::mojom::Role::kMaxValue); i++) {
      auto role = static_cast<ax::mojom::Role>(i);
      screen2xToChromeRoles[GetScreen2xRoleFromChromeRole(role)] = role;
    }
  }

  return screen2xToChromeRoles;
}

}  // namespace screen_ai
