// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/desktop_media_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

class DesktopMediaListView;

// Controls the appearance of DesktopMediaSourceView.
struct DesktopMediaSourceViewStyle {
  DesktopMediaSourceViewStyle(const DesktopMediaSourceViewStyle& style);
  DesktopMediaSourceViewStyle(int columns,
                              const gfx::Size& item_size,
                              const gfx::Rect& icon_rect,
                              const gfx::Rect& label_rect,
                              gfx::HorizontalAlignment text_alignment,
                              const gfx::Rect& image_rect);

  // This parameter controls how many source items can be displayed in a row.
  // Source items are instances of DesktopMediaSourceView.
  int columns;

  // The size of a single source item.
  gfx::Size item_size;

  // The relative position to display icon, label and preview image in the
  // source item.
  gfx::Rect icon_rect;
  gfx::Rect label_rect;
  gfx::HorizontalAlignment text_alignment;
  gfx::Rect image_rect;
};

// View used for each item in DesktopMediaListView. Shows a single desktop media
// source as a thumbnail with the title under it.
class DesktopMediaSourceView : public views::View {
  METADATA_HEADER(DesktopMediaSourceView, views::View)

 public:
  DesktopMediaSourceView(DesktopMediaListView* parent,
                         content::DesktopMediaID source_id,
                         DesktopMediaSourceViewStyle style);
  DesktopMediaSourceView(const DesktopMediaSourceView&) = delete;
  DesktopMediaSourceView& operator=(const DesktopMediaSourceView&) = delete;
  ~DesktopMediaSourceView() override;

  // Used to update the style when the number of available items changes.
  void SetStyle(DesktopMediaSourceViewStyle style);

  // Updates thumbnail and title from |source|.
  void SetName(const std::u16string& name);
  void SetThumbnail(const gfx::ImageSkia& thumbnail);
  void SetIcon(const gfx::ImageSkia& icon);

  // Id for the source shown by this View.
  const content::DesktopMediaID& source_id() const { return source_id_; }

  // Returns true if the source is selected.
  bool GetSelected() const;

  // Clears selection from this item, or no-ops if it is not selected.
  void ClearSelection();

  // views::View interface.
  views::View* GetSelectedViewForGroup(int group) override;
  bool IsGroupFocusTraversable() const override;
  void OnFocus() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Updates selection state of the element. If |selected| is true then also
  // calls SetSelected(false) for the source view that was selected before that
  // (if any).
  void SetSelected(bool selected);

  void OnLabelTextChanged();

  void UpdateAccessibleName();

  base::CallbackListSubscription label_text_changed_callback_;
  raw_ptr<DesktopMediaListView> parent_;
  content::DesktopMediaID source_id_;

  raw_ptr<views::ImageView> icon_view_;
  raw_ptr<views::ImageView> image_view_;
  raw_ptr<views::Label> label_;

  bool selected_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_
