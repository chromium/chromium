// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_view.h"

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/download/download_item_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// TODO(pkasting): Replace these with LayoutProvider constants

// Padding above the content.
constexpr int kTopPadding = 1;

// Padding from left edge and first download view.
constexpr int kStartPadding = 4;

// Padding from right edge and close button/show downloads link.
constexpr int kEndPadding = 6;

// Padding between the show all link and close button.
constexpr int kCloseAndLinkPadding = 6;

}  // namespace

DownloadShelfView::DownloadShelfView(Browser* browser, BrowserView* parent)
    : DownloadShelf(browser, browser->profile()),
      AnimationDelegateViews(this),
      parent_(parent) {
  // Start out hidden: the shelf might be created but never shown in some
  // cases, like when installing a theme. See DownloadShelf::AddDownload().
  SetVisible(false);

  show_all_view_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&chrome::ShowDownloads, browser),
      l10n_util::GetStringUTF16(IDS_SHOW_ALL_DOWNLOADS)));
  show_all_view_->SizeToPreferredSize();

  close_button_ = AddChildView(views::CreateVectorImageButton(
      base::BindRepeating(&DownloadShelf::Close, base::Unretained(this))));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button_->SizeToPreferredSize();

  accessible_alert_ = AddChildView(std::make_unique<views::View>());

  if (gfx::Animation::ShouldRenderRichAnimation()) {
    new_item_animation_.SetSlideDuration(base::Milliseconds(800));
    shelf_animation_.SetSlideDuration(base::Milliseconds(120));
  } else {
    new_item_animation_.SetSlideDuration(base::TimeDelta());
    shelf_animation_.SetSlideDuration(base::TimeDelta());
  }

  views::ViewAccessibility& accessibility = GetViewAccessibility();
  accessibility.SetName(l10n_util::GetStringUTF16(IDS_ACCNAME_DOWNLOADS_BAR),
                        ax::mojom::NameFrom::kAttribute);
  accessibility.SetRole(ax::mojom::Role::kGroup);

  // Delay 5 seconds if the mouse leaves the shelf by way of entering another
  // window. This is much larger than the normal delay as opening a download is
  // most likely going to trigger a new window to appear over the button. Delay
  // a long time so that the user has a chance to quickly close the other app
  // and return to chrome with the download shelf still open.
  mouse_watcher_.set_notify_on_exit_time(base::Seconds(5));
  SetID(VIEW_ID_DOWNLOAD_SHELF);
  views::SetCascadingColorProviderColor(this, views::kCascadingBackgroundColor,
                                        kColorToolbar);
}

DownloadShelfView::~DownloadShelfView() = default;

bool DownloadShelfView::IsShowing() const {
  return GetVisible() && shelf_animation_.IsShowing();
}

bool DownloadShelfView::IsClosing() const {
  return shelf_animation_.IsClosing();
}

views::View* DownloadShelfView::GetView() {
  return this;
}

gfx::Size DownloadShelfView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  gfx::Size prefsize(kEndPadding + kStartPadding + kCloseAndLinkPadding, 0);

  // Enlarge the preferred size enough to hold various other views side-by-side.
  const auto adjust_size = [&prefsize](views::View* view) {
    const gfx::Size size = view->GetPreferredSize();
    prefsize.Enlarge(size.width(), 0);
    prefsize.set_height(std::max(size.height(), prefsize.height()));
  };
  adjust_size(close_button_);
  adjust_size(show_all_view_);
  // Add one download view to the preferred size.
  if (!download_views_.empty())
    adjust_size(download_views_.front());

  prefsize.Enlarge(0, kTopPadding);
  return gfx::Tween::SizeValueBetween(shelf_animation_.GetCurrentValue(),
                                      gfx::Size(prefsize.width(), 0), prefsize);
}

void DownloadShelfView::Layout(PassKey) {
  int x = kStartPadding;
  const int download_items_end =
      std::max(0, width() - kEndPadding - close_button_->width() -
                      kCloseAndLinkPadding - show_all_view_->width());
  const bool all_downloads_hidden =
      !download_views_.empty() &&
      (download_views_.back()->GetPreferredSize().width() >
       (download_items_end - x));

  const auto center_y = [height = height()](int item_height) {
    return std::max((height - item_height) / 2, kTopPadding);
  };

  show_all_view_->SetPosition(
      {// If none of the download items can be shown, move the link to the left
       // to make it more obvious that there is something to see.
       all_downloads_hidden ? x : download_items_end,
       center_y(show_all_view_->height())});
  close_button_->SetPosition(
      {show_all_view_->bounds().right() + kCloseAndLinkPadding,
       center_y(close_button_->height())});

  if (all_downloads_hidden) {
    for (DownloadItemView* view : download_views_) {
      view->SetVisible(false);
    }
    return;
  }

  for (DownloadItemView* view : base::Reversed(download_views_)) {
    gfx::Size view_size = view->GetPreferredSize();
    if (view == download_views_.back()) {
      view_size = gfx::Tween::SizeValueBetween(
          new_item_animation_.GetCurrentValue(),
          gfx::Size(0, view_size.height()), view_size);
    }

    const gfx::Rect bounds({x, center_y(view_size.height())}, view_size);
    view->SetBoundsRect(bounds);
    view->SetVisible(bounds.right() < download_items_end);

    x = bounds.right();
  }
}

