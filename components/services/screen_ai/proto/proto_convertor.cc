// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/proto_convertor.h"

#include <memory>

#include "base/containers/contains.h"
#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "components/services/screen_ai/proto/dimension.pb.h"
#include "components/services/screen_ai/proto/view_hierarchy.pb.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace {

// The minimum confidence level that a Screen AI annotation should have to be
// accepted.
// TODO(https://crbug.com/1278249): Add experiment or heuristics to better
// adjust this threshold.
const float kScreenAIMinConfidenceThreshold = 0.1;

// Returns the next valid ID that can be used for identifying `AXNode`s in the
// accessibility tree.
ui::AXNodeID GetNextNodeID() {
  static int next_node_id = 1;
  return next_node_id++;
}

void SerializePredictedType(
    const chrome_screen_ai::UIComponent_PredictedType& predicted_type,
    ui::AXNodeData& out_data) {
  switch (predicted_type.type_of_case()) {
    case chrome_screen_ai::UIComponent_PredictedType::kEnumType:
      // TODO(https://crbug.com/1278249): Add tests to ensure these two types
      // match. Add a PRESUBMIT test that compares the proto and enum.
      // TODO(accessibility): Why do we even need an enum. Couldn't all
      // predicted types be strings? We could easily map from a string to an
      // ax::mojom::Role. Then, we won't need to keep the enums synced.
      out_data.role = static_cast<ax::mojom::Role>(predicted_type.enum_type());
      break;
    case chrome_screen_ai::UIComponent_PredictedType::kStringType:
      out_data.role = ax::mojom::Role::kGenericContainer;
      out_data.AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                  predicted_type.string_type());
      break;
    case chrome_screen_ai::UIComponent_PredictedType::TYPE_OF_NOT_SET:
      // TODO(accessibility): Why is this a possibility if the member in the
      // proto is not marked optional?
      NOTREACHED();
      break;
  }
}

void SerializeBoundingBox(const chrome_screen_ai::Rect& bounding_box,
                          const ui::AXNodeID& container_id,
                          ui::AXNodeData& out_data) {
  out_data.relative_bounds.bounds =
      gfx::RectF(bounding_box.x(), bounding_box.y(), bounding_box.width(),
                 bounding_box.height());
  if (container_id != ui::kInvalidAXNodeID)
    out_data.relative_bounds.offset_container_id = container_id;
  if (bounding_box.angle()) {
    out_data.relative_bounds.transform = std::make_unique<gfx::Transform>();
    out_data.relative_bounds.transform->Rotate(bounding_box.angle());
  }
}

absl::optional<ui::AXNodeData> SerializeUIComponent(
    const chrome_screen_ai::UIComponent& ui_component) {
  // The score is only used to prune very low confidence detections and we don't
  // use it in the accessibility tree.
  if (ui_component.predicted_type().confidence() <
      kScreenAIMinConfidenceThreshold) {
    return absl::nullopt;
  }

  ui::AXNodeData node_data;
  node_data.id = GetNextNodeID();
  SerializePredictedType(ui_component.predicted_type(), node_data);
  SerializeBoundingBox(ui_component.bounding_box(),
                       /* container_id */ ui::kInvalidAXNodeID, node_data);
  return node_data;
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
    const std::string& serialized_proto) {
  ui::AXTreeUpdate update;

  chrome_screen_ai::VisualAnnotation visual_annotation;
  if (!visual_annotation.ParseFromString(serialized_proto)) {
    VLOG(1) << "Could not parse Screen AI library output.";
    return update;
  }

  // TODO(https://crbug.com/1278249): Create an AXTreeSource and create the
  // update using AXTreeSerializer.

  for (const auto& ui_component : visual_annotation.ui_component()) {
    absl::optional<ui::AXNodeData> node_data =
        SerializeUIComponent(ui_component);
    if (node_data)
      update.nodes.push_back(*node_data);
  }

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
    int ax_node_id = static_cast<int>(node.id);

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
