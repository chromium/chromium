// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/window_icon_util.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"

using content::DesktopMediaID;

namespace {

const int kDesktopMediaSourceViewGroupId = 1;

#if defined(OS_CHROMEOS)
// Here we are going to display default app icon for app windows without an
// icon, and display product logo for chrome browser windows.
gfx::ImageSkia LoadDefaultIcon(aura::Window* window) {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  Browser* browser = browser_view ? browser_view->browser() : nullptr;

  // Apps could be launched in a view other than BrowserView, so we count those
  // windows without Browser association as apps.
  // Technically dev tool is actually a special app, but we would like to
  // display product logo for it, because intuitively it is internal to browser.
  bool is_app = !browser || (browser->is_app() && !browser->is_devtools());
  int idr = is_app ? IDR_APP_DEFAULT_ICON : IDR_PRODUCT_LOGO_32;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return *rb.GetImageSkiaNamed(idr);
}
#endif

}  // namespace

DesktopMediaListView::DesktopMediaListView(
    DesktopMediaPickerDialogView* parent,
    std::unique_ptr<DesktopMediaList> media_list,
    DesktopMediaSourceViewStyle generic_style,
    DesktopMediaSourceViewStyle single_style,
    const base::string16& accessible_name)
    : parent_(parent),
      media_list_(std::move(media_list)),
      single_style_(single_style),
      generic_style_(generic_style),
      active_style_(&single_style_),
      accessible_name_(accessible_name),
      weak_factory_(this) {
  SetStyle(&single_style_);

  SetFocusBehavior(FocusBehavior::ALWAYS);
}

DesktopMediaListView::~DesktopMediaListView() {}

void DesktopMediaListView::StartUpdating(DesktopMediaID dialog_window_id) {
  media_list_->SetViewDialogWindowId(dialog_window_id);
  media_list_->StartUpdating(this);
}

void DesktopMediaListView::OnSelectionChanged() {
  parent_->OnSelectionChanged();
}

void DesktopMediaListView::OnDoubleClick() {
  parent_->OnDoubleClick();
}

DesktopMediaSourceView* DesktopMediaListView::GetSelection() {
  for (int i = 0; i < child_count(); ++i) {
    DesktopMediaSourceView* source_view = GetChild(i);
    DCHECK_EQ(source_view->GetClassName(),
              DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
    if (source_view->is_selected())
      return source_view;
  }
  return nullptr;
}

gfx::Size DesktopMediaListView::CalculatePreferredSize() const {
  int total_rows =
      (child_count() + active_style_->columns - 1) / active_style_->columns;
  return gfx::Size(active_style_->columns * active_style_->item_size.width(),
                   total_rows * active_style_->item_size.height());
}

void DesktopMediaListView::Layout() {
  int x = 0;
  int y = 0;

  for (int i = 0; i < child_count(); ++i) {
    if (i > 0 && i % active_style_->columns == 0) {
      x = 0;
      y += active_style_->item_size.height();
    }

    child_at(i)->SetBounds(x, y, active_style_->item_size.width(),
                           active_style_->item_size.height());

    x += active_style_->item_size.width();
  }
}

bool DesktopMediaListView::OnKeyPressed(const ui::KeyEvent& event) {
  int position_increment = 0;
  switch (event.key_code()) {
    case ui::VKEY_UP:
      position_increment = -active_style_->columns;
      break;
    case ui::VKEY_DOWN:
      position_increment = active_style_->columns;
      break;
    case ui::VKEY_LEFT:
      position_increment = -1;
      break;
    case ui::VKEY_RIGHT:
      position_increment = 1;
      break;
    default:
      return false;
  }

  if (position_increment == 0)
    return false;

  DesktopMediaSourceView* selected = GetSelection();
  DesktopMediaSourceView* new_selected = nullptr;

  if (selected) {
    int index = GetIndexOf(selected);
    int new_index = index + position_increment;
    new_index = std::min(new_index, child_count() - 1);
    new_index = std::max(new_index, 0);
    if (index != new_index) {
      new_selected = GetChild(new_index);
    }
  } else if (has_children()) {
    new_selected = GetChild(0);
  }

  if (new_selected)
    new_selected->RequestFocus();
  return true;
}

void DesktopMediaListView::OnSourceAdded(DesktopMediaList* list, int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);

  // We are going to have a second item, apply the generic style.
  if (child_count() == 1)
    SetStyle(&generic_style_);

  DesktopMediaSourceView* source_view =
      new DesktopMediaSourceView(this, source.id, *active_style_);

  source_view->SetName(source.name);
  source_view->SetGroup(kDesktopMediaSourceViewGroupId);
  if (source.id.type == DesktopMediaID::TYPE_WINDOW) {
    gfx::ImageSkia icon_image = GetWindowIcon(source.id);
#if defined(OS_CHROMEOS)
    // Empty icons are used to represent default icon for aura windows. By
    // detecting this, we load the default icon from resource.
    if (icon_image.isNull()) {
      aura::Window* window = DesktopMediaID::GetAuraWindowById(source.id);
      if (window)
        icon_image = LoadDefaultIcon(window);
    }
#endif
    source_view->SetIcon(icon_image);
  }
  AddChildViewAt(source_view, index);

  if ((child_count() - 1) % active_style_->columns == 0)
    parent_->OnMediaListRowsChanged();

  // Auto select the first screen.
  if (index == 0 && source.id.type == DesktopMediaID::TYPE_SCREEN)
    source_view->RequestFocus();

  PreferredSizeChanged();

  std::string autoselect_source =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutoSelectDesktopCaptureSource);
  if (!autoselect_source.empty() &&
      base::ASCIIToUTF16(autoselect_source) == source.name) {
    // Select, then accept and close the dialog when we're done adding sources.
    parent_->SelectTab(source.id.type);
    source_view->OnFocus();
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&DesktopMediaListView::AcceptSelection,
                       weak_factory_.GetWeakPtr()));
  }
}

