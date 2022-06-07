// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "base/notreached.h"
#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "components/services/screen_ai/proto/dimension.pb.h"
#include "components/services/screen_ai/proto/view_hierarchy.pb.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace {

// The minimum confidence level that a Screen AI annotation should have to be
// accepted.
// TODO(https://crbug.com/1278249): Add experiment or heuristics to better
// adjust this threshold.
constexpr float kScreenAIMinConfidenceThreshold = 0.1f;

// Returns the next valid ID that can be used for identifying `AXNode`s in the
// accessibility tree.
ui::AXNodeID GetNextNodeID() {
  static ui::AXNodeID next_node_id{1};
  return next_node_id++;
}

// Returns whether the provided `predicted_type` is:
// A) set, and
// B) has a confidence that is above our acceptance threshold.
bool SerializePredictedType(
    const chrome_screen_ai::UIComponent::PredictedType& predicted_type,
    ui::AXNodeData& out_data) {
  DCHECK_EQ(out_data.role, ax::mojom::Role::kUnknown);
  if (predicted_type.confidence() < 0.0f ||
      predicted_type.confidence() > 1.0f) {
    NOTREACHED()
        << "Unrecognized chrome_screen_ai::PredictedType::confidence value: "
        << predicted_type.confidence();
    return false;  // Confidence is out of bounds.
  }
  if (predicted_type.confidence() < kScreenAIMinConfidenceThreshold)
    return false;
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
  // A negative width or height will result in an empty rect.
  if (out_data.relative_bounds.bounds.IsEmpty())
    return;
  if (container_id != ui::kInvalidAXNodeID)
    out_data.relative_bounds.offset_container_id = container_id;
  if (bounding_box.angle()) {
    out_data.relative_bounds.transform = std::make_unique<gfx::Transform>();
    out_data.relative_bounds.transform->Rotate(bounding_box.angle());
  }
}

void SerializeDirection(const chrome_screen_ai::Direction& direction,
                        ui::AXNodeData& out_data) {
  if (!chrome_screen_ai::Direction_IsValid(direction)) {
    NOTREACHED() << "Unrecognized chrome_screen_ai::Direction value: "
                 << direction;
    return;
  }
  switch (direction) {
    case chrome_screen_ai::Direction::UNSPECIFIED:
    // We assume that LEFT_TO_RIGHT is the default direction.
    case chrome_screen_ai::Direction::LEFT_TO_RIGHT:
      out_data.AddIntAttribute(
          ax::mojom::IntAttribute::kTextDirection,
          static_cast<int32_t>(ax::mojom::WritingDirection::kLtr));
      break;
    case chrome_screen_ai::Direction::RIGHT_TO_LEFT:
      out_data.AddIntAttribute(
          ax::mojom::IntAttribute::kTextDirection,
          static_cast<int32_t>(ax::mojom::WritingDirection::kRtl));
      break;
    case chrome_screen_ai::Direction::TOP_TO_BOTTOM:
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
  if (!chrome_screen_ai::ContentType_IsValid(content_type)) {
    NOTREACHED() << "Unrecognized chrome_screen_ai::ContentType value: "
                 << content_type;
    return;
  }
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
    case chrome_screen_ai::CONTENT_TYPE_UNKNOWN:
      // This should be "Role::kPresentational" but it has been erroniously
      // removed from the codebase.
      // TODO(nektar): Add presentational role back to avoid confusion with the
      // meaning of kNone vs. kUnknown.
      out_data.role = ax::mojom::Role::kNone;  // Presentational.
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
                      const size_t index,
                      ui::AXNodeData& parent_node,
                      std::vector<ui::AXNodeData>& node_data) {
  DCHECK_LT(index, node_data.size());
  DCHECK_NE(parent_node.id, ui::kInvalidAXNodeID);
  ui::AXNodeData& word_box_node = node_data[index];
  DCHECK_EQ(word_box_node.role, ax::mojom::Role::kUnknown);
  if (word_box.confidence() < 0.0f || word_box.confidence() > 1.0f) {
    NOTREACHED() << "Unrecognized chrome_screen_ai::WordBox::confidence value: "
                 << word_box.confidence();
    return;  // Confidence is out of bounds.
  }
  if (word_box.confidence() < kScreenAIMinConfidenceThreshold)
    return;
  word_box_node.role = ax::mojom::Role::kInlineTextBox;
  word_box_node.id = GetNextNodeID();
  SerializeBoundingBox(word_box.bounding_box(), parent_node.id, word_box_node);
  // Since the role is `kInlineTextBox`, NameFrom would automatically and
  // correctly be set to `ax::mojom::NameFrom::kContents`.
  if (word_box.has_space_after()) {
    word_box_node.SetName(word_box.utf8_string() + " ");
  } else {
    word_box_node.SetName(word_box.utf8_string());
  }
  // TODO(nektar): DCHECK that line box's text is equal to the concatenation of
  // the text found in all contained word boxes.
  // TODO(nektar): Set character bounding box information.
  if (word_box.estimate_color_success()) {
    word_box_node.AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                  word_box.background_rgb_value());
    word_box_node.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                  word_box.foreground_rgb_value());
  }
  SerializeDirection(
      static_cast<chrome_screen_ai::Direction>(word_box.direction()),
      word_box_node);
  parent_node.child_ids.push_back(word_box_node.id);
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

