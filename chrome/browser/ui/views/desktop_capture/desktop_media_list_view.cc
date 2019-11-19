// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/numerics/ranges.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/window_icon_util.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "ui/aura/window.h"
#endif

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
  bool is_app = !browser || browser->is_type_app();
  int idr = is_app ? IDR_APP_DEFAULT_ICON : IDR_PRODUCT_LOGO_32;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return *rb.GetImageSkiaNamed(idr);
}
#endif

DesktopMediaSourceView* AsDesktopMediaSourceView(views::View* view) {
  DCHECK_EQ(DesktopMediaSourceView::kDesktopMediaSourceViewClassName,
            view->GetClassName());
  return static_cast<DesktopMediaSourceView*>(view);
}

}  // namespace

DesktopMediaListView::DesktopMediaListView(
    DesktopMediaListController* controller,
    DesktopMediaSourceViewStyle generic_style,
    DesktopMediaSourceViewStyle single_style,
    const base::string16& accessible_name)
    : controller_(controller),
      single_style_(single_style),
      generic_style_(generic_style),
      active_style_(&single_style_),
      accessible_name_(accessible_name) {
  SetStyle(&single_style_);
}

DesktopMediaListView::~DesktopMediaListView() {}

void DesktopMediaListView::OnSelectionChanged() {
  controller_->OnSourceSelectionChanged();
}

void DesktopMediaListView::OnDoubleClick() {
  controller_->AcceptSource();
}

gfx::Size DesktopMediaListView::CalculatePreferredSize() const {
  int total_rows = (int{children().size()} + active_style_->columns - 1) /
                   active_style_->columns;
  return gfx::Size(active_style_->columns * active_style_->item_size.width(),
                   total_rows * active_style_->item_size.height());
}

void DesktopMediaListView::Layout() {
  // Children lay out in a grid, all with the same size and without padding.
  const int width = active_style_->item_size.width();
  const int height = active_style_->item_size.height();
  auto i = children().begin();
  // Child order is left-to-right, top-to-bottom, so lay out row-major.  The
  // last row may not be full, so the inner loop will need to be careful about
  // the child count anyway, so don't bother to compute a row count.
  for (int y = 0;; y += height) {
    for (int x = 0, col = 0; col < active_style_->columns; ++col, x += width) {
      if (i == children().end())
        return;
      (*i++)->SetBounds(x, y, width, height);
    }
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

  views::View* selected = GetSelectedView();
  views::View* new_selected = nullptr;

  if (selected) {
    int index = GetIndexOf(selected);
    int new_index = base::ClampToRange(index + position_increment, 0,
                                       int{children().size()} - 1);
    if (index != new_index)
      new_selected = children()[size_t{new_index}];
  } else if (!children().empty()) {
    new_selected = children().front();
  }

  if (new_selected)
    new_selected->RequestFocus();
  return true;
}

base::Optional<content::DesktopMediaID> DesktopMediaListView::GetSelection() {
  DesktopMediaSourceView* view = GetSelectedView();
  return view ? base::Optional<content::DesktopMediaID>(view->source_id())
              : base::nullopt;
}

DesktopMediaListController::SourceListListener*
DesktopMediaListView::GetSourceListListener() {
  return this;
}

void DesktopMediaListView::OnSourceAdded(size_t index) {
  const DesktopMediaList::Source& source = controller_->GetSource(index);

  // We are going to have a second item, apply the generic style.
  if (children().size() == 1)
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
      aura::Window* window = DesktopMediaID::GetNativeWindowById(source.id);
      if (window)
        icon_image = LoadDefaultIcon(window);
    }
#endif
    source_view->SetIcon(icon_image);
  }
  AddChildViewAt(source_view, index);

  if ((children().size() - 1) % active_style_->columns == 0)
    controller_->OnSourceListLayoutChanged();

  PreferredSizeChanged();
}

void DesktopMediaListView::OnSourceRemoved(size_t index) {
  DesktopMediaSourceView* view = AsDesktopMediaSourceView(children()[index]);
  DCHECK(view);

  bool was_selected = view->is_selected();
  RemoveChildView(view);
  delete view;

  if (was_selected)
    OnSelectionChanged();

  if (children().size() % active_style_->columns == 0)
    controller_->OnSourceListLayoutChanged();

  // Apply single-item styling when the second source is removed.
  if (children().size() == 1)
    SetStyle(&single_style_);

  PreferredSizeChanged();
}

void DesktopMediaListView::OnSourceMoved(size_t old_index, size_t new_index) {
  ReorderChildView(children()[old_index], new_index);
  PreferredSizeChanged();
}

void DesktopMediaListView::OnSourceNameChanged(size_t index) {
  const DesktopMediaList::Source& source = controller_->GetSource(index);
  DesktopMediaSourceView* source_view =
      AsDesktopMediaSourceView(children()[index]);
  source_view->SetName(source.name);
}

void DesktopMediaListView::OnSourceThumbnailChanged(size_t index) {
  const DesktopMediaList::Source& source = controller_->GetSource(index);
  DesktopMediaSourceView* source_view =
      AsDesktopMediaSourceView(children()[index]);
  source_view->SetThumbnail(source.thumbnail);
}

void DesktopMediaListView::SetStyle(DesktopMediaSourceViewStyle* style) {
  active_style_ = style;
  controller_->SetThumbnailSize(style->image_rect.size());

  for (auto* child : children())
    AsDesktopMediaSourceView(child)->SetStyle(*active_style_);
}

DesktopMediaSourceView* DesktopMediaListView::GetSelectedView() {
  const auto i = std::find_if(
      children().cbegin(), children().cend(),
      [](View* v) { return AsDesktopMediaSourceView(v)->is_selected(); });
  return (i == children().cend()) ? nullptr : AsDesktopMediaSourceView(*i);
}

void DesktopMediaListView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGroup;
  node_data->SetName(accessible_name_);
}
