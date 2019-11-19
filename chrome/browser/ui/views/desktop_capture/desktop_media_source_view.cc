// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"

#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

using content::DesktopMediaID;

DesktopMediaSourceViewStyle::DesktopMediaSourceViewStyle(
    const DesktopMediaSourceViewStyle& style) = default;

DesktopMediaSourceViewStyle::DesktopMediaSourceViewStyle(
    int columns,
    const gfx::Size& item_size,
    const gfx::Rect& icon_rect,
    const gfx::Rect& label_rect,
    gfx::HorizontalAlignment text_alignment,
    const gfx::Rect& image_rect,
    int focus_rectangle_inset)
    : columns(columns),
      item_size(item_size),
      icon_rect(icon_rect),
      label_rect(label_rect),
      text_alignment(text_alignment),
      image_rect(image_rect),
      focus_rectangle_inset(focus_rectangle_inset) {}

DesktopMediaSourceView::DesktopMediaSourceView(
    DesktopMediaListView* parent,
    DesktopMediaID source_id,
    DesktopMediaSourceViewStyle style)
    : parent_(parent),
      source_id_(source_id),
      style_(style),
      selected_(false) {
  AddChildView(icon_view_);
  AddChildView(image_view_);
  AddChildView(label_);
  icon_view_->set_can_process_events_within_subtree(false);
  image_view_->set_can_process_events_within_subtree(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetStyle(style_);
}

DesktopMediaSourceView::~DesktopMediaSourceView() {}

const char DesktopMediaSourceView::kDesktopMediaSourceViewClassName[] =
    "DesktopMediaPicker_DesktopMediaSourceView";

void DesktopMediaSourceView::SetName(const base::string16& name) {
  label_->SetText(name);
}

void DesktopMediaSourceView::SetThumbnail(const gfx::ImageSkia& thumbnail) {
  image_view_->SetImage(thumbnail);
}

void DesktopMediaSourceView::SetIcon(const gfx::ImageSkia& icon) {
  icon_view_->SetImage(icon);
}

void DesktopMediaSourceView::SetSelected(bool selected) {
  if (selected == selected_)
    return;
  selected_ = selected;

  if (selected) {
    // Unselect all other sources.
    Views neighbours;
    parent()->GetViewsInGroup(GetGroup(), &neighbours);
    for (auto i(neighbours.begin()); i != neighbours.end(); ++i) {
      if (*i != this) {
        DCHECK_EQ((*i)->GetClassName(),
                  DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
        DesktopMediaSourceView* source_view =
            static_cast<DesktopMediaSourceView*>(*i);
        source_view->SetSelected(false);
      }
    }

    image_view_->SetBackground(
        views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor)));
    label_->SetFontList(label_->font_list().Derive(0, gfx::Font::NORMAL,
                                                   gfx::Font::Weight::BOLD));
    parent_->OnSelectionChanged();
  } else {
    image_view_->SetBackground(nullptr);
    label_->SetFontList(label_->font_list().Derive(0, gfx::Font::NORMAL,
                                                   gfx::Font::Weight::NORMAL));
  }

  SchedulePaint();
}

const char* DesktopMediaSourceView::GetClassName() const {
  return DesktopMediaSourceView::kDesktopMediaSourceViewClassName;
}

void DesktopMediaSourceView::SetStyle(DesktopMediaSourceViewStyle style) {
  style_ = style;
  image_view_->SetBoundsRect(style_.image_rect);
  icon_view_->SetBoundsRect(style_.icon_rect);
  icon_view_->SetImageSize(style_.icon_rect.size());
  label_->SetBoundsRect(style_.label_rect);
  label_->SetHorizontalAlignment(style_.text_alignment);
}

views::View* DesktopMediaSourceView::GetSelectedViewForGroup(int group) {
  Views neighbours;
  parent()->GetViewsInGroup(group, &neighbours);
  if (neighbours.empty())
    return nullptr;

  for (auto i(neighbours.begin()); i != neighbours.end(); ++i) {
    DCHECK_EQ((*i)->GetClassName(),
              DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
    DesktopMediaSourceView* source_view =
        static_cast<DesktopMediaSourceView*>(*i);
    if (source_view->selected_)
      return source_view;
  }
  return nullptr;
}

bool DesktopMediaSourceView::IsGroupFocusTraversable() const {
  return false;
}

void DesktopMediaSourceView::OnFocus() {
  View::OnFocus();
  SetSelected(true);
  ScrollRectToVisible(gfx::Rect(size()));
}

bool DesktopMediaSourceView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.GetClickCount() == 1) {
    RequestFocus();
  } else if (event.GetClickCount() == 2) {
    RequestFocus();
    parent_->OnDoubleClick();
  }
  return true;
}

void DesktopMediaSourceView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP &&
      event->details().tap_count() == 2) {
    RequestFocus();
    parent_->OnDoubleClick();
    event->SetHandled();
    return;
  }

  // Detect tap gesture using ET_GESTURE_TAP_DOWN so the view also gets focused
  // on the long tap (when the tap gesture starts).
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    RequestFocus();
    event->SetHandled();
  }
}

void DesktopMediaSourceView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(label_->GetText());
}
