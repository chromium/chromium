// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/native_window_notification_source.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/save_card_bubble_view.h"
#include "chrome/browser/ui/autofill/save_card_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/accessibility/invert_bubble_view.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_card_manage_cards_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_offer_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_sign_in_promo_bubble_views.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/contents_layout_manager.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/browser/ui/views/ime/ime_warning_bubble_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/quit_instruction_bubble_controller.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/browser/ui/views/update_recommended_message_box.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/command.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/password_protection/metrics_util.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/translate/core/browser/language_state.h"
#include "components/version_info/channel.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme_dark_aura.h"
#include "ui/views/accessibility/view_accessibility_utils.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/window_pin_type.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#else
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#endif  // !defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/views/frame/browser_view_commands_mac.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/win/jumplist.h"
#include "chrome/browser/win/jumplist_factory.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme_win.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
#include "chrome/browser/ui/sync/one_click_signin_links_delegate_impl.h"
#include "chrome/browser/ui/views/sync/one_click_signin_dialog_view.h"
#endif

using base::TimeDelta;
using base::UserMetricsAction;
using content::NativeWebKeyboardEvent;
using content::WebContents;
using views::ColumnSet;
using web_modal::WebContentsModalDialogHost;

namespace {

// The name of a key to store on the window handle so that other code can
// locate this object using just the handle.
const char* const kBrowserViewKey = "__BROWSER_VIEW__";

// The number of milliseconds between loading animation frames.
const int kLoadingAnimationFrameTimeMs = 30;

// See SetDisableRevealerDelayForTesting().
bool g_disable_revealer_delay_for_testing = false;

// Paints the horizontal border separating the Bookmarks Bar from the Toolbar
// or page content according to |at_top| with |color|.
void PaintDetachedBookmarkBar(gfx::Canvas* canvas,
                              BookmarkBarView* view) {
  // Paint background for detached state; if animating, this is fade in/out.
  const ui::ThemeProvider* tp = view->GetThemeProvider();
  gfx::Rect fill_rect = view->GetLocalBounds();

  // In detached mode, the bar is meant to overlap with |contents_container_|.
  // The detached background color may be partially transparent, but the layer
  // for |view| must be painted opaquely to avoid subpixel anti-aliasing
  // artifacts, so we recreate the contents container base color here.
  canvas->FillRect(fill_rect,
                   tp->GetColor(ThemeProperties::COLOR_CONTROL_BACKGROUND));
  canvas->FillRect(
      fill_rect,
      tp->GetColor(ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND));

  // Draw the separator below the detached bookmark bar.
  BrowserView::Paint1pxHorizontalLine(
      canvas,
      tp->GetColor(ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR),
      view->GetLocalBounds(), true);
}

// Paints the background (including the theme image behind content area) for
// the Bookmarks Bar when it is attached to the Toolbar into |bounds|.
// |background_origin| is the origin to use for painting the theme image.
void PaintBackgroundAttachedMode(gfx::Canvas* canvas,
                                 const ui::ThemeProvider* theme_provider,
                                 const gfx::Rect& bounds,
                                 const gfx::Point& background_origin) {
  canvas->DrawColor(theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR));

  // If there's a non-default background image, tile it.
  if (theme_provider->HasCustomImage(IDR_THEME_TOOLBAR)) {
    canvas->TileImageInt(*theme_provider->GetImageSkiaNamed(IDR_THEME_TOOLBAR),
                         background_origin.x(),
                         background_origin.y(),
                         bounds.x(),
                         bounds.y(),
                         bounds.width(),
                         bounds.height());
  }
}

void PaintAttachedBookmarkBar(gfx::Canvas* canvas,
                              BookmarkBarView* view,
                              BrowserView* browser_view,
                              int toolbar_overlap) {
  // Paint background for attached state.
  gfx::Point background_image_offset =
      browser_view->OffsetPointForToolbarBackgroundImage(
          gfx::Point(view->GetMirroredX(), view->y()));
  PaintBackgroundAttachedMode(canvas, view->GetThemeProvider(),
                              view->GetLocalBounds(), background_image_offset);
  if (view->height() >= toolbar_overlap) {
    BrowserView::Paint1pxHorizontalLine(
        canvas,
        view->GetThemeProvider()->GetColor(
            ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR),
        view->GetLocalBounds(), true);
  }
}

bool GetGestureCommand(ui::GestureEvent* event, int* command) {
  DCHECK(command);
  *command = 0;
#if defined(OS_MACOSX)
  if (event->details().type() == ui::ET_GESTURE_SWIPE) {
    if (event->details().swipe_left()) {
      *command = IDC_BACK;
      return true;
    } else if (event->details().swipe_right()) {
      *command = IDC_FORWARD;
      return true;
    }
  }
#endif  // OS_MACOSX
  return false;
}

// A view targeter for the overlay view, which makes sure the overlay view
// itself is never a target for events, but its children (i.e. top_container)
// may be.
class OverlayViewTargeterDelegate : public views::ViewTargeterDelegate {
 public:
  OverlayViewTargeterDelegate() = default;
  ~OverlayViewTargeterDelegate() override = default;

  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    for (int i = 0; i < target->child_count(); ++i) {
      gfx::RectF child_rect(rect);
      views::View::ConvertRectToTarget(target, target->child_at(i),
                                       &child_rect);
      if (target->child_at(i)->HitTestRect(gfx::ToEnclosingRect(child_rect)))
        return true;
    }

    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayViewTargeterDelegate);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// Delegate implementation for BrowserViewLayout. Usually just forwards calls
// into BrowserView.
class BrowserViewLayoutDelegateImpl : public BrowserViewLayoutDelegate {
 public:
  explicit BrowserViewLayoutDelegateImpl(BrowserView* browser_view)
      : browser_view_(browser_view) {}
  ~BrowserViewLayoutDelegateImpl() override {}

  // BrowserViewLayoutDelegate overrides:
  views::View* GetContentsWebView() const override {
    return browser_view_->contents_web_view_;
  }

  bool DownloadShelfNeedsLayout() const override {
    DownloadShelfView* download_shelf = browser_view_->download_shelf_.get();
    // Re-layout the shelf either if it is visible or if its close animation
    // is currently running.
    return download_shelf &&
           (download_shelf->IsShowing() || download_shelf->IsClosing());
  }

  bool IsTabStripVisible() const override {
    return browser_view_->IsTabStripVisible();
  }

  gfx::Rect GetBoundsForTabStripInBrowserView() const override {
    gfx::RectF bounds_f(browser_view_->frame()->GetBoundsForTabStrip(
        browser_view_->tabstrip()));
    views::View::ConvertRectToTarget(browser_view_->parent(), browser_view_,
        &bounds_f);
    return gfx::ToEnclosingRect(bounds_f);
  }

  int GetTopInsetInBrowserView() const override {
    return browser_view_->frame()->GetTopInset() - browser_view_->y();
  }

  int GetThemeBackgroundXInset() const override {
    // TODO(pkotwicz): Return the inset with respect to the left edge of the
    // BrowserView.
    return browser_view_->frame()->GetThemeBackgroundXInset();
  }

  bool IsToolbarVisible() const override {
    return browser_view_->IsToolbarVisible();
  }

  bool IsBookmarkBarVisible() const override {
    return browser_view_->IsBookmarkBarVisible();
  }

  ExclusiveAccessBubbleViews* GetExclusiveAccessBubble() const override {
    return browser_view_->exclusive_access_bubble();
  }

  bool IsTopControlsSlideBehaviorEnabled() const override {
    return browser_view_->IsTopControlsSlideBehaviorEnabled();
  }

  float GetTopControlsSlideBehaviorShownRatio() const override {
    return browser_view_->GetTopControlsSlideBehaviorShownRatio();
  }

 private:
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayoutDelegateImpl);
};

// This class is used to paint the background for Bookmarks Bar.
class BookmarkBarViewBackground : public views::Background {
 public:
  BookmarkBarViewBackground(BrowserView* browser_view,
                            BookmarkBarView* bookmark_bar_view);

  // views:Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

 private:
  BrowserView* browser_view_;

  // The view hosting this background.
  BookmarkBarView* bookmark_bar_view_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewBackground);
};

BookmarkBarViewBackground::BookmarkBarViewBackground(
    BrowserView* browser_view,
    BookmarkBarView* bookmark_bar_view)
    : browser_view_(browser_view), bookmark_bar_view_(bookmark_bar_view) {}

void BookmarkBarViewBackground::Paint(gfx::Canvas* canvas,
                                      views::View* view) const {
  int toolbar_overlap = bookmark_bar_view_->GetToolbarOverlap();

  SkAlpha detached_alpha = static_cast<SkAlpha>(
      bookmark_bar_view_->size_animation().CurrentValueBetween(0xff, 0));
  if (detached_alpha != 0xff) {
    PaintAttachedBookmarkBar(canvas, bookmark_bar_view_, browser_view_,
                             toolbar_overlap);
  }

  if (!bookmark_bar_view_->IsDetached() || detached_alpha == 0)
    return;

  // While animating, set opacity to cross-fade between attached and detached
  // backgrounds including their respective separators.
  canvas->SaveLayerAlpha(detached_alpha);
  PaintDetachedBookmarkBar(canvas, bookmark_bar_view_);
  canvas->Restore();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, public:

// static
const char BrowserView::kViewClassName[] = "BrowserView";

BrowserView::BrowserView() : views::ClientView(nullptr, nullptr) {}

BrowserView::~BrowserView() {
  // Destroy the top controls slide controller first as it depends on the
  // tabstrip model and the browser frame.
  top_controls_slide_controller_.reset();

  // All the tabs should have been destroyed already. If we were closed by the
  // OS with some tabs than the NativeBrowserFrame should have destroyed them.
  DCHECK_EQ(0, browser_->tab_strip_model()->count());

  // Stop the animation timer explicitly here to avoid running it in a nested
  // message loop, which may run by Browser destructor.
  loading_animation_timer_.Stop();

  // Immersive mode may need to reparent views before they are removed/deleted.
  immersive_mode_controller_.reset();

  browser_->tab_strip_model()->RemoveObserver(this);

  extensions::ExtensionCommandsGlobalRegistry* global_registry =
      extensions::ExtensionCommandsGlobalRegistry::Get(browser_->profile());
  if (global_registry->registry_for_active_window() ==
          extension_keybinding_registry_.get())
    global_registry->set_registry_for_active_window(nullptr);

  // We destroy the download shelf before |browser_| to remove its child
  // download views from the set of download observers (since the observed
  // downloads can be destroyed along with |browser_| and the observer
  // notifications will call back into deleted objects).
  BrowserViewLayout* browser_view_layout = GetBrowserViewLayout();
  if (browser_view_layout)
    browser_view_layout->set_download_shelf(nullptr);
  download_shelf_.reset();

  // The TabStrip attaches a listener to the model. Make sure we shut down the
  // TabStrip first so that it can cleanly remove the listener.
  if (tabstrip_) {
    tabstrip_->parent()->RemoveChildView(tabstrip_);
    if (browser_view_layout)
      browser_view_layout->set_tab_strip(nullptr);
    delete tabstrip_;
    tabstrip_ = nullptr;
  }
  // Child views maintain PrefMember attributes that point to
  // OffTheRecordProfile's PrefService which gets deleted by ~Browser.
  RemoveAllChildViews(true);
  toolbar_ = nullptr;
}

void BrowserView::Init(std::unique_ptr<Browser> browser) {
  browser_ = std::move(browser);
  browser_->tab_strip_model()->AddObserver(this);
  immersive_mode_controller_.reset(chrome::CreateImmersiveModeController());
}

// static
BrowserView* BrowserView::GetBrowserViewForNativeWindow(
    gfx::NativeWindow window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  return widget ?
      reinterpret_cast<BrowserView*>(widget->GetNativeWindowProperty(
          kBrowserViewKey)) : nullptr;
}

// static
BrowserView* BrowserView::GetBrowserViewForBrowser(const Browser* browser) {
  return static_cast<BrowserView*>(browser->window());
}

// static
void BrowserView::Paint1pxHorizontalLine(gfx::Canvas* canvas,
                                         SkColor color,
                                         const gfx::Rect& bounds,
                                         bool at_bottom) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  gfx::RectF rect(gfx::ScaleRect(gfx::RectF(bounds), scale));
  const float inset = rect.height() - 1;
  rect.Inset(0, at_bottom ? inset : 0, 0, at_bottom ? 0 : inset);
  cc::PaintFlags flags;
  flags.setColor(color);
  canvas->sk_canvas()->drawRect(gfx::RectFToSkRect(rect), flags);
}

// static
void BrowserView::SetDisableRevealerDelayForTesting(bool disable) {
  g_disable_revealer_delay_for_testing = disable;
}

