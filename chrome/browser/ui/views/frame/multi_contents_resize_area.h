// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_RESIZE_AREA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_RESIZE_AREA_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

class MultiContentsView;

// Keyboard-accessible drag handle icon intended to be drawn on top of a
// MultiContentsResizeArea.
class MultiContentsResizeHandle : public views::View,
                                  public views::FocusChangeListener {
  METADATA_HEADER(MultiContentsResizeHandle, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsResizeHandleElementId);

  MultiContentsResizeHandle();

  void UpdateVisibility(bool visible);

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
};

// ResizeArea meant to draw in between WebContents within a MultiContentsView,
// and responsiveness to key events via a focusable MultiContentsResizeHandle.
class MultiContentsResizeArea : public views::ResizeArea {
  METADATA_HEADER(MultiContentsResizeArea, ResizeArea)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsResizeAreaElementId);

  explicit MultiContentsResizeArea(MultiContentsView* multi_contents_view);

  // views::ResizeArea:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  raw_ptr<MultiContentsView> multi_contents_view_;
  raw_ptr<MultiContentsResizeHandle> resize_handle_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_RESIZE_AREA_H_
