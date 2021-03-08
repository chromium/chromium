// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_SEMANTICS_NODE_WRAPPER_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_SEMANTICS_NODE_WRAPPER_H_

#include <string>
#include <vector>

#include "chromecast/browser/accessibility/flutter/flutter_semantics_node.h"
#include "chromecast/browser/accessibility/proto/cast_server_accessibility.pb.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_tree_source.h"

namespace chromecast {
namespace accessibility {

using gallium::castos::SemanticsNode;

class AXTreeSourceFlutter;

// A wrapper class for a SemanticsNode proto object.
// This is used by AXTreeSourceFlutter to create accessibility tree updates
// from semantics trees sent to us from the flutter process.
class FlutterSemanticsNodeWrapper : public FlutterSemanticsNode {
 public:
  FlutterSemanticsNodeWrapper(
      ui::AXTreeSource<FlutterSemanticsNode*>* tree_source,
      const SemanticsNode* node);
  FlutterSemanticsNodeWrapper(const FlutterSemanticsNodeWrapper&) = delete;
  FlutterSemanticsNodeWrapper& operator=(const FlutterSemanticsNodeWrapper&) =
      delete;

  // FlutterSemanticsNode implementation:
  int32_t GetId() const override;
  const gfx::Rect GetBounds() const override;
  bool IsVisibleToUser() const override;
  bool IsFocused() const override;
  bool IsLiveRegion() const override;
  bool HasScopesRoute() const override;
  bool HasNamesRoute() const override;
  bool IsRapidChangingSlider() const override;
  bool CanBeAccessibilityFocused() const override;
  void PopulateAXRole(ui::AXNodeData* out_data) const override;
  void PopulateAXState(ui::AXNodeData* out_data) const override;
  void Serialize(ui::AXNodeData* out_data) const override;
  void GetChildren(std::vector<FlutterSemanticsNode*>* children) const override;
  bool HasLabelHint() const override;
  std::string GetLabelHint() const override;
  bool HasValue() const override;
  std::string GetValue() const override;
  bool IsKeyboardNode() const override;

  const SemanticsNode* node() { return node_ptr_; }

 private:
  bool AnyChildIsActionable() const;
  bool HasTapOrPress() const;
  bool IsActionable() const;
  bool IsScrollable() const;
  bool IsFocusable() const;
  void ComputeNameFromContents(std::vector<std::string>* names) const override;
  void GetActionableChildren(
      std::vector<FlutterSemanticsNodeWrapper*>* out_children) const;
  // Check if this is a list item and return the node of its ancestor whose role
  // is kList
  FlutterSemanticsNodeWrapper* IsListItem() const;
  bool IsDescendant(FlutterSemanticsNodeWrapper* ancestor) const;

  // Returns bounds of a node which can be passed to AXNodeData.location. Bounds
  // are returned in the following coordinates depending on whether it's root or
  // not.
  // - Root node is relative to its container.
  // - Non-root node is relative to the root node of this tree.
  const gfx::Rect GetRelativeBounds() const;

  ui::AXTreeSource<FlutterSemanticsNode*>* const tree_source_;
  const SemanticsNode* const node_ptr_;
};

}  // namespace accessibility
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_FLUTTER_SEMANTICS_NODE_WRAPPER_H_