void BrowserView::InitStatusBubble() {
  status_bubble_ = std::make_unique<StatusBubbleViews>(contents_web_view_);
  contents_web_view_->SetStatusBubble(status_bubble_.get());
}

gfx::Rect BrowserView::GetFindBarBoundingBox() const {
  return GetBrowserViewLayout()->GetFindBarBoundingBox();
}

int BrowserView::GetTabStripHeight() const {
  // We want to return tabstrip_->height(), but we might be called in the midst
  // of layout, when that hasn't yet been updated to reflect the current state.
  // So return what the tabstrip height _ought_ to be right now.
  return IsTabStripVisible() ? tabstrip_->GetPreferredSize().height() : 0;
}

gfx::Point BrowserView::OffsetPointForToolbarBackgroundImage(
    const gfx::Point& point) const {
  // The background image starts tiling horizontally at the window left edge and
  // vertically at the top edge of the horizontal tab strip (or where it would
  // be).  We expect our parent's origin to be the window origin.
  gfx::Point window_point(point + GetMirroredPosition().OffsetFromOrigin());
  window_point.Offset(frame_->GetThemeBackgroundXInset(),
                      -frame_->GetTopInset());
  return window_point;
}

bool BrowserView::IsTabStripVisible() const {
  // Return false if this window does not normally display a tabstrip.
  if (!browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP))
    return false;

  // Return false if the tabstrip has not yet been created (by InitViews()),
  // since callers may otherwise try to access it. Note that we can't just check
  // this alone, as the tabstrip is created unconditionally even for windows
  // that won't display it.
  return tabstrip_ != nullptr;
}

bool BrowserView::IsIncognito() const {
  // TODO(pbos): This is confusing or incorrect as IsIncognito() returns true
  // for Guest sessions. We should probably rename this function and audit
  // callers to make sure that's actually what was intended, or make this return
  // false for Guest sessions.
  return browser_->profile()->IsOffTheRecord();
}

bool BrowserView::IsGuestSession() const {
  return browser_->profile()->IsGuestSession();
}

bool BrowserView::IsRegularOrGuestSession() const {
  return profiles::IsRegularOrGuestSession(browser_.get());
}

bool BrowserView::GetAccelerator(int cmd_id,
                                 ui::Accelerator* accelerator) const {
#if defined(OS_MACOSX)
  // On macOS, most accelerators are defined in MainMenu.xib and are user
  // configurable. Furthermore, their values and enabled state depends on the
  // key window. Views code relies on a static mapping that is not dependent on
  // the key window. Thus, we provide the default Mac accelerator for each
  // CommandId, which is static. This may be inaccurate, but is at least
  // sufficiently well defined for Views to use.
  if (GetDefaultMacAcceleratorForCommandId(cmd_id, accelerator))
    return true;
#else
  // We retrieve the accelerator information for standard accelerators
  // for cut, copy and paste.
  if (GetStandardAcceleratorForCommandId(cmd_id, accelerator))
    return true;
#endif
  // Else, we retrieve the accelerator information from the accelerator table.
  for (auto it = accelerator_table_.begin(); it != accelerator_table_.end();
       ++it) {
    if (it->second == cmd_id) {
      *accelerator = it->first;
      return true;
    }
  }
  return false;
}

bool BrowserView::IsAcceleratorRegistered(const ui::Accelerator& accelerator) {
  return accelerator_table_.find(accelerator) != accelerator_table_.end();
}

WebContents* BrowserView::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

bool BrowserView::IsBrowserTypeHostedApp() const {
  return extensions::HostedAppBrowserController::
      IsForExperimentalHostedAppBrowser(browser_.get());
}

bool BrowserView::IsTopControlsSlideBehaviorEnabled() const {
  return top_controls_slide_controller_ &&
         top_controls_slide_controller_->IsEnabled();
}

float BrowserView::GetTopControlsSlideBehaviorShownRatio() const {
  if (top_controls_slide_controller_)
    return top_controls_slide_controller_->GetShownRatio();

  return 1.f;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindow implementation:

void BrowserView::Show() {
#if !defined(OS_WIN) && !defined(OS_CHROMEOS)
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behavior under
  // Windows and Chrome OS, but other platforms will not trigger
  // OnWidgetActivationChanged() until we return to the runloop. Therefore any
  // calls to Browser::GetLastActive() will return the wrong result if we do not
  // explicitly set it here.
  // A similar block also appears in BrowserWindowCocoa::Show().
  BrowserList::SetLastActive(browser());
#endif

  // If the window is already visible, just activate it.
  if (frame_->IsVisible()) {
    frame_->Activate();
    return;
  }

  // Showing the window doesn't make the browser window active right away.
  // This can cause SetFocusToLocationBar() to skip setting focus to the
  // location bar. To avoid this we explicilty let SetFocusToLocationBar()
  // know that it's ok to steal focus.
  force_location_bar_focus_ = true;

  // Setting the focus doesn't work when the window is invisible, so any focus
  // initialization that happened before this will be lost.
  //
  // We really "should" restore the focus whenever the window becomes unhidden,
  // but I think initializing is the only time where this can happen where
  // there is some focus change we need to pick up, and this is easier than
  // plumbing through an un-hide message all the way from the frame.
  //
  // If we do find there are cases where we need to restore the focus on show,
  // that should be added and this should be removed.
  RestoreFocus();

  frame_->Show();

  force_location_bar_focus_ = false;

  browser()->OnWindowDidShow();

  MaybeShowInvertBubbleView(this);
}

void BrowserView::ShowInactive() {
  if (!frame_->IsVisible())
    frame_->ShowInactive();
}

void BrowserView::Hide() {
  // Not implemented.
}

bool BrowserView::IsVisible() const {
  return frame_->IsVisible();
}

void BrowserView::SetBounds(const gfx::Rect& bounds) {
  ExitFullscreen();
  GetWidget()->SetBounds(bounds);
}

void BrowserView::Close() {
  frame_->Close();
}

void BrowserView::Activate() {
  frame_->Activate();
}

void BrowserView::Deactivate() {
  frame_->Deactivate();
}

bool BrowserView::IsActive() const {
  return frame_->IsActive();
}

void BrowserView::FlashFrame(bool flash) {
  frame_->FlashFrame(flash);
}

bool BrowserView::IsAlwaysOnTop() const {
  return false;
}

void BrowserView::SetAlwaysOnTop(bool always_on_top) {
  // Not implemented for browser windows.
  NOTIMPLEMENTED();
}

gfx::NativeWindow BrowserView::GetNativeWindow() const {
  // While the browser destruction is going on, the widget can already be gone,
  // but utility functions like FindBrowserWithWindow will still call this.
  return GetWidget() ? GetWidget()->GetNativeWindow() : nullptr;
}

void BrowserView::SetTopControlsShownRatio(content::WebContents* web_contents,
                                           float ratio) {
  if (top_controls_slide_controller_)
    top_controls_slide_controller_->SetShownRatio(web_contents, ratio);
}

bool BrowserView::DoBrowserControlsShrinkRendererSize(
    const content::WebContents* contents) const {
  return top_controls_slide_controller_ &&
         top_controls_slide_controller_->DoBrowserControlsShrinkRendererSize(
             contents);
}

int BrowserView::GetTopControlsHeight() const {
  if (top_controls_slide_controller_ &&
      top_controls_slide_controller_->IsEnabled()) {
    return top_container_->bounds().height();
  }

  // If the top controls slide feature is disabled, we must give the renderers
  // a value of 0, so as they don't get confused thinking that they need to move
  // the top controls first before the pages start scrolling.
  return 0.f;
}

void BrowserView::SetTopControlsGestureScrollInProgress(bool in_progress) {
  if (top_controls_slide_controller_) {
    top_controls_slide_controller_->SetTopControlsGestureScrollInProgress(
        in_progress);
  }
}

StatusBubble* BrowserView::GetStatusBubble() {
  return status_bubble_.get();
}

void BrowserView::UpdateTitleBar() {
  frame_->UpdateWindowTitle();
  if (!loading_animation_timer_.IsRunning())
    frame_->UpdateWindowIcon();
}

void BrowserView::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  if (bookmark_bar_view_.get()) {
    BookmarkBar::State new_state = browser_->bookmark_bar_state();

    // We don't properly support animating the bookmark bar to and from the
    // detached state in immersive fullscreen.
    bool detached_changed = (new_state == BookmarkBar::DETACHED) ||
                            bookmark_bar_view_->IsDetached();
    if (detached_changed && immersive_mode_controller_->IsEnabled())
      change_type = BookmarkBar::DONT_ANIMATE_STATE_CHANGE;
    bookmark_bar_view_->SetBookmarkBarState(new_state, change_type);
  }

  if (MaybeShowBookmarkBar(GetActiveWebContents()))
    Layout();
}

void BrowserView::UpdateDevTools() {
  UpdateDevToolsForContents(GetActiveWebContents(), true);
  Layout();
}

void BrowserView::UpdateLoadingAnimations(bool should_animate) {
  if (should_animate) {
    if (!loading_animation_timer_.IsRunning()) {
      // Loads are happening, and the timer isn't running, so start it.
      loading_animation_timer_.Start(FROM_HERE,
          TimeDelta::FromMilliseconds(kLoadingAnimationFrameTimeMs), this,
          &BrowserView::LoadingAnimationCallback);
    }
  } else {
    if (loading_animation_timer_.IsRunning()) {
      loading_animation_timer_.Stop();
      // Loads are now complete, update the state if a task was scheduled.
      LoadingAnimationCallback();
    }
  }
}

void BrowserView::SetStarredState(bool is_starred) {
  GetLocationBarView()->SetStarToggled(is_starred);
}

void BrowserView::SetTranslateIconToggled(bool is_lit) {
  // Translate icon is never active on Views.
}