void SerializeLineBox(const chrome_screen_ai::LineBox& line_box,
                      const size_t index,
                      ui::AXNodeData& parent_node,
                      std::vector<ui::AXNodeData>& node_data) {
  DCHECK_LT(index, node_data.size());
  DCHECK_NE(parent_node.id, ui::kInvalidAXNodeID);
  ui::AXNodeData& line_box_node = node_data[index];
  DCHECK_EQ(line_box_node.role, ax::mojom::Role::kUnknown);
  if (line_box.confidence() < 0.0f || line_box.confidence() > 1.0f) {
    NOTREACHED() << "Unrecognized chrome_screen_ai::LineBox::confidence value: "
                 << line_box.confidence();
    return;  // Confidence is out of bounds.
  }
  if (line_box.confidence() < kScreenAIMinConfidenceThreshold)
    return;
  SerializeContentType(line_box.content_type(), line_box_node);
  line_box_node.id = GetNextNodeID();
  if (ui::IsText(line_box_node.role)) {
    size_t word_node_index = index + 1u;
    for (const auto& word : line_box.words())
      SerializeWordBox(word, word_node_index++, line_box_node, node_data);
  }
  SerializeBoundingBox(line_box.bounding_box(), parent_node.id, line_box_node);
  // Since the role is `kStaticText`, NameFrom would automatically and correctly
  // be set to `ax::mojom::NameFrom::kContents`.
  line_box_node.SetName(line_box.utf8_string());
  if (!line_box.language().empty()) {
    // TODO(nektar): Only set language if different from parent node to
    // minimize memory usage.
    line_box_node.AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                     line_box.language());
  }
  SerializeDirection(
      static_cast<chrome_screen_ai::Direction>(line_box.direction()),
      line_box_node);
  parent_node.child_ids.push_back(line_box_node.id);
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

}  // namespace

