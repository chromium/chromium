// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_AX_ACTION_TARGET_H_
#define COMPONENTS_PDF_RENDERER_PDF_AX_ACTION_TARGET_H_

#include "ui/accessibility/ax_action_target.h"

namespace ui {
class AXNode;
}

namespace pdf {

class PdfAccessibilityTree;

// Abstracts an AXNode, representing a PDF node, for dispatching
// accessibility actions.
class PdfAXActionTarget : public ui::AXActionTarget {
 public:
  PdfAXActionTarget(const ui::AXNode& plugin_node, PdfAccessibilityTree* tree);
  ~PdfAXActionTarget() override;

 protected:
  // AXActionTarget overrides.
  Type GetType() const override;
  bool ClearAccessibilityFocus() const override;
  bool Click() const override;
  bool Decrement() const override;
  bool Increment() const override;
  bool Focus() const override;
  gfx::Rect GetRelativeBounds() const override;
  gfx::Point GetScrollOffset() const override;
  gfx::Point MinimumScrollOffset() const override;
  gfx::Point MaximumScrollOffset() const override;
  bool SetAccessibilityFocus() const override;
  void SetScrollOffset(const gfx::Point& point) const override;
  bool SetSelected(bool selected) const override;
  bool SetSelection(const ui::AXActionTarget* anchor_object,
                    int anchor_offset,
                    const ui::AXActionTarget* focus_object,
                    int focus_offset) const override;
  bool SetSequentialFocusNavigationStartingPoint() const override;
  bool SetValue(const std::string& value) const override;
  bool ShowContextMenu() const override;
  bool ScrollToMakeVisible() const override;
  bool ScrollToMakeVisibleWithSubFocus(
      const gfx::Rect& rect,
      ax::mojom::ScrollAlignment horizontal_scroll_alignment,
      ax::mojom::ScrollAlignment vertical_scroll_alignment) const override;
  bool ScrollToGlobalPoint(const gfx::Point& point) const override;

 private:
  const ui::AXNode& target_plugin_node_;
  PdfAccessibilityTree* pdf_accessibility_tree_source_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_AX_ACTION_TARGET_H_