void BrowserView::OnActiveTabChanged(content::WebContents* old_contents,
                                     content::WebContents* new_contents,
                                     int index,
                                     int reason) {
  DCHECK(new_contents);

  if (old_contents && !old_contents->IsBeingDestroyed()) {
    // We do not store the focus when closing the tab to work-around bug 4633.
    // Some reports seem to show that the focus manager and/or focused view can
    // be garbage at that point, it is not clear why.
    old_contents->StoreFocus();
  }

  // If |contents_container_| already has the correct WebContents, we can save
  // some work.  This also prevents extra events from being reported by the
  // Visibility API under Windows, as ChangeWebContents will briefly hide
  // the WebContents window.
  bool change_tab_contents =
      contents_web_view_->web_contents() != new_contents;

  bool will_restore_focus = !browser_->tab_strip_model()->closing_all() &&
                            GetWidget()->IsActive() && GetWidget()->IsVisible();

  // Update various elements that are interested in knowing the current
  // WebContents.

  // When we toggle the NTP floating bookmarks bar and/or the info bar,
  // we don't want any WebContents to be attached, so that we
  // avoid an unnecessary resize and re-layout of a WebContents.
  if (change_tab_contents) {
    if (will_restore_focus) {
      // Manually clear focus before setting focus behavior so that the focus
      // is not temporarily advanced to an arbitrary place in the UI via
      // SetFocusBehavior(FocusBehavior::NEVER), confusing screen readers.
      // The saved focus for new_contents is restored after it is attached.
      // In addition, this ensures that the next RestoreFocus() will be
      // read out to screen readers, even if focus doesn't actually change.
      GetWidget()->GetFocusManager()->ClearFocus();
    }
    contents_web_view_->SetWebContents(nullptr);
    devtools_web_view_->SetWebContents(nullptr);
  }

  // Do this before updating InfoBarContainer as the InfoBarContainer may
  // callback to us and trigger layout.
  if (bookmark_bar_view_.get()) {
    bookmark_bar_view_->SetBookmarkBarState(
        browser_->bookmark_bar_state(),
        BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
  }

  infobar_container_->ChangeInfoBarManager(
      InfoBarService::FromWebContents(new_contents));

  UpdateUIForContents(new_contents);
  RevealTabStripIfNeeded();

  // Layout for DevTools _before_ setting the both main and devtools WebContents
  // to avoid toggling the size of any of them.
  UpdateDevToolsForContents(new_contents, !change_tab_contents);

  if (change_tab_contents) {
    // When the location bar or other UI focus will be restored, first focus the
    // root view so that screen readers announce the current page title. The
    // kFocusContext event will delay the subsequent focus event so that screen
    // readers register them as distinct events.
    if (will_restore_focus) {
      ChromeWebContentsViewFocusHelper* focus_helper =
          ChromeWebContentsViewFocusHelper::FromWebContents(new_contents);
      if (focus_helper &&
          focus_helper->GetStoredFocus() != contents_web_view_) {
        GetWidget()->GetRootView()->NotifyAccessibilityEvent(
            ax::mojom::Event::kFocusContext, true);
      }
    }

    web_contents_close_handler_->ActiveTabChanged();
    contents_web_view_->SetWebContents(new_contents);
    SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(new_contents);
    if (sad_tab_helper)
      sad_tab_helper->ReinstallInWebView();

    // The second layout update should be no-op. It will just set the
    // DevTools WebContents.
    UpdateDevToolsForContents(new_contents, true);
  }

  if (will_restore_focus) {
    // We only restore focus if our window is visible, to avoid invoking blur
    // handlers when we are eventually shown.
    new_contents->RestoreFocus();
  }

  // Update all the UI bits.
  UpdateTitleBar();

  TranslateBubbleView::CloseCurrentBubble();
}

void BrowserView::OnTabDetached(content::WebContents* contents,
                                bool was_active) {
  if (was_active) {
    // We need to reset the current tab contents to null before it gets
    // freed. This is because the focus manager performs some operations
    // on the selected WebContents when it is removed.
    web_contents_close_handler_->ActiveTabChanged();
    contents_web_view_->SetWebContents(nullptr);
    infobar_container_->ChangeInfoBarManager(nullptr);
    UpdateDevToolsForContents(nullptr, true);
  }
}

void BrowserView::ZoomChangedForActiveTab(bool can_show_bubble) {
  const AppMenuButton* app_menu_button =
      toolbar_button_provider()->GetAppMenuButton();
  bool app_menu_showing = app_menu_button && app_menu_button->IsMenuShowing();
  toolbar_button_provider()
      ->GetPageActionIconContainerView()
      ->ZoomChangedForActiveTab(can_show_bubble && !app_menu_showing);
}

gfx::Rect BrowserView::GetRestoredBounds() const {
  gfx::Rect bounds;
  ui::WindowShowState state;
  frame_->GetWindowPlacement(&bounds, &state);
  return bounds;
}

ui::WindowShowState BrowserView::GetRestoredState() const {
  gfx::Rect bounds;
  ui::WindowShowState state;
  frame_->GetWindowPlacement(&bounds, &state);
  return state;
}

gfx::Rect BrowserView::GetBounds() const {
  return frame_->GetWindowBoundsInScreen();
}

gfx::Size BrowserView::GetContentsSize() const {
  DCHECK(initialized_);
  return contents_web_view_->size();
}

bool BrowserView::IsMaximized() const {
  return frame_->IsMaximized();
}

bool BrowserView::IsMinimized() const {
  return frame_->IsMinimized();
}

void BrowserView::Maximize() {
  frame_->Maximize();
}

void BrowserView::Minimize() {
  frame_->Minimize();
}

void BrowserView::Restore() {
  frame_->Restore();
}

void BrowserView::EnterFullscreen(const GURL& url,
                                  ExclusiveAccessBubbleType bubble_type) {
  if (IsFullscreen())
    return;  // Nothing to do.

  ProcessFullscreen(true, url, bubble_type);
}

void BrowserView::ExitFullscreen() {
  if (!IsFullscreen())
    return;  // Nothing to do.

  ProcessFullscreen(false, GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
}

void BrowserView::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
    bool force_update) {
  // Immersive mode has no exit bubble because it has a visible strip at the
  // top that gives the user a hover target. In a public session we show the
  // bubble.
  // TODO(jamescook): Figure out what to do with mouse-lock.
  if (bubble_type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE ||
      (ShouldUseImmersiveFullscreenForUrl(url) &&
       !profiles::IsPublicSession())) {
    // |exclusive_access_bubble_.reset()| will trigger callback for current
    // bubble with |ExclusiveAccessBubbleHideReason::kInterrupted| if available.
    exclusive_access_bubble_.reset();
    if (bubble_first_hide_callback) {
      std::move(bubble_first_hide_callback)
          .Run(ExclusiveAccessBubbleHideReason::kNotShown);
    }
    return;
  }

  if (exclusive_access_bubble_) {
    exclusive_access_bubble_->UpdateContent(
        url, bubble_type, std::move(bubble_first_hide_callback), force_update);
    return;
  }

  exclusive_access_bubble_.reset(new ExclusiveAccessBubbleViews(
      this, url, bubble_type, std::move(bubble_first_hide_callback)));
}

void BrowserView::OnExclusiveAccessUserInput() {
  if (exclusive_access_bubble_.get())
    exclusive_access_bubble_->OnUserInput();
}

bool BrowserView::ShouldHideUIForFullscreen() const {
  // Immersive mode needs UI for the slide-down top panel.
  if (immersive_mode_controller_->IsEnabled())
    return false;

  return frame_->GetFrameView()->ShouldHideTopUIForFullscreen();
}

bool BrowserView::IsFullscreen() const {
  return frame_->IsFullscreen();
}

bool BrowserView::IsFullscreenBubbleVisible() const {
  return exclusive_access_bubble_ != nullptr;
}

void BrowserView::RestoreFocus() {
  WebContents* selected_web_contents = GetActiveWebContents();
  if (selected_web_contents)
    selected_web_contents->RestoreFocus();
}

void BrowserView::FullscreenStateChanged() {
  bool fullscreen = IsFullscreen();
  ProcessFullscreen(
      fullscreen, GURL(),
      fullscreen
          ? GetExclusiveAccessManager()->GetExclusiveAccessExitBubbleType()
          : EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
  frame_->GetFrameView()->OnFullscreenStateChanged();
}

void BrowserView::SetToolbarButtonProvider(ToolbarButtonProvider* provider) {
  // There should only be one toolbar button provider.
  DCHECK(!toolbar_button_provider_);
  toolbar_button_provider_ = provider;
}

PageActionIconContainer* BrowserView::GetPageActionIconContainer() {
  return toolbar_button_provider_->GetPageActionIconContainerView();
}

LocationBar* BrowserView::GetLocationBar() const {
  return GetLocationBarView();
}

void BrowserView::SetFocusToLocationBar(bool select_all) {
  // On Windows, changing focus to the location bar causes the browser window to
  // become active. This can steal focus if the user has another window open
  // already. On Chrome OS, changing focus makes a view believe it has a focus
  // even if the widget doens't have a focus. Either cases, we need to ignore
  // this when the browser window isn't active.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  if (!force_location_bar_focus_ && !IsActive())
    return;
#endif

  LocationBarView* location_bar = GetLocationBarView();
  location_bar->FocusLocation(select_all);
  if (!location_bar->omnibox_view()->HasFocus()) {
    // If none of location bar got focus, then clear focus.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    focus_manager->ClearFocus();
  }
}

void BrowserView::UpdateReloadStopState(bool is_loading, bool force) {
  if (toolbar_->reload_button()) {
    toolbar_->reload_button()->ChangeMode(
        is_loading ? ReloadButton::Mode::kStop : ReloadButton::Mode::kReload,
        force);
  }
}

void BrowserView::UpdateToolbar(content::WebContents* contents) {
  // We may end up here during destruction.
  if (toolbar_)
    toolbar_->Update(contents);
}

void BrowserView::ResetToolbarTabState(content::WebContents* contents) {
  // We may end up here during destruction.
  if (toolbar_)
    toolbar_->ResetTabState(contents);
}

void BrowserView::FocusToolbar() {
  // Temporarily reveal the top-of-window views (if not already revealed) so
  // that the toolbar is visible and is considered focusable. If the
  // toolbar gains focus, |immersive_mode_controller_| will keep the
  // top-of-window views revealed.
  std::unique_ptr<ImmersiveRevealedLock> focus_reveal_lock(
      immersive_mode_controller_->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES));

  // Start the traversal within the main toolbar. SetPaneFocus stores
  // the current focused view before changing focus.
  toolbar_button_provider_->FocusToolbar();
}

ToolbarActionsBar* BrowserView::GetToolbarActionsBar() {
  BrowserActionsContainer* container =
      toolbar_button_provider_->GetBrowserActionsContainer();
  return container ? container->toolbar_actions_bar() : nullptr;
}

void BrowserView::ToolbarSizeChanged(bool is_animating) {
  if (is_animating)
    contents_web_view_->SetFastResize(true);
  UpdateUIForContents(GetActiveWebContents());
  if (is_animating)
    contents_web_view_->SetFastResize(false);

  // When transitioning from animating to not animating we need to make sure the
  // contents_container_ gets layed out. If we don't do this and the bounds
  // haven't changed contents_container_ won't get a Layout and we'll end up
  // with a gray rect because the clip wasn't updated.
  if (!is_animating) {
    contents_web_view_->InvalidateLayout();
    contents_container_->Layout();
  }
}

void BrowserView::FocusBookmarksToolbar() {
  DCHECK(!immersive_mode_controller_->IsEnabled());
  if (bookmark_bar_view_ && bookmark_bar_view_->visible() &&
      bookmark_bar_view_->GetPreferredSize().height() != 0) {
    bookmark_bar_view_->SetPaneFocusAndFocusDefault();
  }
}

void BrowserView::FocusInactivePopupForAccessibility() {
  if (GetLocationBarView()->ActivateFirstInactiveBubbleForAccessibility())
    return;

  if (infobar_container_->child_count() > 0)
    infobar_container_->SetPaneFocusAndFocusDefault();
}

void BrowserView::FocusAppMenu() {
  // Chrome doesn't have a traditional menu bar, but it has a menu button in the
  // main toolbar that plays the same role.  If the user presses a key that
  // would typically focus the menu bar, tell the toolbar to focus the menu
  // button.  If the user presses the key again, return focus to the previous
  // location.
  //
  // Not used on the Mac, which has a normal menu bar.
  if (toolbar_->IsAppMenuFocused()) {
    RestoreFocus();
  } else {
    DCHECK(!immersive_mode_controller_->IsEnabled());
    toolbar_->SetPaneFocusAndFocusAppMenu();
  }
}

void BrowserView::RotatePaneFocus(bool forwards) {
  // If an inactive bubble is showing this intentionally focuses that dialog to
  // provide an easy access method to these dialogs without requring additional
  // keyboard shortcuts or commands. To get back out to pane cycling the dialog
  // needs to be accepted or dismissed.
  if (GetLocationBarView()->ActivateFirstInactiveBubbleForAccessibility())
    return;

  GetFocusManager()->RotatePaneFocus(
      forwards ?
          views::FocusManager::kForward : views::FocusManager::kBackward,
      views::FocusManager::kWrap);
}

void BrowserView::DestroyBrowser() {
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  if (added_quit_instructions_) {
    GetWidget()->GetNativeView()->RemovePreTargetHandler(
        QuitInstructionBubbleController::GetInstance());
  }
#endif

  // After this returns other parts of Chrome are going to be shutdown. Close
  // the window now so that we are deleted immediately and aren't left holding
  // references to deleted objects.
  GetWidget()->RemoveObserver(this);
  frame_->CloseNow();
}

bool BrowserView::IsBookmarkBarVisible() const {
  if (!browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR))
    return false;
  if (!bookmark_bar_view_)
    return false;
  if (!bookmark_bar_view_->parent())
    return false;
  if (bookmark_bar_view_->GetPreferredSize().height() == 0)
    return false;
  // New tab page needs visible bookmarks even when top-views are hidden.
  if (immersive_mode_controller_->ShouldHideTopViews() &&
      !bookmark_bar_view_->IsDetached())
    return false;
  return true;
}

bool BrowserView::IsBookmarkBarAnimating() const {
  return bookmark_bar_view_.get() &&
         bookmark_bar_view_->size_animation().is_animating();
}

bool BrowserView::IsTabStripEditable() const {
  return tabstrip_->IsTabStripEditable();
}

bool BrowserView::IsToolbarVisible() const {
  if (immersive_mode_controller_->ShouldHideTopViews())
    return false;
  // It's possible to reach here before we've been notified of being added to a
  // widget, so |toolbar_| is still null.  Return false in this case so callers
  // don't assume they can access the toolbar yet.
  return (browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
          browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR)) &&
         toolbar_;
}

bool BrowserView::IsToolbarShowing() const {
  return IsToolbarVisible();
}

bool BrowserView::IsInfoBarVisible() const {
  return GetBrowserViewLayout()->IsInfobarVisible();
}

void BrowserView::ShowUpdateChromeDialog() {
  UpdateRecommendedMessageBox::Show(GetNativeWindow());
}

#if defined(OS_CHROMEOS)
void BrowserView::ShowIntentPickerBubble(
    std::vector<IntentPickerBubbleView::AppInfo> app_info,
    bool disable_stay_in_chrome,
    IntentPickerResponse callback) {
  toolbar_->ShowIntentPickerBubble(std::move(app_info), disable_stay_in_chrome,
                                   std::move(callback));
}

