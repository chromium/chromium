// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERY_CHECKBOX_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERY_CHECKBOX_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

struct MediaGalleryPrefInfo;

namespace views {
class Checkbox;
class ContextMenuController;
class Label;
}  // namespace views

// A view composed of a checkbox, optional folder icon button, and secondary
// text that will elide to its parent's width. Used by
// MediaGalleriesDialogViews.
class MediaGalleryCheckboxView : public views::BoxLayoutView {
  METADATA_HEADER(MediaGalleryCheckboxView, views::BoxLayoutView)

 public:
  MediaGalleryCheckboxView(const MediaGalleryPrefInfo& pref_info,
                           int trailing_vertical_space,
                           views::ContextMenuController* menu_controller);
  MediaGalleryCheckboxView(const MediaGalleryCheckboxView&) = delete;
  MediaGalleryCheckboxView& operator=(const MediaGalleryCheckboxView&) = delete;
  ~MediaGalleryCheckboxView() override;

  views::Checkbox* checkbox() { return checkbox_; }
  views::Label* secondary_text() { return secondary_text_; }

  // views::View:
  void Layout(PassKey) override;

 private:
  // Owned by the parent class (views::View).
  raw_ptr<views::Checkbox> checkbox_;
  raw_ptr<views::Label> secondary_text_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERY_CHECKBOX_VIEW_H_
