// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_RESIZE_AREA_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_RESIZE_AREA_H_

#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/resize_area.h"

namespace views {

// Keyboard-accessible drag handle icon intended to be drawn on top of a
// SidePanelResizeArea.
class SidePanelResizeHandle : public ImageView,
                              public views::FocusChangeListener {
  METADATA_HEADER(SidePanelResizeHandle, ImageView)

 public:
  explicit SidePanelResizeHandle(SidePanel* side_panel);

  void UpdateVisibility(bool visible);

  // ImageView:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

 private:
  raw_ptr<SidePanel> side_panel_;
};

// ResizeArea with custom Layout override to draw on top of the Side Panel
// border, and responsiveness to key events via a focusable
// SidePanelResizeHandle.
class SidePanelResizeArea : public ResizeArea {
  METADATA_HEADER(SidePanelResizeArea, ResizeArea)

 public:
  explicit SidePanelResizeArea(SidePanel* side_panel);

  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void Layout(PassKey) override;

 private:
  raw_ptr<SidePanel> side_panel_;
  raw_ptr<SidePanelResizeHandle> resize_handle_;
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_RESIZE_AREA_H_
