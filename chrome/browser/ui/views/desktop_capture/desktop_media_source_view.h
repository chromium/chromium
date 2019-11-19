// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_

#include "content/public/browser/desktop_media_id.h"
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
                              const gfx::Rect& image_rect,
                              int focus_rectangle_inset);

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

  // When a source item is focused, we paint dotted line. This parameter
  // controls the distance between dotted line and the source view boundary.
  int focus_rectangle_inset;
};

// View used for each item in DesktopMediaListView. Shows a single desktop media
// source as a thumbnail with the title under it.
class DesktopMediaSourceView : public views::View {
 public:
  DesktopMediaSourceView(DesktopMediaListView* parent,
                         content::DesktopMediaID source_id,
                         DesktopMediaSourceViewStyle style);
  ~DesktopMediaSourceView() override;

  // Used to update the style when the number of available items changes.
  void SetStyle(DesktopMediaSourceViewStyle style);

  // Updates thumbnail and title from |source|.
  void SetName(const base::string16& name);
  void SetThumbnail(const gfx::ImageSkia& thumbnail);
  void SetIcon(const gfx::ImageSkia& icon);

  // Id for the source shown by this View.
  const content::DesktopMediaID& source_id() const { return source_id_; }

  // Returns true if the source is selected.
  bool is_selected() const { return selected_; }

  // views::View interface.
  const char* GetClassName() const override;
  views::View* GetSelectedViewForGroup(int group) override;
  bool IsGroupFocusTraversable() const override;
  void OnFocus() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  static const char kDesktopMediaSourceViewClassName[];

 private:
  // Updates selection state of the element. If |selected| is true then also
  // calls SetSelected(false) for the source view that was selected before that
  // (if any).
  void SetSelected(bool selected);

  // Updates hover state of the element, and the appearance.
  void SetHovered(bool hovered);

  DesktopMediaListView* parent_;
  content::DesktopMediaID source_id_;

  DesktopMediaSourceViewStyle style_;
  views::ImageView* icon_view_ = new views::ImageView;
  views::ImageView* image_view_ = new views::ImageView;
  views::Label* label_ = new views::Label;

  std::unique_ptr<views::FocusRing> focus_ring_ =
      views::FocusRing::Install(this);

  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaSourceView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_SOURCE_VIEW_H_