void DownloadShelfView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &new_item_animation_) {
    InvalidateLayout();
  } else {
    DCHECK_EQ(&shelf_animation_, animation);
    // Force a re-layout of the parent, which will call back into
    // GetPreferredSize(), where we will do our animation. In the case where the
    // animation is hiding, we do a full resize - the fast resizing would
    // otherwise leave blank white areas where the shelf was and where the
    // user's eye is. Thankfully bottom-resizing is a lot faster than
    // top-resizing.
    parent_->ToolbarSizeChanged(shelf_animation_.IsShowing());
  }
}

void DownloadShelfView::AnimationEnded(const gfx::Animation* animation) {
  if (animation != &shelf_animation_)
    return;

  const bool shown = shelf_animation_.IsShowing();
  parent_->SetDownloadShelfVisible(shown);

  // If the shelf was explicitly closed by the user, there are further steps to
  // take to complete closing.
  if (shown || is_hidden())
    return;

  // Remove all completed downloads.
  for (size_t i = 0; i < download_views_.size();) {
    DownloadItemView* const view = download_views_[i];
    DownloadUIModel* const model = view->model();
    if ((model->GetState() == download::DownloadItem::IN_PROGRESS) ||
        model->IsDangerous()) {
      // Treat the item as opened when we close. This way if we get shown again
      // the user need not open this item for the shelf to auto-close.
      model->SetOpened(true);
      ++i;
    } else {
      RemoveDownloadView(view);
    }
  }

  // Make the shelf non-visible.
  //
  // If we had keyboard focus, calling SetVisible(false) will cause keyboard
  // focus to be completely lost. To prevent this, focus the web contents.
  // TODO(crbug.com/41390999): Fix AccessiblePaneView::SetVisible() or
  // FocusManager to make this unnecessary.
  auto* focus_manager = GetFocusManager();
  if (focus_manager && Contains(focus_manager->GetFocusedView()))
    parent_->contents_web_view()->RequestFocus();
  SetVisible(false);
}

void DownloadShelfView::MouseMovedOutOfHost() {
  Close();
}

void DownloadShelfView::AutoClose() {
  if (base::ranges::all_of(download_views_, [](const DownloadItemView* view) {
        return view->model()->GetOpened();
      })) {
    mouse_watcher_.Start(GetWidget()->GetNativeWindow());
  }
}

void DownloadShelfView::RemoveDownloadView(View* view) {
  DCHECK(view);
  const auto i = base::ranges::find(download_views_, view);
  CHECK(i != download_views_.end(), base::NotFatalUntil::M130);
  download_views_.erase(i);
  RemoveChildViewT(view);
  if (download_views_.empty())
    Close();
  else
    AutoClose();
  InvalidateLayout();
}

void DownloadShelfView::ConfigureButtonForTheme(views::MdTextButton* button) {
  button->SetBgColorIdOverride(kColorDownloadShelfButtonBackground);
  button->SetEnabledTextColorIds(kColorDownloadShelfButtonText);
}

void DownloadShelfView::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download) {
  mouse_watcher_.Stop();

  const bool was_empty = download_views_.empty();

  // Insert the new view as the first child, so the logical child order matches
  // the visual order.  This ensures that tabbing through downloads happens in
  // the order users would expect.
  auto view = std::make_unique<DownloadItemView>(std::move(download), this,
                                                 accessible_alert_);
  DownloadItemView* download_item_view = AddChildViewAt(std::move(view), 0);
  download_views_.push_back(download_item_view);

  // Max number of download views we'll contain. Any time a view is added and
  // we already have this many download views, one is removed.
  // TODO(pkasting): Maybe this should use a min width instead.
  constexpr size_t kMaxDownloadViews = 15;
  if (download_views_.size() > kMaxDownloadViews)
    RemoveDownloadView(download_views_.front());

  new_item_animation_.Reset();
  new_item_animation_.Show();

  if (was_empty && !shelf_animation_.is_animating() && GetVisible()) {
    // Force a re-layout of the parent to adjust height of shelf properly.
    parent_->ToolbarSizeChanged(true);
  }
}

void DownloadShelfView::DoOpen() {
  SetVisible(true);
  shelf_animation_.Show();
}

void DownloadShelfView::DoClose() {
  parent_->SetDownloadShelfVisible(false);
  shelf_animation_.Hide();
}

void DownloadShelfView::DoHide() {
  SetVisible(false);
  parent_->ToolbarSizeChanged(false);
  parent_->SetDownloadShelfVisible(false);
}

void DownloadShelfView::DoUnhide() {
  SetVisible(true);
  parent_->ToolbarSizeChanged(true);
  parent_->SetDownloadShelfVisible(true);
}

void DownloadShelfView::OnPaintBorder(gfx::Canvas* canvas) {
  canvas->FillRect(
      gfx::Rect(0, 0, width(), 1),
      GetColorProvider()->GetColor(kColorDownloadShelfContentAreaSeparator));
}

void DownloadShelfView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();

  ConfigureButtonForTheme(show_all_view_);

  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(kColorDownloadShelfBackground)));

  const ui::ColorProvider* cp = GetColorProvider();
  views::SetImageFromVectorIconWithColor(
      close_button_, vector_icons::kCloseRoundedIcon,
      cp->GetColor(kColorDownloadShelfButtonIcon),
      cp->GetColor(kColorDownloadShelfButtonIconDisabled));
}

views::View* DownloadShelfView::GetDefaultFocusableChild() {
  return download_views_.empty() ? static_cast<views::View*>(show_all_view_)
                                 : download_views_.back();
}

DownloadItemView* DownloadShelfView::GetViewOfLastDownloadItemForTesting() {
  return download_views_.empty() ? nullptr : download_views_.back();
}

BEGIN_METADATA(DownloadShelfView)
END_METADATA