void BrowserView::SetIntentPickerViewVisibility(bool visible) {
  LocationBarView* location_bar = GetLocationBarView();

  if (!location_bar->intent_picker_view())
    return;

  if (location_bar->intent_picker_view()->visible() != visible) {
    location_bar->intent_picker_view()->SetVisible(visible);
    location_bar->Layout();
  }
}
#endif  //  defined(OS_CHROMEOS)

void BrowserView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  toolbar_->ShowBookmarkBubble(url, already_bookmarked,
                               bookmark_bar_view_.get());
}

autofill::SaveCardBubbleView* BrowserView::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    autofill::SaveCardBubbleController* controller,
    bool user_gesture) {
  LocationBarView* location_bar = GetLocationBarView();
  PageActionIconView* card_view = location_bar->save_credit_card_icon_view();

  autofill::BubbleType bubble_type = controller->GetBubbleType();
  autofill::SaveCardBubbleViews* bubble = nullptr;

  switch (bubble_type) {
    case autofill::BubbleType::LOCAL_SAVE:
    case autofill::BubbleType::UPLOAD_SAVE:
      bubble = new autofill::SaveCardOfferBubbleViews(
          location_bar, gfx::Point(), web_contents, controller);
      break;
    case autofill::BubbleType::SIGN_IN_PROMO:
      bubble = new autofill::SaveCardSignInPromoBubbleViews(
          location_bar, gfx::Point(), web_contents, controller);
      break;
    case autofill::BubbleType::MANAGE_CARDS:
      bubble = new autofill::SaveCardManageCardsBubbleViews(
          location_bar, gfx::Point(), web_contents, controller);
      break;
    case autofill::BubbleType::INACTIVE:
      break;
  }

  DCHECK(bubble);

  if (card_view)
    bubble->SetHighlightedButton(card_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);

  bubble->Show(user_gesture ? autofill::SaveCardBubbleViews::USER_GESTURE
                            : autofill::SaveCardBubbleViews::AUTOMATIC);
  return bubble;
}

autofill::LocalCardMigrationBubble* BrowserView::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    autofill::LocalCardMigrationBubbleController* controller,
    bool user_gesture) {
  LocationBarView* location_bar = GetLocationBarView();
  PageActionIconView* card_view =
      location_bar->local_card_migration_icon_view();
  autofill::LocalCardMigrationBubbleViews* bubble =
      new autofill::LocalCardMigrationBubbleViews(location_bar, gfx::Point(),
                                                  web_contents, controller);
  if (card_view)
    bubble->SetHighlightedButton(card_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(user_gesture
                   ? autofill::LocalCardMigrationBubbleViews::USER_GESTURE
                   : autofill::LocalCardMigrationBubbleViews::AUTOMATIC);
  return bubble;
}

ShowTranslateBubbleResult BrowserView::ShowTranslateBubble(
    content::WebContents* web_contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  if (contents_web_view_->HasFocus() &&
      !GetLocationBarView()->IsMouseHovered() &&
      web_contents->IsFocusedElementEditable()) {
    return ShowTranslateBubbleResult::EDITABLE_FIELD_IS_ACTIVE;
  }

  translate::LanguageState& language_state =
      ChromeTranslateClient::FromWebContents(web_contents)->GetLanguageState();
  language_state.SetTranslateEnabled(true);

  if (IsMinimized())
    return ShowTranslateBubbleResult::BROWSER_WINDOW_MINIMIZED;

  toolbar_->ShowTranslateBubble(web_contents, step, error_type,
                                is_user_gesture);
  return ShowTranslateBubbleResult::SUCCESS;
}

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
void BrowserView::ShowOneClickSigninConfirmation(
    const base::string16& email,
    const StartSyncCallback& start_sync_callback) {
  std::unique_ptr<OneClickSigninLinksDelegate> delegate(
      new OneClickSigninLinksDelegateImpl(browser()));
  OneClickSigninDialogView::ShowDialog(email, std::move(delegate),
                                       GetNativeWindow(), start_sync_callback);
}
#endif

void BrowserView::SetDownloadShelfVisible(bool visible) {
  DCHECK(download_shelf_);
  browser_->UpdateDownloadShelfVisibility(visible);

  // SetDownloadShelfVisible can force-close the shelf, so make sure we lay out
  // everything correctly, as if the animation had finished. This doesn't
  // matter for showing the shelf, as the show animation will do it.
  ToolbarSizeChanged(false);
}

bool BrowserView::IsDownloadShelfVisible() const {
  return download_shelf_.get() && download_shelf_->IsShowing();
}

DownloadShelf* BrowserView::GetDownloadShelf() {
  DCHECK(browser_->SupportsWindowFeature(Browser::FEATURE_DOWNLOADSHELF));
  if (!download_shelf_.get()) {
    download_shelf_.reset(new DownloadShelfView(browser_.get(), this));
    download_shelf_->set_owned_by_client();
    GetBrowserViewLayout()->set_download_shelf(download_shelf_.get());
  }
  return download_shelf_.get();
}

void BrowserView::ConfirmBrowserCloseWithPendingDownloads(
    int download_count,
    Browser::DownloadClosePreventionType dialog_type,
    bool app_modal,
    const base::Callback<void(bool)>& callback) {
  DownloadInProgressDialogView::Show(
      GetNativeWindow(), download_count, dialog_type, app_modal, callback);
}

void BrowserView::UserChangedTheme() {
  frame_->FrameTypeChanged();
}

void BrowserView::ShowAppMenu() {
  if (!toolbar_button_provider_->GetAppMenuButton())
    return;

  // Keep the top-of-window views revealed as long as the app menu is visible.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller_->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));

  toolbar_button_provider_->GetAppMenuButton()->Activate(nullptr);
}

content::KeyboardEventProcessingResult BrowserView::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if ((event.GetType() != blink::WebInputEvent::kRawKeyDown) &&
      (event.GetType() != blink::WebInputEvent::kKeyUp)) {
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  if (focus_manager->shortcut_handling_suspended())
    return content::KeyboardEventProcessingResult::NOT_HANDLED;

  ui::Accelerator accelerator =
      ui::GetAcceleratorFromNativeWebKeyboardEvent(event);

  // What we have to do here is as follows:
  // - If the |browser_| is for an app, do nothing.
  // - On CrOS if |accelerator| is deprecated, we allow web contents to consume
  //   it if needed.
  // - If the |browser_| is not for an app, and the |accelerator| is not
  //   associated with the browser (e.g. an Ash shortcut), process it.
  // - If the |browser_| is not for an app, and the |accelerator| is associated
  //   with the browser, and it is a reserved one (e.g. Ctrl+w), process it.
  // - If the |browser_| is not for an app, and the |accelerator| is associated
  //   with the browser, and it is not a reserved one, do nothing.

  if (browser_->is_app()) {
    // Let all keys fall through to a v1 app's web content, even accelerators.
    // We don't use NOT_HANDLED_IS_SHORTCUT here. If we do that, the app
    // might not be able to see a subsequent Char event. See OnHandleInputEvent
    // in content/renderer/render_widget.cc for details.
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

#if defined(OS_CHROMEOS)
  if (ash_util::IsAcceleratorDeprecated(accelerator)) {
    return (event.GetType() == blink::WebInputEvent::kRawKeyDown)
               ? content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT
               : content::KeyboardEventProcessingResult::NOT_HANDLED;
  }
#endif  // defined(OS_CHROMEOS)

  content::KeyboardEventProcessingResult result =
      frame_->PreHandleKeyboardEvent(event);
  if (result != content::KeyboardEventProcessingResult::NOT_HANDLED)
    return result;

#if defined(OS_CHROMEOS)
  if (event.os_event && event.os_event->IsKeyEvent() &&
      ash_util::WillAshProcessAcceleratorForEvent(
          *event.os_event->AsKeyEvent())) {
    return content::KeyboardEventProcessingResult::HANDLED_DONT_UPDATE_EVENT;
  }
#endif

  int id;
  if (!FindCommandIdForAccelerator(accelerator, &id)) {
    // |accelerator| is not a browser command, it may be handled by ash (e.g.
    // F4-F10). Report if we handled it.
    if (focus_manager->ProcessAccelerator(accelerator))
      return content::KeyboardEventProcessingResult::HANDLED;
    // Otherwise, it's not an accelerator.
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  // If it's a known browser command, we decide whether to consume it now, i.e.
  // reserved by browser.
  chrome::BrowserCommandController* controller = browser_->command_controller();
  // Executing the command may cause |this| object to be destroyed.
  if (controller->IsReservedCommandOrKey(id, event)) {
    UpdateAcceleratorMetrics(accelerator, id);
    return focus_manager->ProcessAccelerator(accelerator)
               ? content::KeyboardEventProcessingResult::HANDLED
               : content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  // BrowserView does not register RELEASED accelerators. So if we can find the
  // command id from |accelerator_table_|, it must be a keydown event. This
  // DCHECK ensures we won't accidentally return NOT_HANDLED for a later added
  // RELEASED accelerator in BrowserView.
  DCHECK_EQ(event.GetType(), blink::WebInputEvent::kRawKeyDown);
  // |accelerator| is a non-reserved browser shortcut (e.g. Ctrl+f).
  return content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
}

bool BrowserView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  if (frame_->HandleKeyboardEvent(event))
    return true;

  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

// TODO(devint): http://b/issue?id=1117225 Cut, Copy, and Paste are always
// enabled in the page menu regardless of whether the command will do
// anything. When someone selects the menu item, we just act as if they hit
// the keyboard shortcut for the command by sending the associated key press
// to windows. The real fix to this bug is to disable the commands when they
// won't do anything. We'll need something like an overall clipboard command
// manager to do that.
void BrowserView::CutCopyPaste(int command_id) {
#if defined(OS_MACOSX)
  ForwardCutCopyPasteToNSApp(command_id);
#else
  // If a WebContents is focused, call its member method.
  //
  // We could make WebContents register accelerators and then just use the
  // plumbing for accelerators below to dispatch these, but it's not clear
  // whether that would still allow keypresses of ctrl-X/C/V to be sent as
  // key events (and not accelerators) to the WebContents so it can give the web
  // page a chance to override them.
  WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (contents) {
    void (WebContents::*method)();
    if (command_id == IDC_CUT)
      method = &content::WebContents::Cut;
    else if (command_id == IDC_COPY)
      method = &content::WebContents::Copy;
    else
      method = &content::WebContents::Paste;
    if (DoCutCopyPasteForWebContents(contents, method))
      return;

    WebContents* devtools =
        DevToolsWindow::GetInTabWebContents(contents, nullptr);
    if (devtools && DoCutCopyPasteForWebContents(devtools, method))
      return;
  }

  // Any Views which want to handle the clipboard commands in the Chrome menu
  // should:
  //   (a) Register ctrl-X/C/V as accelerators
  //   (b) Implement CanHandleAccelerators() to not return true unless they're
  //       focused, as the FocusManager will try all registered accelerator
  //       handlers, not just the focused one.
  // Currently, Textfield (which covers the omnibox and find bar, and likely any
  // other native UI in the future that wants to deal with clipboard commands)
  // does the above.
  ui::Accelerator accelerator;
  GetAccelerator(command_id, &accelerator);
  GetFocusManager()->ProcessAccelerator(accelerator);
#endif  // defined(OS_MACOSX)
}

FindBar* BrowserView::CreateFindBar() {
  return new FindBarHost(this);
}

WebContentsModalDialogHost* BrowserView::GetWebContentsModalDialogHost() {
  return GetBrowserViewLayout()->GetWebContentsModalDialogHost();
}

BookmarkBarView* BrowserView::GetBookmarkBarView() const {
  return bookmark_bar_view_.get();
}

LocationBarView* BrowserView::GetLocationBarView() const {
  return toolbar_ ? toolbar_->location_bar() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, TabStripModelObserver implementation:

void BrowserView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted)
    return;

  for (size_t i = 0; i < change.deltas().size(); i++) {
#if defined(USE_AURA)
    // WebContents inserted in tabs might not have been added to the root
    // window yet. Per http://crbug/342672 add them now since drawing the
    // WebContents requires root window specific data - information about
    // the screen the WebContents is drawn on, for example.
    const auto& delta = change.deltas()[i];
    content::WebContents* contents = delta.insert.contents;
    if (!contents->GetNativeView()->GetRootWindow()) {
      aura::Window* window = contents->GetNativeView();
      aura::Window* root_window = GetNativeWindow()->GetRootWindow();
      aura::client::ParentWindowWithContext(window, root_window,
                                            root_window->GetBoundsInScreen());
      DCHECK(contents->GetNativeView()->GetRootWindow());
    }
#endif
    web_contents_close_handler_->TabInserted();
  }
}

void BrowserView::TabStripEmpty() {
  // Make sure all optional UI is removed before we are destroyed, otherwise
  // there will be consequences (since our view hierarchy will still have
  // references to freed views).
  UpdateUIForContents(nullptr);
}

void BrowserView::WillCloseAllTabs(TabStripModel* tab_strip_model) {
  web_contents_close_handler_->WillCloseAllTabs();
}

void BrowserView::CloseAllTabsStopped(TabStripModel* tab_strip_model,
                                      CloseAllStoppedReason reason) {
  if (reason == kCloseAllCanceled)
    web_contents_close_handler_->CloseAllTabsCanceled();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ui::AcceleratorProvider implementation:

bool BrowserView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  // Let's let the ToolbarView own the canonical implementation of this method.
  return toolbar_->GetAcceleratorForCommandId(command_id, accelerator);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::WidgetDelegate implementation:

bool BrowserView::CanResize() const {
  return true;
}

bool BrowserView::CanMaximize() const {
  return true;
}

bool BrowserView::CanMinimize() const {
  return true;
}

bool BrowserView::CanActivate() const {
  app_modal::AppModalDialogQueue* queue =
      app_modal::AppModalDialogQueue::GetInstance();
  if (!queue->active_dialog() || !queue->active_dialog()->native_dialog() ||
      !queue->active_dialog()->native_dialog()->IsShowing()) {
    return true;
  }

#if defined(USE_AURA) && defined(OS_CHROMEOS)
  // On Aura window manager controls all windows so settings focus via PostTask
  // will make only worse because posted task will keep trying to steal focus.
  queue->ActivateModalDialog();
#else
  // If another browser is app modal, flash and activate the modal browser. This
  // has to be done in a post task, otherwise if the user clicked on a window
  // that doesn't have the modal dialog the windows keep trying to get the focus
  // from each other on Windows. http://crbug.com/141650.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserView::ActivateAppModalDialog,
                                activate_modal_dialog_factory_.GetWeakPtr()));
#endif
  return false;
}

base::string16 BrowserView::GetWindowTitle() const {
  base::string16 title =
      browser_->GetWindowTitleForCurrentTab(true /* include_app_name */);
#if defined(OS_MACOSX)
  content::WebContents* contents = GetActiveWebContents();
  if (contents) {
    auto* helper = RecentlyAudibleHelper::FromWebContents(contents);
    if (helper && helper->WasRecentlyAudible()) {
      title =
          contents->IsAudioMuted()
              ? l10n_util::GetStringFUTF16(IDS_WINDOW_AUDIO_MUTING_MAC, title,
                                           base::WideToUTF16(L"\U0001F507"))
              : title = l10n_util::GetStringFUTF16(
                    IDS_WINDOW_AUDIO_PLAYING_MAC, title,
                    base::WideToUTF16(L"\U0001F50A"));
    }
  }
#endif
  return title;
}

base::string16 BrowserView::GetAccessibleWindowTitle() const {
  // If there is a focused and visible tab-modal dialog, report the dialog's
  // title instead of the page title.
  views::Widget* tab_modal =
      views::ViewAccessibilityUtils::GetFocusedChildWidgetForAccessibility(
          this);
  if (tab_modal)
    return tab_modal->widget_delegate()->GetAccessibleWindowTitle();

  return GetAccessibleWindowTitleForChannelAndProfile(chrome::GetChannel(),
                                                      browser_->profile());
}

base::string16 BrowserView::GetAccessibleWindowTitleForChannelAndProfile(
    version_info::Channel channel,
    Profile* profile) const {
  // Start with the tab title, which includes properties of the tab
  // like playing audio or network error.
  const bool include_app_name = false;
  int active_index = browser_->tab_strip_model()->active_index();
  base::string16 title;
  if (active_index > -1)
    title = GetAccessibleTabLabel(include_app_name, active_index);
  else
    title = browser_->GetWindowTitleForCurrentTab(include_app_name);

  // Add the name of the browser, unless this is an app window.
  if (!browser()->is_app()) {
    int message_id;
    switch (channel) {
      case version_info::Channel::CANARY:
        message_id = IDS_ACCESSIBLE_CANARY_BROWSER_WINDOW_TITLE_FORMAT;
        break;
      case version_info::Channel::DEV:
        message_id = IDS_ACCESSIBLE_DEV_BROWSER_WINDOW_TITLE_FORMAT;
        break;
      case version_info::Channel::BETA:
        message_id = IDS_ACCESSIBLE_BETA_BROWSER_WINDOW_TITLE_FORMAT;
        break;
      default:
        // Stable or unknown.
        message_id = IDS_ACCESSIBLE_BROWSER_WINDOW_TITLE_FORMAT;
        break;
    }
    title = l10n_util::GetStringFUTF16(message_id, title);
  }

  // Finally annotate with the user - add Incognito if it's an incognito
  // window, otherwise use the avatar name.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile->IsOffTheRecord()) {
    title = l10n_util::GetStringFUTF16(
        IDS_ACCESSIBLE_INCOGNITO_WINDOW_TITLE_FORMAT, title);
  } else if (profile->GetProfileType() == Profile::REGULAR_PROFILE &&
             profile_manager->GetNumberOfProfiles() > 1) {
    base::string16 profile_name =
        profiles::GetAvatarNameForProfile(profile->GetPath());
    if (!profile_name.empty()) {
      title = l10n_util::GetStringFUTF16(
          IDS_ACCESSIBLE_WINDOW_TITLE_WITH_PROFILE_FORMAT, title, profile_name);
    }
  }

  return title;
}

base::string16 BrowserView::GetAccessibleTabLabel(bool include_app_name,
                                                  int index) const {
  // ChromeVox provides an invalid index on browser start up before
  // any tabs are created.
  if (index == -1)
    return base::string16();

  base::string16 title =
      browser_->GetWindowTitleForTab(include_app_name, index);

  // Tab has crashed.
  if (tabstrip_->IsTabCrashed(index))
    return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_CRASHED_FORMAT, title);

  // Network error interstitial.
  if (tabstrip_->TabHasNetworkError(index)) {
    return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_NETWORK_ERROR_FORMAT,
                                      title);
  }

  // Alert tab states.
  switch (tabstrip_->GetTabAlertState(index)) {
    case TabAlertState::AUDIO_PLAYING:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_AUDIO_PLAYING_FORMAT,
                                        title);
    case TabAlertState::USB_CONNECTED:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_USB_CONNECTED_FORMAT,
                                        title);
    case TabAlertState::BLUETOOTH_CONNECTED:
      return l10n_util::GetStringFUTF16(
          IDS_TAB_AX_LABEL_BLUETOOTH_CONNECTED_FORMAT, title);
    case TabAlertState::MEDIA_RECORDING:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_MEDIA_RECORDING_FORMAT,
                                        title);
    case TabAlertState::AUDIO_MUTING:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_AUDIO_MUTING_FORMAT,
                                        title);
    case TabAlertState::TAB_CAPTURING:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_TAB_CAPTURING_FORMAT,
                                        title);
    case TabAlertState::PIP_PLAYING:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_PIP_PLAYING_FORMAT,
                                        title);

    case TabAlertState::DESKTOP_CAPTURING:
      return l10n_util::GetStringFUTF16(
          IDS_TAB_AX_LABEL_DESKTOP_CAPTURING_FORMAT, title);
    case TabAlertState::NONE:
      return title;
  }

  NOTREACHED();
  return base::string16();
}

