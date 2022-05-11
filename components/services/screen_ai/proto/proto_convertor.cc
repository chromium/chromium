// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/proto_convertor.h"

#include "base/containers/contains.h"
#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "components/services/screen_ai/proto/dimension.pb.h"
#include "components/services/screen_ai/proto/view_hierarchy.pb.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/ax_enum_util.h"

namespace {

// The minimum confidence level that a Screen AI annotation should have to be
// accepted.
// TODO(https://crbug.com/1278249): Add experiment or heuristics to better
// adjust this threshold.
const float kScreenAIMinConfidenceThreshold = 0.1;

}  // namespace

namespace screen_ai {

ui::AXTreeUpdate ScreenAIVisualAnnotationToAXTreeUpdate(
    const std::string& serialized_proto) {
  ui::AXTreeUpdate updates;

  chrome_screen_ai::VisualAnnotation results;
  if (!results.ParseFromString(serialized_proto)) {
    VLOG(1) << "Could not parse Screen AI library output.";
    return updates;
  }

  // TODO(https://crbug.com/1278249): Create an AXTreeSource and create the
  // update using AXTreeSerializer.

  for (const auto& uic : results.ui_component()) {
    // Score is only used to prune very low confidence detections and we don't
    // use it downstream.
    if (uic.predicted_type().score() < kScreenAIMinConfidenceThreshold)
      continue;

    ui::AXNodeData node;

    node.relative_bounds.bounds.set_x(uic.bounding_box().x());
    node.relative_bounds.bounds.set_y(uic.bounding_box().y());
    node.relative_bounds.bounds.set_width(uic.bounding_box().width());
    node.relative_bounds.bounds.set_height(uic.bounding_box().height());

    switch (uic.predicted_type().type_of_case()) {
      case chrome_screen_ai::UIComponent_PredictedType::kEnumType:
        // TODO(https://crbug.com/1278249): Add tests to ensure these two types
        // match. Add a PRESUBMIT test that compares the proto and enum.
        node.role =
            static_cast<ax::mojom::Role>(uic.predicted_type().enum_type());
        break;
      case chrome_screen_ai::UIComponent_PredictedType::kStringType:
        node.role = ax::mojom::Role::kGenericContainer;
        node.AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                uic.predicted_type().string_type());
        break;
      case chrome_screen_ai::UIComponent_PredictedType::TYPE_OF_NOT_SET:
        NOTREACHED();
        continue;
    }
    updates.nodes.push_back(node);
  }

  // TODO(https://crbug.com/1278249): Add UMA metrics to record the number of
  // annotations, item types, confidence levels, etc.

  return updates;
}

std::string Screen2xSnapshotToViewHierarchy(const ui::AXTreeUpdate& snapshot) {
  screenai::ViewHierarchy view_hierarchy;

  // TODO(https://crbug.com/1278249): Use actual screen resolution.
  // As root's |relative_bounding_box| is relative to the outside container,
  // we can assume it as resolution now.
  float screen_width = -1;
  float screen_height = -1;
  for (auto& node : snapshot.nodes) {
    if (node.id != snapshot.root_id)
      continue;
    screen_width = node.relative_bounds.bounds.width();
    screen_height = node.relative_bounds.bounds.height();
    break;
  }
  VLOG(2) << "Assumed screen size for Screen2x: " << screen_width << " x "
          << screen_height;

  // TODO(https://crbug.com/1278249): Screen2x requires id 0 for the root, and
  // -1 as no-parent indicator. Consider just using the type and ignoring the id
  // value for detecting root.
  std::map<int, int> chrome_to_screen2x_ids;
  int last_used_id = 0;
  // A map for finding parents faster.
  std::map<int, int> child_to_parent_map;
  for (auto& node : snapshot.nodes) {
    int ax_node_id = static_cast<int>(node.id);
    if (node.id == snapshot.root_id)
      chrome_to_screen2x_ids[ax_node_id] = 0;
    else
      chrome_to_screen2x_ids[ax_node_id] = ++last_used_id;

    for (int32_t child_id : node.child_ids)
      child_to_parent_map[child_id] = chrome_to_screen2x_ids[ax_node_id];
  }

  for (auto& node : snapshot.nodes) {
    screenai::UiElement* uie = view_hierarchy.add_ui_elements();
    screenai::UiElementAttribute* attrib = nullptr;
    int ax_node_id = static_cast<int>(node.id);

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

    // AxNode ID.
    attrib = uie->add_attributes();
    attrib->set_name("/axnode/node_id");
    attrib->set_int_value(ax_node_id);

    // Child IDs.
    for (int32_t id : node.child_ids) {
      attrib = uie->add_attributes();
      attrib->set_name("/axnode/child_ids");
      attrib->set_int_value(id);
      uie->add_child_ids(chrome_to_screen2x_ids[id]);
    }

    // ID, Type, and parent.
    uie->set_id(chrome_to_screen2x_ids[ax_node_id]);
    if (node.id == snapshot.root_id) {
      uie->set_parent_id(-1);
      uie->set_type(screenai::UiElementType::ROOT);
    } else {
      uie->set_parent_id(child_to_parent_map[ax_node_id]);
      uie->set_type(screenai::UiElementType::VIEW);
    }

    // Bounding Box.
    uie->mutable_bounding_box()->set_top(node.relative_bounds.bounds.y() /
                                         screen_height);
    uie->mutable_bounding_box()->set_left(node.relative_bounds.bounds.x() /
                                          screen_width);
    uie->mutable_bounding_box()->set_bottom(
        node.relative_bounds.bounds.bottom() / screen_height);
    uie->mutable_bounding_box()->set_right(node.relative_bounds.bounds.right() /
                                           screen_width);

    // Bounding Box Pixels.
    // TODO(https://crbug.com/1278249): These values are relative to the
    // container position and not to the actual top-left of the screen. Consider
    // getting the container position and update based on that.
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
