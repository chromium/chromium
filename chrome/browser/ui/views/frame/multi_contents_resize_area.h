// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_RESIZE_AREA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_RESIZE_AREA_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/resize_area.h"

class MultiContentsView;

// Keyboard-accessible drag handle icon intended to be drawn on top of a
// MultiContentsResizeArea.
class MultiContentsResizeHandle : public views::ImageView {
  METADATA_HEADER(MultiContentsResizeHandle, ImageView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsResizeHandleElementId);

  MultiContentsResizeHandle();
};

// ResizeArea meant to draw in between WebContents within a MultiContentsView,
// and responsiveness to key events via a focusable MultiContentsResizeHandle.
class MultiContentsResizeArea : public views::ResizeArea {
  METADATA_HEADER(MultiContentsResizeArea, ResizeArea)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsResizeAreaElementId);

  explicit MultiContentsResizeArea(MultiContentsView* multi_contents_view);

  bool OnKeyPressed(const ui::KeyEvent& event) override;

 private:
  raw_ptr<MultiContentsView> multi_contents_view_;
  raw_ptr<MultiContentsResizeHandle> resize_handle_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_RESIZE_AREA_H_