void BrowserView::NativeThemeUpdated(const ui::NativeTheme* theme) {
  // We don't handle theme updates in OnThemeChanged() as that function is
  // called while views are being iterated over. Please see
  // View::PropagateNativeThemeChanged() for details. The theme update
  // handling in UserChangedTheme() can cause views to be nuked or created
  // which is a bad thing during iteration.

  // Do not handle native theme changes before the browser view is initialized.
  if (!initialized_)
    return;
  // Don't infinitely recurse.
  if (!handling_theme_changed_)
    UserChangedTheme();
  MaybeShowInvertBubbleView(this);
}

int BrowserView::GetBookmarkBarContentVerticalOffset() const {
  if (!bookmark_bar_view_.get()) {
    return 0;
  }

  // If the info bar is visible and the bookmark bar is detached, the info bar
  // will be rendered above the bookmark bar. When this is the case, the shadow
  // of the info bar will overlap onto the bookmark bar. The icons on the
  // bookmark bar should be moved down so the icons appeared centered on the non
  // shaded parts of the bookmark bar.
  if (IsInfoBarVisible() && bookmark_bar_view_->IsDetached()) {
    // 2 is roughly the number of visible pixels of the infobar shadow over the
    // bookmark bar, so if we shift the top of the bookmark buttons down by this
    // value, it will appear centered.
    return 2;
  }

  // If the bookmark bar is attached, there will appear to be extra space above
  // the bookmark bar icons due to the space below the location bar on the
  // toolbar. The bookmark bar icons need to be moved up to compensate.
  if (!bookmark_bar_view_->IsDetached()) {
    return -GetBottomInsetOfLocationBarWithinToolbar() / 2;
  }
  return 0;
}

int BrowserView::GetBottomInsetOfLocationBarWithinToolbar() const {
  return (toolbar_->height() - GetLocationBarView()->height() -
          bookmark_bar_view_->GetToolbarOverlap()) /
         2;
}

void BrowserView::ReparentTopContainerForEndOfImmersive() {
  overlay_view_->SetVisible(false);
  top_container()->DestroyLayer();
  AddChildViewAt(top_container(), GetIndexOf(contents_container_));
}

views::View* BrowserView::GetInitiallyFocusedView() {
  return nullptr;
}

bool BrowserView::ShouldShowWindowTitle() const {
#if defined(OS_CHROMEOS)
  // For Chrome OS only, trusted windows (apps and settings) do not show a
  // title, crbug.com/119411. Child windows (i.e. popups) do show a title.
  if (browser_->is_trusted_source()) {
    return false;
  }
#endif  // OS_CHROMEOS

  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

gfx::ImageSkia BrowserView::GetWindowAppIcon() {
  extensions::HostedAppBrowserController* app_controller =
      browser()->hosted_app_controller();
  return app_controller ? app_controller->GetWindowAppIcon() : GetWindowIcon();
}

gfx::ImageSkia BrowserView::GetWindowIcon() {
  // Use the default icon for devtools.
  if (browser_->is_devtools())
    return gfx::ImageSkia();

  // Hosted apps always show their app icon.
  extensions::HostedAppBrowserController* app_controller =
      browser()->hosted_app_controller();
  if (app_controller)
    return app_controller->GetWindowIcon();

#if defined(OS_CHROMEOS)
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (browser_->is_type_tabbed()) {
    return rb.GetImageNamed(IDR_CHROME_APP_ICON_192).AsImageSkia();
  }
  auto* window = GetNativeWindow();
  int override_window_icon_resource_id =
      window ? window->GetProperty(kOverrideWindowIconResourceIdKey) : -1;
  if (override_window_icon_resource_id >= 0)
    return rb.GetImageNamed(override_window_icon_resource_id).AsImageSkia();
#endif

  if (browser_->is_app() || browser_->is_type_popup())
    return browser_->GetCurrentPageIcon().AsImageSkia();

  return gfx::ImageSkia();
}

bool BrowserView::ShouldShowWindowIcon() const {
  // Currently the icon and title are always shown together.
  return ShouldShowWindowTitle();
}

bool BrowserView::ExecuteWindowsCommand(int command_id) {
  // This function handles WM_SYSCOMMAND, WM_APPCOMMAND, and WM_COMMAND.
#if defined(OS_WIN)
  if (command_id == IDC_DEBUG_FRAME_TOGGLE)
    GetWidget()->DebugToggleFrameType();
#endif
  // Translate WM_APPCOMMAND command ids into a command id that the browser
  // knows how to handle.
  int command_id_from_app_command = GetCommandIDForAppCommandID(command_id);
  if (command_id_from_app_command != -1)
    command_id = command_id_from_app_command;

  return chrome::ExecuteCommand(browser_.get(), command_id);
}

std::string BrowserView::GetWindowName() const {
  return chrome::GetWindowName(browser_.get());
}

void BrowserView::SaveWindowPlacement(const gfx::Rect& bounds,
                                      ui::WindowShowState show_state) {
  // If IsFullscreen() is true, we've just changed into fullscreen mode, and
  // we're catching the going-into-fullscreen sizing and positioning calls,
  // which we want to ignore.
  if (!IsFullscreen() && frame_->ShouldSaveWindowPlacement() &&
      chrome::ShouldSaveWindowPlacement(browser_.get())) {
    WidgetDelegate::SaveWindowPlacement(bounds, show_state);
    chrome::SaveWindowPlacement(browser_.get(), bounds, show_state);
  }
}

bool BrowserView::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  chrome::GetSavedWindowBoundsAndShowState(browser_.get(), bounds, show_state);

  if (chrome::SavedBoundsAreContentBounds(browser_.get())) {
    // This is normal non-app popup window. The value passed in |bounds|
    // represents two pieces of information:
    // - the position of the window, in screen coordinates (outer position).
    // - the size of the content area (inner size).
    // We need to use these values to determine the appropriate size and
    // position of the resulting window.
    if (IsToolbarVisible()) {
      // If we're showing the toolbar, we need to adjust |*bounds| to include
      // its desired height, since the toolbar is considered part of the
      // window's client area as far as GetWindowBoundsForClientBounds is
      // concerned...
      bounds->set_height(
          bounds->height() + toolbar_->GetPreferredSize().height());
    }

    gfx::Rect window_rect = frame_->non_client_view()->
        GetWindowBoundsForClientBounds(*bounds);
    window_rect.set_origin(bounds->origin());

    // When we are given x/y coordinates of 0 on a created popup window,
    // assume none were given by the window.open() command.
    if (window_rect.x() == 0 && window_rect.y() == 0) {
      gfx::Size size = window_rect.size();
      window_rect.set_origin(WindowSizer::GetDefaultPopupOrigin(size));
    }

    *bounds = window_rect;
    *show_state = ui::SHOW_STATE_NORMAL;
  }

  // We return true because we can _always_ locate reasonable bounds using the
  // WindowSizer, and we don't want to trigger the Window's built-in "size to
  // default" handling because the browser window has no default preferred
  // size.
  return true;
}

