// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_AX_ACTION_TARGET_H_
#define COMPONENTS_PDF_RENDERER_PDF_AX_ACTION_TARGET_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/accessibility/ax_action_target.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {
class AXNode;
}

namespace pdf {

class PdfAccessibilityTree;

// Abstracts an AXNode, representing a PDF node, for dispatching
// accessibility actions.
class PdfAXActionTarget : public ui::AXActionTarget {
 public:
  static const PdfAXActionTarget* FromAXActionTarget(
      const ui::AXActionTarget* ax_action_target);

  PdfAXActionTarget(const ui::AXNode& plugin_node, PdfAccessibilityTree* tree);
  ~PdfAXActionTarget() override;

  const ui::AXNode& AXNode() const { return *target_plugin_node_; }

 protected:
  // AXActionTarget overrides.
  Type GetType() const override;
  bool PerformAction(const ui::AXActionData& action_data) const override;
  gfx::Rect GetRelativeBounds() const override;
  gfx::Point GetScrollOffset() const override;
  gfx::Point MinimumScrollOffset() const override;
  gfx::Point MaximumScrollOffset() const override;
  void SetScrollOffset(const gfx::Point& point) const override;
  bool SetSelection(const ui::AXActionTarget* anchor_object,
                    int anchor_offset,
                    const ui::AXActionTarget* focus_object,
                    int focus_offset) const override;
  bool ScrollToMakeVisible() const override;
  bool ScrollToMakeVisibleWithSubFocus(
      const gfx::Rect& rect,
      ax::mojom::ScrollAlignment horizontal_scroll_alignment,
      ax::mojom::ScrollAlignment vertical_scroll_alignment,
      ax::mojom::ScrollBehavior scroll_behavior) const override;

 private:
  bool Click() const;
  bool ShowContextMenu() const;
  bool ScrollToGlobalPoint(const gfx::Point& point) const;
  bool StitchChildTree(const ui::AXTreeID& child_tree_id) const;

  const raw_ref<const ui::AXNode> target_plugin_node_;
  raw_ptr<PdfAccessibilityTree> pdf_accessibility_tree_source_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_AX_ACTION_TARGET_H_