void DesktopMediaListView::OnSourceRemoved(DesktopMediaList* list, int index) {
  DesktopMediaSourceView* view = GetChild(index);
  DCHECK(view);
  DCHECK_EQ(view->GetClassName(),
            DesktopMediaSourceView::kDesktopMediaSourceViewClassName);

  bool was_selected = view->is_selected();
  RemoveChildView(view);
  delete view;

  if (was_selected)
    OnSelectionChanged();

  if (child_count() % active_style_->columns == 0)
    parent_->OnMediaListRowsChanged();

  // Apply single-item styling when the second source is removed.
  if (child_count() == 1)
    SetStyle(&single_style_);

  PreferredSizeChanged();
}

void DesktopMediaListView::OnSourceMoved(DesktopMediaList* list,
                                         int old_index,
                                         int new_index) {
  ReorderChildView(GetChild(old_index), new_index);
  PreferredSizeChanged();
}

void DesktopMediaListView::OnSourceNameChanged(DesktopMediaList* list,
                                               int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  DesktopMediaSourceView* source_view = GetChild(index);
  source_view->SetName(source.name);
}

void DesktopMediaListView::OnSourceThumbnailChanged(DesktopMediaList* list,
                                                    int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  DesktopMediaSourceView* source_view = GetChild(index);
  source_view->SetThumbnail(source.thumbnail);
}

void DesktopMediaListView::AcceptSelection() {
  OnSelectionChanged();
  OnDoubleClick();
}

void DesktopMediaListView::SetStyle(DesktopMediaSourceViewStyle* style) {
  active_style_ = style;
  media_list_->SetThumbnailSize(gfx::Size(
      style->image_rect.width() - 2 * style->selection_border_thickness,
      style->image_rect.height() - 2 * style->selection_border_thickness));

  for (int i = 0; i < child_count(); i++) {
    GetChild(i)->SetStyle(*active_style_);
  }
}

DesktopMediaSourceView* DesktopMediaListView::GetChild(int index) {
  return static_cast<DesktopMediaSourceView*>(child_at(index));
}

void DesktopMediaListView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGroup;
  node_data->SetName(accessible_name_);
}