views::View* BrowserView::GetContentsView() {
  return contents_web_view_;
}

views::ClientView* BrowserView::CreateClientView(views::Widget* widget) {
  return this;
}

views::View* BrowserView::CreateOverlayView() {
  overlay_view_ = new views::View();
  overlay_view_->SetVisible(false);
  overlay_view_targeter_ = std::make_unique<OverlayViewTargeterDelegate>();
  overlay_view_->SetEventTargeter(
      std::make_unique<views::ViewTargeter>(overlay_view_targeter_.get()));
  return overlay_view_;
}

void BrowserView::OnWidgetDestroying(views::Widget* widget) {
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  if (added_quit_instructions_) {
    GetWidget()->GetNativeView()->RemovePreTargetHandler(
        QuitInstructionBubbleController::GetInstance());
  }
#endif

  // Destroy any remaining WebContents early on. Doing so may result in
  // calling back to one of the Views/LayoutManagers or supporting classes of
  // BrowserView. By destroying here we ensure all said classes are valid.
  std::vector<std::unique_ptr<content::WebContents>> contents;
  while (browser()->tab_strip_model()->count())
    contents.push_back(browser()->tab_strip_model()->DetachWebContentsAt(0));
  // Note: The BrowserViewTest tests rely on the contents being destroyed in the
  // order that they were present in the tab strip.
  for (auto& content : contents)
    content.reset();
  // Destroy the fullscreen control host, as it observes the native window.
  fullscreen_control_host_.reset();
}

void BrowserView::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  if (browser_->window()) {
    if (active)
      BrowserList::SetLastActive(browser_.get());
    else
      BrowserList::NotifyBrowserNoLongerActive(browser_.get());
  }

  if (!extension_keybinding_registry_ &&
      GetFocusManager()) {  // focus manager can be null in tests.
    extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryViews(
        browser_->profile(), GetFocusManager(),
        extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS, this));
  }

  extensions::ExtensionCommandsGlobalRegistry* registry =
      extensions::ExtensionCommandsGlobalRegistry::Get(browser_->profile());
  if (active) {
    registry->set_registry_for_active_window(
        extension_keybinding_registry_.get());
  } else if (registry->registry_for_active_window() ==
             extension_keybinding_registry_.get()) {
    registry->set_registry_for_active_window(nullptr);
  }

  immersive_mode_controller()->OnWidgetActivationChanged(widget, active);
}

void BrowserView::OnWindowBeginUserBoundsChange() {
  if (interactive_resize_)
    return;
  WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return;
  interactive_resize_ = ResizeSession();
  interactive_resize_->begin_timestamp = base::TimeTicks::Now();
  web_contents->GetRenderViewHost()->NotifyMoveOrResizeStarted();
}

void BrowserView::OnWindowEndUserBoundsChange() {
  if (!interactive_resize_)
    return;
  safe_browsing::LogContentsSize(GetContentsSize());
  auto now = base::TimeTicks::Now();
  DCHECK(!interactive_resize_->begin_timestamp.is_null());
  UMA_HISTOGRAM_TIMES("BrowserWindow.Resize.Duration",
                      now - interactive_resize_->begin_timestamp);
  UMA_HISTOGRAM_COUNTS_1000("BrowserWindow.Resize.StepCount",
                            interactive_resize_->step_count);
  interactive_resize_.reset();
}

void BrowserView::OnWidgetMove() {
  if (!initialized_) {
    // Creating the widget can trigger a move. Ignore it until we've initialized
    // things.
    return;
  }

  // Cancel any tabstrip animations, some of them may be invalidated by the
  // window being repositioned.
  // Comment out for one cycle to see if this fixes dist tests.
  // tabstrip_->DestroyDragController();

  // status_bubble_ may be null if this is invoked during construction.
  if (status_bubble_.get())
    status_bubble_->Reposition();

  BookmarkBubbleView::Hide();

  // Close the omnibox popup, if any.
  LocationBarView* location_bar_view = GetLocationBarView();
  if (location_bar_view)
    location_bar_view->GetOmniboxView()->CloseOmniboxPopup();
}

views::Widget* BrowserView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* BrowserView::GetWidget() const {
  return View::GetWidget();
}

void BrowserView::RevealTabStripIfNeeded() {
  if (!immersive_mode_controller_->IsEnabled())
    return;

  std::unique_ptr<ImmersiveRevealedLock> revealer(
      immersive_mode_controller_->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES));
  auto delete_revealer = base::BindOnce(
      [](std::unique_ptr<ImmersiveRevealedLock>) {}, std::move(revealer));
  constexpr auto kDefaultDelay = base::TimeDelta::FromSeconds(1);
  constexpr auto kZeroDelay = base::TimeDelta::FromSeconds(0);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(delete_revealer),
      g_disable_revealer_delay_for_testing ? kZeroDelay : kDefaultDelay);
}

void BrowserView::GetAccessiblePanes(std::vector<views::View*>* panes) {
  // This should be in the order of pane traversal of the panes using F6
  // (Windows) or Ctrl+Back/Forward (Chrome OS).  If one of these is
  // invisible or has no focusable children, it will be automatically
  // skipped.
  if (tabstrip_)
    panes->push_back(tabstrip_);
  panes->push_back(toolbar_button_provider_->GetAsAccessiblePaneView());
  if (bookmark_bar_view_.get())
    panes->push_back(bookmark_bar_view_.get());
  if (infobar_container_)
    panes->push_back(infobar_container_);
  if (download_shelf_.get())
    panes->push_back(download_shelf_.get());
  panes->push_back(contents_web_view_);
  if (devtools_web_view_->visible())
    panes->push_back(devtools_web_view_);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::ClientView overrides:

bool BrowserView::CanClose() {
  // You cannot close a frame for which there is an active originating drag
  // session.
  if (tabstrip_ && !tabstrip_->IsTabStripCloseable())
    return false;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return false;

  bool fast_tab_closing_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFastUnload);

  if (!browser_->tab_strip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    frame_->Hide();
    browser_->OnWindowClosing();
    if (fast_tab_closing_enabled)
      browser_->tab_strip_model()->CloseAllTabs();
    return false;
  } else if (fast_tab_closing_enabled &&
        !browser_->HasCompletedUnloadProcessing()) {
    // The browser needs to finish running unload handlers.
    // Hide the frame (so it appears to have closed immediately), and
    // the browser will call us back again when it is ready to close.
    frame_->Hide();
    return false;
  }

  return true;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
  return GetBrowserViewLayout()->NonClientHitTest(point);
}

gfx::Size BrowserView::GetMinimumSize() const {
  return GetBrowserViewLayout()->GetMinimumSize();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::View overrides:

const char* BrowserView::GetClassName() const {
  return kViewClassName;
}

void BrowserView::Layout() {
  if (!initialized_ || in_process_fullscreen_)
    return;

  views::View::Layout();

  // TODO(jamescook): Why was this in the middle of layout code?
  toolbar_->location_bar()->omnibox_view()->SetFocusBehavior(
      IsToolbarVisible() ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
  frame()->GetFrameView()->UpdateMinimumSize();

  // Some of the situations when the BrowserView is laid out are:
  // - Enter/exit immersive fullscreen mode.
  // - Enter/exit tablet mode.
  // - At the beginning/end of the top controls slide behavior in tablet mode.
  // The above may result in a change in the location bar's position, to which a
  // permission bubble may be anchored. For that we must update its anchor
  // position.
  WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (contents && PermissionRequestManager::FromWebContents(contents))
    PermissionRequestManager::FromWebContents(contents)->UpdateAnchorPosition();
}

void BrowserView::OnGestureEvent(ui::GestureEvent* event) {
  int command;
  if (GetGestureCommand(event, &command) &&
      chrome::IsCommandEnabled(browser(), command)) {
    chrome::ExecuteCommandWithDisposition(
        browser(), command, ui::DispositionFromEventFlags(event->flags()));
    return;
  }

  ClientView::OnGestureEvent(event);
}

void BrowserView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.child != this)
    return;

  // On removal, this class may not have a widget anymore, so go to the parent.
  auto* widget = details.is_add ? GetWidget() : details.parent->GetWidget();
  if (!widget)
    return;

  bool init = !initialized_ && details.is_add;
  if (init) {
    InitViews();
    initialized_ = true;
  }
}

void BrowserView::PaintChildren(const views::PaintInfo& paint_info) {
  views::ClientView::PaintChildren(paint_info);
  // Don't reset the instance before it had a chance to get compositor callback.
  if (!histogram_helper_) {
    histogram_helper_ = BrowserWindowHistogramHelper::
        MaybeRecordValueAndCreateInstanceOnBrowserPaint(
            GetWidget()->GetCompositor());
  }
}

void BrowserView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (!interactive_resize_)
    return;
  auto now = base::TimeTicks::Now();
  if (!interactive_resize_->last_resize_timestamp.is_null()) {
    const auto& current_size = size();
    // If size doesn't change, then do not update the timestamp.
    if (current_size == previous_bounds.size())
      return;
    UMA_HISTOGRAM_CUSTOM_TIMES("BrowserWindow.Resize.StepInterval",
                               now - interactive_resize_->last_resize_timestamp,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromSeconds(1), 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "BrowserWindow.Resize.StepBoundsChange.Width",
        std::abs(previous_bounds.size().width() - current_size.width()),
        1 /* min */, 300 /* max */, 100 /* buckets */);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "BrowserWindow.Resize.StepBoundsChange.Height",
        std::abs(previous_bounds.size().height() - current_size.height()),
        1 /* min */, 300 /* max */, 100 /* buckets */);
  }
  ++interactive_resize_->step_count;
  interactive_resize_->last_resize_timestamp = now;
}

void BrowserView::ChildPreferredSizeChanged(View* child) {
  Layout();
}

void BrowserView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kClient;
}