namespace screen_ai {

ui::AXTreeUpdate ScreenAIVisualAnnotationToAXTreeUpdate(
    const std::string& serialized_proto,
    const gfx::Rect& image_rect) {
  ui::AXTreeUpdate update;

  chrome_screen_ai::VisualAnnotation visual_annotation;
  if (!visual_annotation.ParseFromString(serialized_proto)) {
    NOTREACHED() << "Could not parse Screen AI library output.";
    return update;
  }

  // TODO(https://crbug.com/1278249): Create an AXTreeSource and create the
  // update using AXTreeSerializer.

  // Each `UIComponent` and `LineBox` will take up one node in the accessibility
  // tree, resulting in hundreds of nodes, making it inefficient to push_back
  // one node at a time. We pre-allocate the needed nodes making node creation
  // an O(n) operation.
  const size_t word_count = std::accumulate(
      std::begin(visual_annotation.lines()),
      std::end(visual_annotation.lines()), 0u,
      [](const size_t& count, const chrome_screen_ai::LineBox& line_box) {
        return count + line_box.words().size();
      });

  // Each unique `chrome_screen_ai::LineBox::block_id` creates a new
  // paragraph, each paragraph is placed in its correct reading order,
  // and each paragraph has a sorted set of line boxes. Line boxes are sorted
  // using their `chrome_screen_ai::LineBox::order_within_block` member and they
  // are identified by their index in the container of line boxes. Use std::map
  // to sort both paragraphs and lines, both operations having an O(n * log(n))
  // complexity.
  // TODO(accessibility): Determine reading order based on visual positioning of
  // paragraphs, not on their block IDs.
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
      blocks_to_lines_map.size() + visual_annotation.lines().size() +
      word_count);
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
        SerializeLineBox(line_box, index++, page_node, nodes);
        index += line_box.words().size();
      }
    }
  }

  // Filter out invalid / unrecognized / unused nodes from the update.
  update.nodes.resize(nodes.size());
  auto end_node_iter =
      std::copy_if(std::begin(nodes), std::end(nodes), std::begin(update.nodes),
                   [](const ui::AXNodeData& node_data) {
                     return node_data.role != ax::mojom::Role::kUnknown &&
                            node_data.id != ui::kInvalidAXNodeID;
                   });
  update.nodes.resize(std::distance(std::begin(update.nodes), end_node_iter));

  // TODO(https://crbug.com/1278249): Add UMA metrics to record the number of
  // annotations, item types, confidence levels, etc.

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
    screenai::UiElement* uie = view_hierarchy.add_ui_elements();
    screenai::UiElementAttribute* attrib = nullptr;

    // ID.
    uie->set_id(new_id[ax_node_id]);

    // Text.
    attrib = uie->add_attributes();
    attrib->set_name("text");
    attrib->set_string_value(
        node.GetStringAttribute(ax::mojom::StringAttribute::kName));

    // Class Name.
    // This is a fixed constant for Chrome requests to Screen2x.
    attrib = uie->add_attributes();
    attrib->set_name("class_name");
    attrib->set_string_value("chrome.unicorn");

    // Role.
    attrib = uie->add_attributes();
    attrib->set_name("chrome_role");
    attrib->set_string_value(ui::ToString(node.role));

    // AXNode ID.
    attrib = uie->add_attributes();
    attrib->set_name("/axnode/node_id");
    attrib->set_int_value(ax_node_id);

    // Child IDs.
    for (const ui::AXNodeID& id : node.child_ids) {
      attrib = uie->add_attributes();
      attrib->set_name("/axnode/child_ids");
      attrib->set_int_value(id);
      uie->add_child_ids(new_id[id]);
    }

    // Type and parent.
    if (node.id == snapshot.root_id) {
      uie->set_type(screenai::UiElementType::ROOT);
      uie->set_parent_id(-1);
    } else {
      uie->set_type(screenai::UiElementType::VIEW);
      uie->set_parent_id(new_id[child_id_to_parent_id[ax_node_id]]);
    }

    // TODO(https://crbug.com/1278249): Bounding box and Bounding Box Pixels
    // do not consider offset container, ransforms, device scaling, clipping,
    // offscreen state, etc. This should be fixed the same way the data is
    // created for training Screen2x models.

    // Bounding Box.
    uie->mutable_bounding_box()->set_top(node.relative_bounds.bounds.y() /
                                         snapshot_height);
    uie->mutable_bounding_box()->set_left(node.relative_bounds.bounds.x() /
                                          snapshot_width);
    uie->mutable_bounding_box()->set_bottom(
        node.relative_bounds.bounds.bottom() / snapshot_height);
    uie->mutable_bounding_box()->set_right(node.relative_bounds.bounds.right() /
                                           snapshot_width);

    // Bounding Box Pixels.
    uie->mutable_bounding_box_pixels()->set_top(
        node.relative_bounds.bounds.y());
    uie->mutable_bounding_box_pixels()->set_left(
        node.relative_bounds.bounds.x());
    uie->mutable_bounding_box_pixels()->set_bottom(
        node.relative_bounds.bounds.bottom());
    uie->mutable_bounding_box_pixels()->set_right(
        node.relative_bounds.bounds.right());

    // TODO(https://crbug.com/1278249): Add non-essential values.
  }

  return view_hierarchy.SerializeAsString();
}

}  // namespace screen_ai
