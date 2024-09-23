// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/ranges/algorithm.h"
#include "build/chromeos_buildflags.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/window.h"
#endif

using content::DesktopMediaID;

namespace {

const int kDesktopMediaSourceViewGroupId = 1;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  bool is_app =
      !browser || browser->is_type_app() || browser->is_type_app_popup();
  int idr = is_app ? IDR_APP_DEFAULT_ICON : IDR_PRODUCT_LOGO_32;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return *rb.GetImageSkiaNamed(idr);
}
#endif

DesktopMediaSourceView* AsDesktopMediaSourceView(views::View* view) {
  DCHECK(views::IsViewClass<DesktopMediaSourceView>(view));
  return static_cast<DesktopMediaSourceView*>(view);
}

}  // namespace

DesktopMediaListView::DesktopMediaListView(
    DesktopMediaListController* controller,
    DesktopMediaSourceViewStyle generic_style,
    DesktopMediaSourceViewStyle single_style,
    const std::u16string& accessible_name)
    : item_spacing_(4),
      horizontal_margins_(16),
      vertical_margins_(16),
      controller_(controller),
      single_style_(single_style),
      generic_style_(generic_style),
      active_style_(&single_style_) {
  SetStyle(&single_style_);

  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(accessible_name);
}

DesktopMediaListView::~DesktopMediaListView() {}

void DesktopMediaListView::OnSelectionChanged() {
  controller_->OnSourceSelectionChanged();
}

gfx::Size DesktopMediaListView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  const int total_rows =
      (static_cast<int>(children().size()) + active_style_->columns - 1) /
      active_style_->columns;
  return gfx::Size(active_style_->columns * active_style_->item_size.width() +
                       (active_style_->columns - 1) * item_spacing_ +
                       2 * horizontal_margins_,
                   total_rows * active_style_->item_size.height() +
                       (total_rows - 1) * item_spacing_ +
                       2 * vertical_margins_);
}

void DesktopMediaListView::Layout(PassKey) {
  // Children lay out in a grid, all with the same size and without padding.
  const int width = active_style_->item_size.width();
  const int height = active_style_->item_size.height();
  auto i = children().begin();
  // Child order is left-to-right, top-to-bottom, so lay out row-major.  The
  // last row may not be full, so the inner loop will need to be careful about
  // the child count anyway, so don't bother to compute a row count.
  for (int y = 0;; y += (height + item_spacing_)) {
    for (int x = 0, col = 0; col < active_style_->columns;
         ++col, x += (width + item_spacing_)) {
      if (i == children().end())
        return;
      (*i++)->SetBounds(x + horizontal_margins_, y + vertical_margins_, width,
                        height);
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
    size_t index = GetIndexOf(selected).value();
    size_t new_index = index + static_cast<size_t>(position_increment);
    if (position_increment < 0 &&
        index < static_cast<size_t>(-position_increment)) {
      new_index = 0;
    } else if (position_increment > 0 &&
               (index + position_increment) > (children().size() - 1)) {
      new_index = children().size() - 1;
    }
    if (index != new_index)
      new_selected = children()[new_index];
  } else if (!children().empty()) {
    new_selected = children().front();
  }

  if (new_selected)
    new_selected->RequestFocus();
  return true;
}

std::optional<content::DesktopMediaID> DesktopMediaListView::GetSelection() {
  DesktopMediaSourceView* view = GetSelectedView();
  return view ? std::optional<content::DesktopMediaID>(view->source_id())
              : std::nullopt;
}

DesktopMediaListController::SourceListListener*
DesktopMediaListView::GetSourceListListener() {
  return this;
}

void DesktopMediaListView::ClearSelection() {
  DesktopMediaSourceView* view = GetSelectedView();
  if (view) {
    view->ClearSelection();
  }
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  bool was_selected = view->GetSelected();
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

void DesktopMediaListView::OnSourcePreviewChanged(size_t index) {}

void DesktopMediaListView::OnDelegatedSourceListSelection() {
  // If the SourceList is delegated, we will only have one (or zero), sources.
  // As long as we have one source, select it once we get notified that the user
  // made a selection in the delegated source list.
  if (!children().empty())
    children().front()->RequestFocus();
}

void DesktopMediaListView::SetStyle(DesktopMediaSourceViewStyle* style) {
  active_style_ = style;
  controller_->SetThumbnailSize(style->image_rect.size());

  for (views::View* child : children()) {
    AsDesktopMediaSourceView(child)->SetStyle(*active_style_);
  }
}

DesktopMediaSourceView* DesktopMediaListView::GetSelectedView() {
  const auto i =
      base::ranges::find_if(children(), &DesktopMediaSourceView::GetSelected,
                            &AsDesktopMediaSourceView);
  return (i == children().cend()) ? nullptr : AsDesktopMediaSourceView(*i);
}