void BrowserView::OnThemeChanged() {
  if (!IsRegularOrGuestSession()) {
    // When the theme changes, the native theme may also change (in incognito,
    // the usage of dark or normal hinges on the browser theme), so we have to
    // propagate both kinds of change.
    base::AutoReset<bool> reset(&handling_theme_changed_, true);
#if defined(USE_AURA)
    ui::NativeThemeDarkAura::instance()->NotifyObservers();
#endif
    ui::NativeTheme::GetInstanceForNativeUi()->NotifyObservers();
  }

  views::View::OnThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ui::AcceleratorTarget overrides:

bool BrowserView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  int command_id;
  // Though AcceleratorManager should not send unknown |accelerator| to us, it's
  // still possible the command cannot be executed now.
  if (!FindCommandIdForAccelerator(accelerator, &command_id))
    return false;

  UpdateAcceleratorMetrics(accelerator, command_id);
  return chrome::ExecuteCommand(browser_.get(), command_id);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, infobars::InfoBarContainer::Delegate overrides:

void BrowserView::InfoBarContainerStateChanged(bool is_animating) {
  ToolbarSizeChanged(is_animating);
}

void BrowserView::InitViews() {
  // TopControlsSlideController must be initialized here in InitViews() rather
  // than Init() as it depends on the browser frame being ready.
  if (IsBrowserTypeNormal()) {
    DCHECK(frame_);
    top_controls_slide_controller_ = CreateTopControlsSlideController(this);
  }

  GetWidget()->AddObserver(this);

  // Stow a pointer to this object onto the window handle so that we can get at
  // it later when all we have is a native view.
  GetWidget()->SetNativeWindowProperty(kBrowserViewKey, this);

  // Stow a pointer to the browser's profile onto the window handle so that we
  // can get it later when all we have is a native view.
  GetWidget()->SetNativeWindowProperty(Profile::kProfileKey,
                                       browser_->profile());

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  if (browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR)) {
    added_quit_instructions_ = true;
    GetWidget()->GetNativeView()->AddPreTargetHandler(
        QuitInstructionBubbleController::GetInstance());
  }
#endif

#if defined(USE_AURA)
  // Stow a pointer to the browser's profile onto the window handle so
  // that windows will be styled with the appropriate NativeTheme.
  SetThemeProfileForWindow(GetNativeWindow(), browser_->profile());
#endif

  LoadAccelerators();

  // Top container holds tab strip and toolbar and lives at the front of the
  // view hierarchy.
  top_container_ = new TopContainerView(this);
  AddChildView(top_container_);

  // TabStrip takes ownership of the controller.
  BrowserTabStripController* tabstrip_controller =
      new BrowserTabStripController(browser_->tab_strip_model(), this);
  tabstrip_ =
      new TabStrip(std::unique_ptr<TabStripController>(tabstrip_controller));
  top_container_->AddChildView(tabstrip_);  // Takes ownership.
  tabstrip_controller->InitFromModel(tabstrip_);

  toolbar_ = new ToolbarView(browser_.get(), this);
  top_container_->AddChildView(toolbar_);
  toolbar_->Init();

  // This browser view may already have a custom button provider set (e.g the
  // hosted app frame).
  if (!toolbar_button_provider_)
    SetToolbarButtonProvider(toolbar_);

  contents_web_view_ = new ContentsWebView(browser_->profile());
  contents_web_view_->set_id(VIEW_ID_TAB_CONTAINER);
  contents_web_view_->SetEmbedFullscreenWidgetMode(true);

  web_contents_close_handler_.reset(
      new WebContentsCloseHandler(contents_web_view_));

  devtools_web_view_ = new views::WebView(browser_->profile());
  devtools_web_view_->set_id(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_web_view_->SetVisible(false);

  contents_container_ = new views::View();
  contents_container_->SetBackground(views::CreateSolidBackground(
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_CONTROL_BACKGROUND)));
  contents_container_->AddChildView(devtools_web_view_);
  contents_container_->AddChildView(contents_web_view_);
  contents_container_->SetLayoutManager(std::make_unique<ContentsLayoutManager>(
      devtools_web_view_, contents_web_view_));
  AddChildView(contents_container_);
  set_contents_view(contents_container_);

  infobar_container_ = new InfoBarContainerView(this);
  AddChildView(infobar_container_);

  InitStatusBubble();

  // Create do-nothing view for the sake of controlling the z-order of the find
  // bar widget.
  find_bar_host_view_ = new View();
  AddChildView(find_bar_host_view_);

  immersive_mode_controller_->Init(this);
  immersive_mode_controller_->AddObserver(this);

  auto browser_view_layout = std::make_unique<BrowserViewLayout>();
  browser_view_layout->Init(new BrowserViewLayoutDelegateImpl(this),
                            browser(),
                            this,
                            top_container_,
                            tabstrip_,
                            toolbar_,
                            infobar_container_,
                            contents_container_,
                            GetContentsLayoutManager(),
                            immersive_mode_controller_.get());
  SetLayoutManager(std::move(browser_view_layout));

#if defined(OS_WIN)
  // Create a custom JumpList and add it to an observer of TabRestoreService
  // so we can update the custom JumpList when a tab is added or removed.
  if (JumpList::Enabled()) {
    load_complete_listener_.reset(new LoadCompleteListener(this));
  }
#endif

  frame_->OnBrowserViewInitViewsComplete();
  frame_->GetFrameView()->UpdateMinimumSize();
}

void BrowserView::LoadingAnimationCallback() {
  if (browser_->is_type_tabbed()) {
    // Loading animations are shown in the tab for tabbed windows.  We check the
    // browser type instead of calling IsTabStripVisible() because the latter
    // will return false for fullscreen windows, but we still need to update
    // their animations (so that when they come out of fullscreen mode they'll
    // be correct).
    tabstrip_->UpdateLoadingAnimations();
  } else if (ShouldShowWindowIcon()) {
    // ... or in the window icon area for popups and app windows.
    WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    // GetActiveWebContents can return null for example under Purify when
    // the animations are running slowly and this function is called on a timer
    // through LoadingAnimationCallback.
    frame_->UpdateThrobber(web_contents && web_contents->IsLoading());
  }
}

void BrowserView::OnLoadCompleted() {
#if defined(OS_WIN)
  // Ensure that this browser's Profile has a JumpList so that the JumpList is
  // kept up to date.
  JumpListFactory::GetForProfile(browser_->profile());
#endif
}

BrowserViewLayout* BrowserView::GetBrowserViewLayout() const {
  return static_cast<BrowserViewLayout*>(GetLayoutManager());
}

ContentsLayoutManager* BrowserView::GetContentsLayoutManager() const {
  return static_cast<ContentsLayoutManager*>(
      contents_container_->GetLayoutManager());
}

bool BrowserView::MaybeShowBookmarkBar(WebContents* contents) {
  const bool show_bookmark_bar =
      contents && browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR);
  if (!show_bookmark_bar && !bookmark_bar_view_.get())
    return false;
  if (!bookmark_bar_view_.get()) {
    bookmark_bar_view_.reset(new BookmarkBarView(browser_.get(), this));
    bookmark_bar_view_->set_owned_by_client();
    bookmark_bar_view_->SetBackground(
        std::make_unique<BookmarkBarViewBackground>(this,
                                                    bookmark_bar_view_.get()));
    bookmark_bar_view_->SetBookmarkBarState(
        browser_->bookmark_bar_state(),
        BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
    GetBrowserViewLayout()->set_bookmark_bar(bookmark_bar_view_.get());
  }
  // Don't change the visibility of the BookmarkBarView. BrowserViewLayout
  // handles it.
  bookmark_bar_view_->SetPageNavigator(GetActiveWebContents());

  // Update parenting for the bookmark bar. This may detach it from all views.
  bool needs_layout = false;
  views::View* new_parent = nullptr;
  if (show_bookmark_bar) {
    if (bookmark_bar_view_->IsDetached())
      new_parent = this;
    else
      new_parent = top_container_;
  }
  if (new_parent != bookmark_bar_view_->parent()) {
    SetBookmarkBarParent(new_parent);
    needs_layout = true;
  }

  // Check for updates to the desired size.
  if (bookmark_bar_view_->GetPreferredSize().height() !=
      bookmark_bar_view_->height())
    needs_layout = true;

  return needs_layout;
}

void BrowserView::SetBookmarkBarParent(views::View* new_parent) {
  if (new_parent == this) {
    // BookmarkBarView is detached.
    views::View* target_view = infobar_container_;
    const int target_index = GetIndexOf(target_view);
    DCHECK_GE(target_index, 0);
    // Putting the bookmark bar ahead of the infobar container ensures infobar
    // shadows can draw atop it.
    AddChildViewAt(bookmark_bar_view_.get(), target_index);
  } else if (new_parent == top_container_) {
    // BookmarkBarView is attached.
    new_parent->AddChildView(bookmark_bar_view_.get());
  } else {
    DCHECK(!new_parent);
    // Bookmark bar is being detached from all views because it is hidden.
    bookmark_bar_view_->parent()->RemoveChildView(bookmark_bar_view_.get());
  }
}

bool BrowserView::MaybeShowInfoBar(WebContents* contents) {
  // TODO(beng): Remove this function once the interface between
  //             InfoBarContainer, DownloadShelfView and WebContents and this
  //             view is sorted out.
  return true;
}

void BrowserView::UpdateDevToolsForContents(
    WebContents* web_contents, bool update_devtools_web_contents) {
  DevToolsContentsResizingStrategy strategy;
  WebContents* devtools = DevToolsWindow::GetInTabWebContents(
      web_contents, &strategy);

  if (!devtools_web_view_->web_contents() && devtools &&
      !devtools_focus_tracker_.get()) {
    // Install devtools focus tracker when dev tools window is shown for the
    // first time.
    devtools_focus_tracker_.reset(
        new views::ExternalFocusTracker(devtools_web_view_,
                                        GetFocusManager()));
  }

  // Restore focus to the last focused view when hiding devtools window.
  if (devtools_web_view_->web_contents() && !devtools &&
      devtools_focus_tracker_.get()) {
    devtools_focus_tracker_->FocusLastFocusedExternalView();
    devtools_focus_tracker_.reset();
  }

  // Replace devtools WebContents.
  if (devtools_web_view_->web_contents() != devtools &&
      update_devtools_web_contents) {
    devtools_web_view_->SetWebContents(devtools);
  }

  if (devtools) {
    devtools_web_view_->SetVisible(true);
    GetContentsLayoutManager()->SetContentsResizingStrategy(strategy);
  } else {
    devtools_web_view_->SetVisible(false);
    GetContentsLayoutManager()->SetContentsResizingStrategy(
        DevToolsContentsResizingStrategy());
  }
  contents_container_->Layout();

  if (devtools) {
    // When strategy.hide_inspected_contents() returns true, we are hiding
    // contents_web_view_ behind the devtools_web_view_. Otherwise,
    // contents_web_view_ should be right above the devtools_web_view_.
    int devtools_index = contents_container_->GetIndexOf(devtools_web_view_);
    int contents_index = contents_container_->GetIndexOf(contents_web_view_);
    bool devtools_is_on_top = devtools_index > contents_index;
    if (strategy.hide_inspected_contents() != devtools_is_on_top)
      contents_container_->ReorderChildView(contents_web_view_, devtools_index);
  }
}

void BrowserView::UpdateUIForContents(WebContents* contents) {
  bool needs_layout = MaybeShowBookmarkBar(contents);
  // TODO(jamescook): This function always returns true. Remove it and figure
  // out when layout is actually required.
  needs_layout |= MaybeShowInfoBar(contents);
  if (needs_layout)
    Layout();
}

void BrowserView::ProcessFullscreen(bool fullscreen,
                                    const GURL& url,
                                    ExclusiveAccessBubbleType bubble_type) {
  if (in_process_fullscreen_)
    return;
  in_process_fullscreen_ = true;

  if (top_controls_slide_controller_)
    top_controls_slide_controller_->OnBrowserFullscreenStateWillChange(
        fullscreen);

  // Reduce jankiness during the following position changes by:
  //   * Hiding the window until it's in the final position
  //   * Ignoring all intervening Layout() calls, which resize the webpage and
  //     thus are slow and look ugly (enforced via |in_process_fullscreen_|).
  if (fullscreen) {
    // Move focus out of the location bar if necessary.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    // Look for focus in the location bar itself or any child view.
    if (GetLocationBarView()->Contains(focus_manager->GetFocusedView()))
      focus_manager->ClearFocus();

    fullscreen_control_host_ = std::make_unique<FullscreenControlHost>(this);
  } else {
    // Hide the fullscreen bubble as soon as possible, since the mode toggle can
    // take enough time for the user to notice.
    exclusive_access_bubble_.reset();

    if (fullscreen_control_host_) {
      fullscreen_control_host_->Hide(false);
      fullscreen_control_host_.reset();
    }
  }

  // Toggle fullscreen mode.
  frame_->SetFullscreen(fullscreen);

  // Enable immersive before the browser refreshes its list of enabled commands.
  const bool should_stay_in_immersive =
      !fullscreen &&
      immersive_mode_controller_->ShouldStayImmersiveAfterExitingFullscreen();
  bool is_locked_fullscreen = false;
#if defined(OS_CHROMEOS)
  is_locked_fullscreen = ash::IsWindowTrustedPinned(GetNativeWindow());
#endif
  // Never use immersive in locked fullscreen as it allows the user to exit the
  // locked mode.
  if (is_locked_fullscreen) {
    immersive_mode_controller_->SetEnabled(false);
  } else if (ShouldUseImmersiveFullscreenForUrl(url) &&
             !should_stay_in_immersive) {
    immersive_mode_controller_->SetEnabled(fullscreen);
  }

  browser_->WindowFullscreenStateWillChange();
  browser_->WindowFullscreenStateChanged();

  if (fullscreen && !chrome::IsRunningInAppMode()) {
    UpdateExclusiveAccessExitBubbleContent(url, bubble_type,
                                           ExclusiveAccessBubbleHideCallback(),
                                           /*force_update=*/false);
  }

  // Undo our anti-jankiness hacks and force a re-layout.
  in_process_fullscreen_ = false;
  ToolbarSizeChanged(false);
}

bool BrowserView::ShouldUseImmersiveFullscreenForUrl(const GURL& url) const {
#if defined(OS_CHROMEOS)
  // Kiosk mode needs the whole screen.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return false;

  // In Public Session, always use immersive fullscreen.
  if (profiles::IsPublicSession())
    return true;

  return url.is_empty();
#else
  // No immersive except in Chrome OS.
  return false;
#endif
}

void BrowserView::LoadAccelerators() {
  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  // Let's fill our own accelerator table.
  const bool is_app_mode = chrome::IsRunningInForcedAppMode();
  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (const auto& entry : accelerator_list) {
    // In app mode, only allow accelerators of white listed commands to pass
    // through.
    if (is_app_mode && !chrome::IsCommandAllowedInAppMode(entry.command_id))
      continue;

    ui::Accelerator accelerator(entry.keycode, entry.modifiers);
    accelerator_table_[accelerator] = entry.command_id;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::kNormalPriority, this);
  }
}

int BrowserView::GetCommandIDForAppCommandID(int app_command_id) const {
#if defined(OS_WIN)
  switch (app_command_id) {
    // NOTE: The order here matches the APPCOMMAND declaration order in the
    // Windows headers.
    case APPCOMMAND_BROWSER_BACKWARD: return IDC_BACK;
    case APPCOMMAND_BROWSER_FORWARD:  return IDC_FORWARD;
    case APPCOMMAND_BROWSER_REFRESH:  return IDC_RELOAD;
    case APPCOMMAND_BROWSER_HOME:     return IDC_HOME;
    case APPCOMMAND_BROWSER_STOP:     return IDC_STOP;
    case APPCOMMAND_BROWSER_SEARCH:   return IDC_FOCUS_SEARCH;
    case APPCOMMAND_HELP:             return IDC_HELP_PAGE_VIA_KEYBOARD;
    case APPCOMMAND_NEW:              return IDC_NEW_TAB;
    case APPCOMMAND_OPEN:             return IDC_OPEN_FILE;
    case APPCOMMAND_CLOSE:            return IDC_CLOSE_TAB;
    case APPCOMMAND_SAVE:             return IDC_SAVE_PAGE;
    case APPCOMMAND_PRINT:            return IDC_PRINT;
    case APPCOMMAND_COPY:             return IDC_COPY;
    case APPCOMMAND_CUT:              return IDC_CUT;
    case APPCOMMAND_PASTE:            return IDC_PASTE;

      // TODO(pkasting): http://b/1113069 Handle these.
    case APPCOMMAND_UNDO:
    case APPCOMMAND_REDO:
    case APPCOMMAND_SPELL_CHECK:
    default:                          return -1;
  }
#else
  // App commands are Windows-specific so there's nothing to do here.
  return -1;
#endif
}

void BrowserView::UpdateAcceleratorMetrics(const ui::Accelerator& accelerator,
                                           int command_id) {
  const ui::KeyboardCode key_code = accelerator.key_code();
  if (command_id == IDC_HELP_PAGE_VIA_KEYBOARD && key_code == ui::VKEY_F1)
    base::RecordAction(UserMetricsAction("ShowHelpTabViaF1"));

  if (command_id == IDC_BOOKMARK_PAGE)
    UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                              BOOKMARK_ENTRY_POINT_ACCELERATOR,
                              BOOKMARK_ENTRY_POINT_LIMIT);

#if defined(OS_CHROMEOS)
  // Collect information about the relative popularity of various accelerators
  // on Chrome OS.
  switch (command_id) {
    case IDC_BACK:
      if (key_code == ui::VKEY_BROWSER_BACK)
        base::RecordAction(UserMetricsAction("Accel_Back_F1"));
      else if (key_code == ui::VKEY_LEFT)
        base::RecordAction(UserMetricsAction("Accel_Back_Left"));
      break;
    case IDC_FORWARD:
      if (key_code == ui::VKEY_BROWSER_FORWARD)
        base::RecordAction(UserMetricsAction("Accel_Forward_F2"));
      else if (key_code == ui::VKEY_RIGHT)
        base::RecordAction(UserMetricsAction("Accel_Forward_Right"));
      break;
    case IDC_RELOAD:
    case IDC_RELOAD_BYPASSING_CACHE:
      if (key_code == ui::VKEY_R)
        base::RecordAction(UserMetricsAction("Accel_Reload_R"));
      else if (key_code == ui::VKEY_BROWSER_REFRESH)
        base::RecordAction(UserMetricsAction("Accel_Reload_F3"));
      break;
    case IDC_FOCUS_LOCATION:
      if (key_code == ui::VKEY_D)
        base::RecordAction(UserMetricsAction("Accel_FocusLocation_D"));
      else if (key_code == ui::VKEY_L)
        base::RecordAction(UserMetricsAction("Accel_FocusLocation_L"));
      break;
    case IDC_FOCUS_SEARCH:
      if (key_code == ui::VKEY_E)
        base::RecordAction(UserMetricsAction("Accel_FocusSearch_E"));
      else if (key_code == ui::VKEY_K)
        base::RecordAction(UserMetricsAction("Accel_FocusSearch_K"));
      break;
    default:
      // Do nothing.
      break;
  }
#endif
}

#if !defined(OS_ANDROID)
void BrowserView::ShowHatsBubbleFromAppMenuButton() {
  // Never show any HaTS bubble in Incognito.
  if (!IsRegularOrGuestSession())
    return;

  AppMenuButton* app_menu_button =
      toolbar_button_provider()->GetAppMenuButton();

  // Do not show Hatsbubble if there is no avatar menu button to anchor from
  if (!app_menu_button)
    return;

  DCHECK(app_menu_button->GetWidget());
  views::BubbleDialogDelegateView* bubble = HatsBubbleView::CreateHatsBubble(
      app_menu_button, browser(),
      app_menu_button->GetWidget()->GetNativeView());

  bubble->SetHighlightedButton(app_menu_button);
  bubble->GetWidget()->Show();
}
#endif

void BrowserView::ShowAvatarBubbleFromAvatarButton(
    AvatarBubbleMode mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point,
    bool focus_first_profile_button) {
#if !defined(OS_CHROMEOS)
  // Never show any avatar bubble in Incognito.
  if (!IsRegularOrGuestSession())
    return;

  // Do not show avatar bubble if there is no avatar menu button.
  views::Button* avatar_button = toolbar_->avatar_button();
  if (!avatar_button)
    return;

  profiles::BubbleViewMode bubble_view_mode;
  profiles::BubbleViewModeFromAvatarBubbleMode(mode, &bubble_view_mode);
  if (SigninViewController::ShouldShowSigninForMode(bubble_view_mode)) {
    browser_->signin_view_controller()->ShowSignin(
        bubble_view_mode, browser_.get(), access_point);
  } else {
    ProfileChooserView::ShowBubble(
        bubble_view_mode, manage_accounts_params, access_point, avatar_button,
        nullptr, gfx::Rect(), browser(), focus_first_profile_button);
    ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
  }
#else
  NOTREACHED();
#endif
}

int BrowserView::GetRenderViewHeightInsetWithDetachedBookmarkBar() {
  if (browser_->bookmark_bar_state() != BookmarkBar::DETACHED ||
      !bookmark_bar_view_ || !bookmark_bar_view_->IsDetached()) {
    return 0;
  }
  // Don't use bookmark_bar_view_->height() which won't be the final height if
  // the bookmark bar is animating.
  return GetLayoutConstant(BOOKMARK_BAR_NTP_HEIGHT);
}

void BrowserView::ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) {
  extension_keybinding_registry_->ExecuteCommand(extension->id(),
                                                 command.accelerator());
}

ExclusiveAccessContext* BrowserView::GetExclusiveAccessContext() {
  return this;
}

void BrowserView::ShowImeWarningBubble(
    const extensions::Extension* extension,
    const base::Callback<void(ImeWarningBubblePermissionStatus status)>&
        callback) {
  ImeWarningBubbleView::ShowBubble(extension, this, callback);
}

std::string BrowserView::GetWorkspace() const {
  return frame_->GetWorkspace();
}

bool BrowserView::IsVisibleOnAllWorkspaces() const {
  return frame_->IsVisibleOnAllWorkspaces();
}

bool BrowserView::DoCutCopyPasteForWebContents(
    WebContents* contents,
    void (WebContents::*method)()) {
  // It's possible for a non-null WebContents to have a null RWHV if it's
  // crashed or otherwise been killed.
  content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
  if (!rwhv || !rwhv->HasFocus())
    return false;
  // Calling |method| rather than using a fake key event is important since a
  // fake event might be consumed by the web content.
  (contents->*method)();
  return true;
}

void BrowserView::ActivateAppModalDialog() const {
  // If another browser is app modal, flash and activate the modal browser.
  app_modal::JavaScriptAppModalDialog* active_dialog =
      app_modal::AppModalDialogQueue::GetInstance()->active_dialog();
  if (!active_dialog)
    return;

  Browser* modal_browser =
      chrome::FindBrowserWithWebContents(active_dialog->web_contents());
  if (modal_browser && (browser_.get() != modal_browser)) {
    modal_browser->window()->FlashFrame(true);
    modal_browser->window()->Activate();
  }

  app_modal::AppModalDialogQueue::GetInstance()->ActivateModalDialog();
}

bool BrowserView::FindCommandIdForAccelerator(
    const ui::Accelerator& accelerator,
    int* command_id) const {
  auto iter = accelerator_table_.find(accelerator);
  if (iter == accelerator_table_.end())
    return false;

  *command_id = iter->second;
  if (accelerator.IsRepeat() && !IsCommandRepeatable(*command_id))
    return false;

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ExclusiveAccessContext implementation:
Profile* BrowserView::GetProfile() {
  return browser_->profile();
}

void BrowserView::UpdateUIForTabFullscreen(TabFullscreenState state) {
  frame()->GetFrameView()->UpdateFullscreenTopUI(
      state == ExclusiveAccessContext::STATE_ENTER_TAB_FULLSCREEN);
}

WebContents* BrowserView::GetActiveWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void BrowserView::UnhideDownloadShelf() {
  if (download_shelf_)
    download_shelf_->Unhide();
}

void BrowserView::HideDownloadShelf() {
  if (download_shelf_)
    download_shelf_->Hide();

  StatusBubble* status_bubble = GetStatusBubble();
  if (status_bubble)
    status_bubble->Hide();
}

bool BrowserView::CanUserExitFullscreen() const {
  return frame_->GetFrameView()->CanUserExitFullscreen();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ExclusiveAccessBubbleViewsContext implementation:
ExclusiveAccessManager* BrowserView::GetExclusiveAccessManager() {
  return browser_->exclusive_access_manager();
}

views::Widget* BrowserView::GetBubbleAssociatedWidget() {
  return GetWidget();
}

ui::AcceleratorProvider* BrowserView::GetAcceleratorProvider() {
  return this;
}

gfx::NativeView BrowserView::GetBubbleParentView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point BrowserView::GetCursorPointInParent() const {
  gfx::Point cursor_pos = display::Screen::GetScreen()->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(GetWidget()->GetRootView(), &cursor_pos);
  return cursor_pos;
}

gfx::Rect BrowserView::GetClientAreaBoundsInScreen() const {
  return GetWidget()->GetClientAreaBoundsInScreen();
}

bool BrowserView::IsImmersiveModeEnabled() const {
  return immersive_mode_controller()->IsEnabled();
}

gfx::Rect BrowserView::GetTopContainerBoundsInScreen() {
  return top_container_->GetBoundsInScreen();
}

void BrowserView::DestroyAnyExclusiveAccessBubble() {
  exclusive_access_bubble_.reset();
}

bool BrowserView::CanTriggerOnMouse() const {
  return !IsImmersiveModeEnabled();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, extension::ExtensionKeybindingRegistry::Delegate implementation:
extensions::ActiveTabPermissionGranter*
BrowserView::GetActiveTabPermissionGranter() {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return nullptr;
  return extensions::TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ImmersiveModeController::Observer implementation:
void BrowserView::OnImmersiveRevealStarted() {
  top_container()->SetPaintToLayer();
  top_container()->layer()->SetFillsBoundsOpaquely(false);
  overlay_view_->AddChildView(top_container());
  overlay_view_->SetVisible(true);
  InvalidateLayout();
  GetWidget()->GetRootView()->Layout();
}

void BrowserView::OnImmersiveRevealEnded() {
  ReparentTopContainerForEndOfImmersive();
  InvalidateLayout();
  GetWidget()->GetRootView()->Layout();
}

void BrowserView::OnImmersiveFullscreenExited() {
  OnImmersiveRevealEnded();
}

void BrowserView::OnImmersiveModeControllerDestroyed() {
  ReparentTopContainerForEndOfImmersive();
}
