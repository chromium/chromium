// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/native_window_notification_source.h"
#include "chrome/browser/platform_util.h"
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
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_view.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/accessibility/accessibility_focus_highlight.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"
#include "chrome/browser/ui/views/accessibility/caret_browsing_dialog_delegate.h"
#include "chrome/browser/ui/views/accessibility/invert_bubble_view.h"
#include "chrome/browser/ui/views/autofill/autofill_bubble_handler_impl.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/contents_layout_manager.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/top_container_loading_bar.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"
#include "chrome/browser/ui/views/frame/web_footer_experiment_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_controller_views.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view_impl.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_icon_view.h"
#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/browser/ui/views/update_recommended_message_box.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
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
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/password_protection/metrics_util.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/version_info/channel.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/views/accessibility/view_accessibility_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/desks_helper.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/views/frame/top_controls_slide_controller_chromeos.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/compositor/throughput_tracker.h"
#else
#include "chrome/browser/ui/signin_view_controller.h"
#endif  // !defined(OS_CHROMEOS)

#if defined(OS_MAC)
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
#include <shobjidl.h>
#include <wrl/client.h>
#include "base/win/windows_version.h"
#include "chrome/browser/taskbar/taskbar_decorator_win.h"
#include "chrome/browser/win/jumplist.h"
#include "chrome/browser/win/jumplist_factory.h"
#include "chrome/browser/win/titlebar_config.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme_win.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
#include "chrome/browser/ui/sync/one_click_signin_links_delegate_impl.h"
#include "chrome/browser/ui/views/sync/one_click_signin_dialog_view.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

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

#if defined(OS_CHROMEOS)
// UMA histograms that record animation smoothness for tab loading animation.
constexpr char kTabLoadingSmoothnessHistogramName[] =
    "Chrome.Tabs.AnimationSmoothness.TabLoading";

void RecordTabLoadingSmoothness(int smoothness) {
  UMA_HISTOGRAM_PERCENTAGE(kTabLoadingSmoothnessHistogramName, smoothness);
}
#endif

// See SetDisableRevealerDelayForTesting().
bool g_disable_revealer_delay_for_testing = false;

#if DCHECK_IS_ON()

std::string FocusListToString(views::View* view) {
  std::ostringstream result;
  base::flat_set<views::View*> seen_views;

  while (view) {
    if (base::Contains(seen_views, view)) {
      result << "*CYCLE TO " << view->GetClassName() << "*";
      break;
    }
    seen_views.insert(view);
    result << view->GetClassName() << " ";

    view = view->GetNextFocusableView();
  }

  return result.str();
}

void CheckFocusListForCycles(views::View* const start_view) {
  views::View* view = start_view;

  base::flat_set<views::View*> seen_views;

  while (view) {
    DCHECK(!base::Contains(seen_views, view)) << FocusListToString(start_view);
    seen_views.insert(view);

    views::View* next_view = view->GetNextFocusableView();
    if (next_view) {
      DCHECK_EQ(view, next_view->GetPreviousFocusableView())
          << view->GetClassName();
    }

    view = next_view;
  }
}

#endif  // DCHECK_IS_ON()

bool GetGestureCommand(ui::GestureEvent* event, int* command) {
  DCHECK(command);
  *command = 0;
#if defined(OS_MAC)
  if (event->details().type() == ui::ET_GESTURE_SWIPE) {
    if (event->details().swipe_left()) {
      *command = IDC_BACK;
      return true;
    } else if (event->details().swipe_right()) {
      *command = IDC_FORWARD;
      return true;
    }
  }
#endif  // OS_MAC
  return false;
}

bool IsShowingWebContentsModalDialog(content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  return manager && manager->IsDialogActive();
}

// Overlay view that owns TopContainerView in some cases (such as during
// immersive fullscreen reveal).
class TopContainerOverlayView : public views::View {
 public:
  explicit TopContainerOverlayView(base::WeakPtr<BrowserView> browser_view)
      : browser_view_(std::move(browser_view)) {}
  ~TopContainerOverlayView() override = default;

  void ChildPreferredSizeChanged(views::View* child) override {
    // When a child of BrowserView changes its preferred size, it
    // invalidates the BrowserView's layout as well. When a child is
    // reparented under this overlay view, this doesn't happen since the
    // overlay view is owned by NonClientView.
    //
    // BrowserView's layout logic still applies in this case. To ensure
    // it is used, we must invalidate BrowserView's layout.
    if (browser_view_)
      browser_view_->InvalidateLayout();
  }

 private:
  // The BrowserView this overlay is created for. WeakPtr is used since
  // this view is held in a different hierarchy.
  base::WeakPtr<BrowserView> browser_view_;
};

// A view targeter for the overlay view, which makes sure the overlay view
// itself is never a target for events, but its children (i.e. top_container)
// may be.
class OverlayViewTargeterDelegate : public views::ViewTargeterDelegate {
 public:
  OverlayViewTargeterDelegate() = default;
  OverlayViewTargeterDelegate(const OverlayViewTargeterDelegate&) = delete;
  OverlayViewTargeterDelegate& operator=(const OverlayViewTargeterDelegate&) =
      delete;
  ~OverlayViewTargeterDelegate() override = default;

  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    const auto& children = target->children();
    const auto hits_child = [target, rect](const views::View* child) {
      gfx::RectF child_rect(rect);
      views::View::ConvertRectToTarget(target, child, &child_rect);
      return child->HitTestRect(gfx::ToEnclosingRect(child_rect));
    };
    return std::any_of(children.cbegin(), children.cend(), hits_child);
  }
};

class ContentsSeparator : public views::Separator {
 private:
  // views::View:
  void OnThemeChanged() override {
    views::Separator::OnThemeChanged();
    UpdateColor();
  }
  void AddedToWidget() override { UpdateColor(); }

  void UpdateColor() {
    const ui::ThemeProvider* const theme_provider = GetThemeProvider();
    SetColor(color_utils::GetResultingPaintColor(
        theme_provider->GetColor(
            ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR),
        theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR)));
  }
};

bool ShouldShowWindowIcon(const Browser* browser) {
#if defined(OS_CHROMEOS)
  // For Chrome OS only, trusted windows (apps and settings) do not show a
  // window icon, crbug.com/119411. Child windows (i.e. popups) do show an icon.
  if (browser->is_trusted_source())
    return false;
#endif
  return browser->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// Delegate implementation for BrowserViewLayout. Usually just forwards calls
// into BrowserView.
class BrowserViewLayoutDelegateImpl : public BrowserViewLayoutDelegate {
 public:
  explicit BrowserViewLayoutDelegateImpl(BrowserView* browser_view)
      : browser_view_(browser_view) {}
  BrowserViewLayoutDelegateImpl(const BrowserViewLayoutDelegateImpl&) = delete;
  BrowserViewLayoutDelegateImpl& operator=(
      const BrowserViewLayoutDelegateImpl&) = delete;
  ~BrowserViewLayoutDelegateImpl() override = default;

  bool IsTabStripVisible() const override {
    return browser_view_->IsTabStripVisible();
  }

  gfx::Rect GetBoundsForTabStripRegionInBrowserView() const override {
    const gfx::Size tabstrip_minimum_size =
        browser_view_->tab_strip_region_view()->GetMinimumSize();
    gfx::RectF bounds_f(browser_view_->frame()->GetBoundsForTabStripRegion(
        tabstrip_minimum_size));
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

  bool IsContentsSeparatorEnabled() const override {
    // Web app windows manage their own separator.
    // TODO(crbug.com/1012979): Make PWAs set the visibility of the ToolbarView
    // based on whether it is visible instead of setting the height to 0px. This
    // will enable BrowserViewLayout to hide the contents separator on its own
    // using the same logic used by normal BrowserViews.
    return !browser_view_->browser()->app_controller();
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

  bool SupportsWindowFeature(Browser::WindowFeature feature) const override {
    return browser_view_->browser()->SupportsWindowFeature(feature);
  }

  gfx::NativeView GetHostView() const override {
    return browser_view_->GetWidget()->GetNativeView();
  }

  bool BrowserIsTypeNormal() const override {
    return browser_view_->browser()->is_type_normal();
  }

  bool HasFindBarController() const override {
    return browser_view_->browser()->HasFindBarController();
  }

  void MoveWindowForFindBarIfNecessary() const override {
    auto* const controller = browser_view_->browser()->GetFindBarController();
    return controller->find_bar()->MoveWindowIfNecessary();
  }

 private:
  BrowserView* browser_view_;
};

///////////////////////////////////////////////////////////////////////////////
// BrowserView, public:

// static
const char BrowserView::kViewClassName[] = "BrowserView";

BrowserView::BrowserView(std::unique_ptr<Browser> browser)
    : views::ClientView(nullptr, nullptr), browser_(std::move(browser)) {
  SetShowIcon(::ShouldShowWindowIcon(browser_.get()));
  SetHasWindowSizeControls(!chrome::IsRunningInForcedAppMode());
  SetCanResize(browser_->can_resize());

  browser_->tab_strip_model()->AddObserver(this);
  immersive_mode_controller_ = chrome::CreateImmersiveModeController();

  // Top container holds tab strip region and toolbar and lives at the front of
  // the view hierarchy.

  std::unique_ptr<TabMenuModelFactory> tab_menu_model_factory;
  if (browser_->app_controller()) {
    tab_menu_model_factory =
        browser_->app_controller()->GetTabMenuModelFactory();
  }
  // TabStrip takes ownership of the controller.
  auto tabstrip_controller = std::make_unique<BrowserTabStripController>(
      browser_->tab_strip_model(), this, std::move(tab_menu_model_factory));
  BrowserTabStripController* tabstrip_controller_ptr =
      tabstrip_controller.get();
  auto tabstrip = std::make_unique<TabStrip>(std::move(tabstrip_controller));
  tabstrip_ = tabstrip.get();
  tabstrip_controller_ptr->InitFromModel(tabstrip_);
  top_container_ = AddChildView(std::make_unique<TopContainerView>(this));
  tab_strip_region_view_ = top_container_->AddChildView(
      std::make_unique<TabStripRegionView>(std::move(tabstrip)));

  feature_promo_controller_ =
      std::make_unique<FeaturePromoControllerViews>(this);

  // Create WebViews early so |webui_tab_strip_| can observe their size.
  auto devtools_web_view =
      std::make_unique<views::WebView>(browser_->profile());
  devtools_web_view->SetID(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_web_view->SetVisible(false);

  auto contents_web_view =
      std::make_unique<ContentsWebView>(browser_->profile());
  contents_web_view->SetID(VIEW_ID_TAB_CONTAINER);
  contents_web_view->SetEmbedFullscreenWidgetMode(true);

  auto contents_container = std::make_unique<views::View>();
  devtools_web_view_ =
      contents_container->AddChildView(std::move(devtools_web_view));
  contents_web_view_ =
      contents_container->AddChildView(std::move(contents_web_view));
  contents_web_view_->set_is_primary_web_contents_for_window(true);
  contents_container->SetLayoutManager(std::make_unique<ContentsLayoutManager>(
      devtools_web_view_, contents_web_view_));

  toolbar_ = top_container_->AddChildView(
      std::make_unique<ToolbarView>(browser_.get(), this));

  contents_separator_ =
      top_container_->AddChildView(std::make_unique<ContentsSeparator>());

  web_contents_close_handler_ =
      std::make_unique<WebContentsCloseHandler>(contents_web_view_);

  contents_container_ = AddChildView(std::move(contents_container));
  set_contents_view(contents_container_);

  // InfoBarContainer needs to be added as a child here for drop-shadow, but
  // needs to come after toolbar in focus order (see EnsureFocusOrder()).
  infobar_container_ =
      AddChildView(std::make_unique<InfoBarContainerView>(this));

  InitStatusBubble();

  // Create do-nothing view for the sake of controlling the z-order of the find
  // bar widget.
  find_bar_host_view_ = AddChildView(std::make_unique<View>());

#if defined(OS_WIN)
  // Create a custom JumpList and add it to an observer of TabRestoreService
  // so we can update the custom JumpList when a tab is added or removed.
  if (JumpList::Enabled())
    load_complete_listener_ = std::make_unique<LoadCompleteListener>(this);
#endif
}

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

  // Reset autofill bubble handler to make sure it does not out-live toolbar,
  // since it is responsible for showing autofill related bubbles from toolbar's
  // child views and it is an observer for avatar toolbar button if any.
  autofill_bubble_handler_.reset();

  auto* global_registry =
      extensions::ExtensionCommandsGlobalRegistry::Get(browser_->profile());
  if (global_registry->registry_for_active_window() ==
          extension_keybinding_registry_.get())
    global_registry->set_registry_for_active_window(nullptr);

  // The TabStrip attaches a listener to the model. Make sure we shut down the
  // TabStrip first so that it can cleanly remove the listener.
  if (tabstrip_)
    tabstrip_->parent()->RemoveChildViewT(tabstrip_);

  // Child views maintain PrefMember attributes that point to
  // OffTheRecordProfile's PrefService which gets deleted by ~Browser.
  RemoveAllChildViews(true);
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
  // It might look like this method should be implemented as:
  //   return static_cast<BrowserView*>(browser->window())
  // but in fact in unit tests browser->window() may not be a BrowserView even
  // in Views Browser builds. Always go through the ForNativeWindow path, which
  // is robust against being given any kind of native window.
  //
  // Also, tests don't always have a non-null NativeWindow backing the
  // BrowserWindow, so be sure to check for that as well.
  //
  // Lastly note that this function can be called during construction of
  // Browser, at which point browser->window() might return nullptr.
  if (!browser->window() || !browser->window()->GetNativeWindow())
    return nullptr;
  return GetBrowserViewForNativeWindow(browser->window()->GetNativeWindow());
}

// static
void BrowserView::SetDisableRevealerDelayForTesting(bool disable) {
  g_disable_revealer_delay_for_testing = disable;
}

void BrowserView::DisableTopControlsSlideForTesting() {
  top_controls_slide_controller_.reset();
}

void BrowserView::InitStatusBubble() {
  status_bubble_ = std::make_unique<StatusBubbleViews>(contents_web_view_);
  contents_web_view_->SetStatusBubble(status_bubble_.get());
}

gfx::Rect BrowserView::GetFindBarBoundingBox() const {
  gfx::Rect contents_bounds = contents_container_->ConvertRectToWidget(
      contents_container_->GetLocalBounds());

  // If the location bar is visible use it to position the bounding box,
  // otherwise use the contents container.
  if (!immersive_mode_controller_->IsEnabled() ||
      immersive_mode_controller_->IsRevealed()) {
    const gfx::Rect bounding_box =
        toolbar_button_provider_->GetFindBarBoundingBox(
            contents_bounds.bottom());
    if (!bounding_box.IsEmpty())
      return bounding_box;
  }

  contents_bounds.Inset(0, 0, gfx::scrollbar_size(), 0);
  return contents_container_->GetMirroredRect(contents_bounds);
}

int BrowserView::GetTabStripHeight() const {
  // We want to return tabstrip_->height(), but we might be called in the midst
  // of layout, when that hasn't yet been updated to reflect the current state.
  // So return what the tabstrip height _ought_ to be right now.
  return IsTabStripVisible() ? tabstrip_->GetPreferredSize().height() : 0;
}

TabSearchButton* BrowserView::GetTabSearchButton() {
  if (!base::FeatureList::IsEnabled(features::kTabSearch))
    return nullptr;

  // If kTabSearchFixedEntrypoint is enabled then the tab search button is
  // defined in the tab strip region view.
  // TODO(tluk): Consolidate these once Tab Scrolling successfully moves the
  // tab controls container to the tab strip region view.
  return base::FeatureList::IsEnabled(features::kTabSearchFixedEntrypoint)
             ? tab_strip_region_view_->tab_search_button()
             : tabstrip_->tab_search_button();
}

bool BrowserView::IsTabStripVisible() const {
  // Return false if this window does not normally display a tabstrip or if the
  // tabstrip is currently hidden, e.g. because we're in fullscreen.
  if (!browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP))
    return false;

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (WebUITabStripContainerView::UseTouchableTabStrip(browser_.get()))
    return false;
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  // Return false if the tabstrip has not yet been created (by InitViews()),
  // since callers may otherwise try to access it. Note that we can't just check
  // this alone, as the tabstrip is created unconditionally even for windows
  // that won't display it.
  return tabstrip_ != nullptr;
}

bool BrowserView::IsIncognito() const {
  return browser_->profile()->IsIncognitoProfile();
}

bool BrowserView::IsGuestSession() const {
  return browser_->profile()->IsGuestSession();
}

bool BrowserView::IsRegularOrGuestSession() const {
  return profiles::IsRegularOrGuestSession(browser_.get());
}

bool BrowserView::GetAccelerator(int cmd_id,
                                 ui::Accelerator* accelerator) const {
#if defined(OS_MAC)
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

bool BrowserView::CanSupportTabStrip() const {
  return browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserView::IsBrowserTypeWebApp() const {
  return web_app::AppBrowserController::IsForWebAppBrowser(browser_.get());
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
  BrowserList::SetLastActive(browser());
#endif

  // If the window is already visible, just activate it.
  if (frame_->IsVisible()) {
    frame_->Activate();
    return;
  }

  // Only set |restore_focus_on_activation_| when it is not set so that restore
  // focus on activation only happen once for the very first Show() call.
  if (!restore_focus_on_activation_.has_value())
    restore_focus_on_activation_ = true;

  frame_->Show();

  browser()->OnWindowDidShow();

  MaybeShowInvertBubbleView(this);

  // The fullscreen transition clears out focus, but there are some cases (for
  // example, new window in Mac fullscreen with toolbar showing) where we need
  // restore it.
  if (frame_->IsFullscreen() &&
      !frame_->GetFrameView()->ShouldHideTopUIForFullscreen() &&
      GetFocusManager() && !GetFocusManager()->GetFocusedView()) {
    SetFocusToLocationBar(false);
  }

#if !defined(OS_CHROMEOS)
  if (features::IsAccessibilityFocusHighlightEnabled() &&
      !accessibility_focus_highlight_) {
    accessibility_focus_highlight_ =
        std::make_unique<AccessibilityFocusHighlight>(this);
  }
#endif  // !defined(OS_CHROMEOS)
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

ui::ZOrderLevel BrowserView::GetZOrderLevel() const {
  return ui::ZOrderLevel::kNormal;
}

void BrowserView::SetZOrderLevel(ui::ZOrderLevel level) {
  // Not implemented for browser windows.
  NOTIMPLEMENTED();
}

gfx::NativeWindow BrowserView::GetNativeWindow() const {
  // While the browser destruction is going on, the widget can already be gone,
  // but utility functions like FindBrowserWithWindow will still call this.
  return GetWidget() ? GetWidget()->GetNativeWindow() : nullptr;
}

bool BrowserView::IsOnCurrentWorkspace() const {
  // In tests, the native window can be nullptr.
  gfx::NativeWindow native_win = GetNativeWindow();
  if (!native_win)
    return true;

#if defined(OS_CHROMEOS)
  return ash::DesksHelper::Get()->BelongsToActiveDesk(native_win);
#elif defined(OS_WIN)
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return true;

  Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager;
  if (!SUCCEEDED(::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr,
                                    CLSCTX_ALL,
                                    IID_PPV_ARGS(&virtual_desktop_manager)))) {
    return true;
  }

  BOOL on_current_desktop;
  if (FAILED(virtual_desktop_manager->IsWindowOnCurrentVirtualDesktop(
          native_win->GetHost()->GetAcceleratedWidget(),
          &on_current_desktop)) ||
      on_current_desktop) {
    return true;
  }

  // IsWindowOnCurrentVirtualDesktop() is flaky for newly opened windows,
  // which causes test flakiness. Occasionally, it incorrectly says a window
  // is not on the current virtual desktop when it is. In this situation,
  // it also returns GUID_NULL for the desktop id.
  GUID workspace_guid;
  return !SUCCEEDED(virtual_desktop_manager->GetWindowDesktopId(
             native_win->GetHost()->GetAcceleratedWidget(), &workspace_guid)) ||
         workspace_guid == GUID_NULL;
#else
  return true;
#endif  // defined(OS_CHROMEOS)
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
  if (!loading_animation_timer_.IsRunning() && CanChangeWindowIcon())
    frame_->UpdateWindowIcon();
}

void BrowserView::UpdateFrameColor() {
  frame_->GetFrameView()->UpdateFrameColor();
}

void BrowserView::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  if (bookmark_bar_view_.get()) {
    BookmarkBar::State new_state = browser_->bookmark_bar_state();
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
#if defined(OS_CHROMEOS)
      loading_animation_tracker_.emplace(
        GetWidget()->GetCompositor()->RequestNewThroughputTracker());
      loading_animation_tracker_->Start(ash::metrics_util::ForSmoothness(
        base::BindRepeating(&RecordTabLoadingSmoothness)));
#endif
      // Loads are happening, and the timer isn't running, so start it.
      loading_animation_start_ = base::TimeTicks::Now();
      loading_animation_timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(30),
                                     this,
                                     &BrowserView::LoadingAnimationCallback);
    }
  } else {
    if (loading_animation_timer_.IsRunning()) {
      loading_animation_timer_.Stop();
#if defined(OS_CHROMEOS)
      loading_animation_tracker_->Stop();
#endif
      // Loads are now complete, update the state if a task was scheduled.
      LoadingAnimationCallback();
    }
  }
}

void BrowserView::SetStarredState(bool is_starred) {
  PageActionIconView* star_icon =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kBookmarkStar);
  if (star_icon)
    star_icon->SetActive(is_starred);
}

void BrowserView::SetTranslateIconToggled(bool is_lit) {
  // Translate icon is never active on Views.
}

void BrowserView::OnActiveTabChanged(content::WebContents* old_contents,
                                     content::WebContents* new_contents,
                                     int index,
                                     int reason) {
  DCHECK(new_contents);
  TRACE_EVENT0("ui", "BrowserView::OnActiveTabChanged");

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

#if defined(OS_MAC)
  // Widget::IsActive is inconsistent between Mac and Aura, so don't check for
  // it on Mac. The check is also unnecessary for Mac, since restoring focus
  // won't activate the widget on that platform.
  bool will_restore_focus =
      !browser_->tab_strip_model()->closing_all() && GetWidget()->IsVisible();
#else
  bool will_restore_focus = !browser_->tab_strip_model()->closing_all() &&
                            GetWidget()->IsActive() && GetWidget()->IsVisible();
#endif
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
    if (loading_bar_)
      loading_bar_->SetWebContents(nullptr);
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

  ObserveAppBannerManager(
      banners::AppBannerManager::FromWebContents(new_contents));

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
    if (loading_bar_)
      loading_bar_->SetWebContents(new_contents);
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
    if (loading_bar_)
      loading_bar_->SetWebContents(nullptr);
    contents_web_view_->SetWebContents(nullptr);
    infobar_container_->ChangeInfoBarManager(nullptr);
    app_banner_manager_observer_.RemoveAll();
    UpdateDevToolsForContents(nullptr, true);
  }
}

void BrowserView::OnTabRestored(int command_id) {
  reopen_tab_promo_controller_.OnTabReopened(command_id);
}

void BrowserView::ZoomChangedForActiveTab(bool can_show_bubble) {
  const AppMenuButton* app_menu_button =
      toolbar_button_provider()->GetAppMenuButton();
  bool app_menu_showing = app_menu_button && app_menu_button->IsMenuShowing();
  toolbar_button_provider()
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

void BrowserView::SetContentsSize(const gfx::Size& size) {
  DCHECK(!GetContentsSize().IsEmpty());

  const int width_diff = size.width() - GetContentsSize().width();
  const int height_diff = size.height() - GetContentsSize().height();

  // Resizing the window may be expensive, so only do it if the size is wrong.
  if (width_diff == 0 && height_diff == 0)
    return;

  gfx::Rect bounds = GetBounds();
  bounds.set_width(bounds.width() + width_diff);
  bounds.set_height(bounds.height() + height_diff);

  // Constrain the final bounds to the current screen's available area. Bounds
  // enforcement applied earlier does not know the specific frame dimensions.
  // Changes to the window size should not generally trigger screen changes.
  auto display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(GetNativeWindow());
  bounds.AdjustToFit(display.work_area());
  SetBounds(bounds);
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
                                  ExclusiveAccessBubbleType bubble_type,
                                  const int64_t display_id) {
  if (IsFullscreen())
    return;  // Nothing to do.

  ProcessFullscreen(true, url, bubble_type, display_id);
}

void BrowserView::ExitFullscreen() {
  if (!IsFullscreen())
    return;  // Nothing to do.

  ProcessFullscreen(false, GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
                    display::kInvalidDisplayId);
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

  exclusive_access_bubble_ = std::make_unique<ExclusiveAccessBubbleViews>(
      this, url, bubble_type, std::move(bubble_first_hide_callback));
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

void BrowserView::FullscreenStateChanging() {
  bool fullscreen = IsFullscreen();
  ProcessFullscreen(
      fullscreen, GURL(),
      fullscreen
          ? GetExclusiveAccessManager()->GetExclusiveAccessExitBubbleType()
          : EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
      display::kInvalidDisplayId);
  frame_->GetFrameView()->OnFullscreenStateChanged();
}

void BrowserView::FullscreenStateChanged() {
#if defined(OS_MAC)
  if (!IsFullscreen() && restore_pre_fullscreen_bounds_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(restore_pre_fullscreen_bounds_callback_));
  }
#endif  // OS_MAC
}

void BrowserView::SetToolbarButtonProvider(ToolbarButtonProvider* provider) {
  toolbar_button_provider_ = provider;
  // Recreate the autofill bubble handler when toolbar button provider changes.
  autofill_bubble_handler_ =
      std::make_unique<autofill::AutofillBubbleHandlerImpl>(
          browser_.get(), toolbar_button_provider_);
}

void BrowserView::UpdatePageActionIcon(PageActionIconType type) {
  PageActionIconView* icon =
      toolbar_button_provider_->GetPageActionIconView(type);
  if (icon)
    icon->Update();
}

autofill::AutofillBubbleHandler* BrowserView::GetAutofillBubbleHandler() {
  return autofill_bubble_handler_.get();
}

void BrowserView::ExecutePageActionIconForTesting(PageActionIconType type) {
  toolbar_button_provider_->GetPageActionIconView(type)->ExecuteForTesting();
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
  if (!IsActive())
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
  if (toolbar_button_provider_->GetReloadButton()) {
    toolbar_button_provider_->GetReloadButton()->ChangeMode(
        is_loading ? ReloadButton::Mode::kStop : ReloadButton::Mode::kReload,
        force);
  }
}

void BrowserView::UpdateToolbar(content::WebContents* contents) {
  // We may end up here during destruction.
  if (toolbar_)
    toolbar_->Update(contents);
}

void BrowserView::UpdateCustomTabBarVisibility(bool visible, bool animate) {
  if (toolbar_)
    toolbar_->UpdateCustomTabBarVisibility(visible, animate);
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

ExtensionsContainer* BrowserView::GetExtensionsContainer() {
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu))
    return toolbar_button_provider_->GetExtensionsToolbarContainer();

  BrowserActionsContainer* container =
      toolbar_button_provider_->GetBrowserActionsContainer();
  // Note that in some cases (such as an app window), there is no extensions
  // container.
  return container ? container->toolbar_actions_bar() : nullptr;
}

void BrowserView::ToolbarSizeChanged(bool is_animating) {
  if (is_animating)
    contents_web_view_->SetFastResize(true);
  UpdateUIForContents(GetActiveWebContents());

  // Do nothing if we're currently participating in a tab dragging process. The
  // fast resize bit will be reset and the web contents will get re-layed out
  // after the tab dragging ends.
  if (frame()->tab_drag_kind() != TabDragKind::kNone)
    return;

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

void BrowserView::TabDraggingStatusChanged(bool is_dragging) {
  // TODO(crbug.com/1110266): Remove explicit OS_CHROMEOS check once OS_LINUX
  // CrOS cleanup is done.
#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
  contents_web_view_->SetFastResize(is_dragging);
  if (!is_dragging) {
    // When tab dragging is ended, we need to make sure the web contents get
    // re-layed out. Otherwise we may see web contents get clipped to the window
    // size that was used during dragging.
    contents_web_view_->InvalidateLayout();
    contents_container_->Layout();
  }
#endif
}

BrowserView::OnLinkOpeningFromGestureSubscription
BrowserView::AddOnLinkOpeningFromGestureCallback(
    OnLinkOpeningFromGestureCallback callback) {
  return link_opened_from_gesture_callbacks_.Add(callback);
}

void BrowserView::LinkOpeningFromGesture(WindowOpenDisposition disposition) {
  link_opened_from_gesture_callbacks_.Notify(disposition);
}

void BrowserView::FocusBookmarksToolbar() {
  DCHECK(!immersive_mode_controller_->IsEnabled());
  if (bookmark_bar_view_ && bookmark_bar_view_->GetVisible() &&
      bookmark_bar_view_->GetPreferredSize().height() != 0) {
    bookmark_bar_view_->SetPaneFocusAndFocusDefault();
  }
}

void BrowserView::FocusInactivePopupForAccessibility() {
  if (ActivateFirstInactiveBubbleForAccessibility())
    return;

  if (!infobar_container_->children().empty())
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
  // provide an easy access method to these dialogs without requiring additional
  // keyboard shortcuts or commands. To get back out to pane cycling the dialog
  // needs to be accepted or dismissed.
  if (ActivateFirstInactiveBubbleForAccessibility())
    return;

  GetFocusManager()->RotatePaneFocus(
      forwards ? views::FocusManager::kForward : views::FocusManager::kBackward,
      views::FocusManager::FocusCycleWrapping::kEnabled);
}

bool BrowserView::ActivateFirstInactiveBubbleForAccessibility() {
  if (GetLocationBarView()->ActivateFirstInactiveBubbleForAccessibility())
    return true;

  // TODO: this fixes crbug.com/1042010 and crbug.com/1052676, but a more
  // general solution should be desirable to find any bubbles anchored in the
  // views hierarchy.
  if (toolbar_ && toolbar_->app_menu_button()) {
    views::BubbleDialogDelegate* bubble =
        toolbar_->app_menu_button()->GetProperty(views::kAnchoredDialogKey);
    if (!bubble && GetLocationBarView())
      bubble = GetLocationBarView()->GetProperty(views::kAnchoredDialogKey);

    if (bubble) {
      View* focusable = bubble->GetInitiallyFocusedView();

      // A PermissionPromptBubbleView will explicitly return nullptr due to
      // crbug.com/619429. In that case, we explicitly focus the cancel button.
      if (!focusable)
        focusable = bubble->GetCancelButton();

      if (focusable) {
        focusable->RequestFocus();
        return true;
      }
    }
  }

  if (toolbar_ && toolbar_->toolbar_account_icon_container() &&
      toolbar_->toolbar_account_icon_container()
          ->page_action_icon_controller()
          ->ActivateFirstInactiveBubbleForAccessibility()) {
    return true;
  }

  return false;
}

void BrowserView::TryNotifyWindowBoundsChanged(const gfx::Rect& widget_bounds) {
  if (interactive_resize_in_progress_ || last_widget_bounds_ == widget_bounds)
    return;

  last_widget_bounds_ = widget_bounds;
  browser()->extension_window_controller()->NotifyWindowBoundsChanged();
}

void BrowserView::TouchModeChanged() {
  MaybeInitializeWebUITabStrip();
  MaybeShowWebUITabStripIPH();
}

void BrowserView::OnFeatureEngagementTrackerInitialized(bool initialized) {
  if (!initialized)
    return;
  MaybeShowWebUITabStripIPH();
}

void BrowserView::MaybeShowWebUITabStripIPH() {
  if (!webui_tab_strip_)
    return;

  feature_promo_controller_->MaybeShowPromo(
      feature_engagement::kIPHWebUITabStripFeature);
}

void BrowserView::DestroyBrowser() {
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
  if (immersive_mode_controller_->ShouldHideTopViews())
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
#if defined(OS_MAC)
  // This Mac-only preference disables display of the toolbar in fullscreen mode
  // so we need to take it into account when determining if the toolbar is
  // visible - especially as pertains to anchoring views.
  if (IsFullscreen() && !browser()->profile()->GetPrefs()->GetBoolean(
                            prefs::kShowFullscreenToolbar)) {
    return false;
  }
#endif
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

void BrowserView::ShowIntentPickerBubble(
    std::vector<IntentPickerBubbleView::AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    PageActionIconType icon_type,
    const base::Optional<url::Origin>& initiating_origin,
    IntentPickerResponse callback) {
  toolbar_->ShowIntentPickerBubble(std::move(app_info), show_stay_in_chrome,
                                   show_remember_selection, icon_type,
                                   initiating_origin, std::move(callback));
}

void BrowserView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  toolbar_->ShowBookmarkBubble(url, already_bookmarked,
                               bookmark_bar_view_.get());
}

qrcode_generator::QRCodeGeneratorBubbleView*
BrowserView::ShowQRCodeGeneratorBubble(
    content::WebContents* contents,
    qrcode_generator::QRCodeGeneratorBubbleController* controller,
    const GURL& url) {
  qrcode_generator::QRCodeGeneratorBubble* bubble =
      new qrcode_generator::QRCodeGeneratorBubble(GetLocationBarView(),
                                                  contents, controller, url);

  PageActionIconView* icon_view =
      toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kQRCodeGenerator);
  if (icon_view)
    bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show();

  return bubble;
}

SharingDialog* BrowserView::ShowSharingDialog(
    content::WebContents* web_contents,
    SharingDialogData data) {
  auto* dialog_view =
      new SharingDialogView(toolbar_button_provider()->GetAnchorView(
                                PageActionIconType::kSharedClipboard),
                            web_contents, std::move(data));

  views::BubbleDialogDelegateView::CreateBubble(dialog_view)->Show();

  return dialog_view;
}

send_tab_to_self::SendTabToSelfBubbleView* BrowserView::ShowSendTabToSelfBubble(
    content::WebContents* web_contents,
    send_tab_to_self::SendTabToSelfBubbleController* controller,
    bool is_user_gesture) {
  if (!is_user_gesture) {
    return nullptr;
  }

  send_tab_to_self::SendTabToSelfBubbleViewImpl* bubble =
      new send_tab_to_self::SendTabToSelfBubbleViewImpl(
          toolbar_button_provider()->GetAnchorView(
              PageActionIconType::kSendTabToSelf),
          web_contents, controller);
  PageActionIconView* icon_view =
      toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kSendTabToSelf);
  if (icon_view)
    bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(send_tab_to_self::SendTabToSelfBubbleViewImpl::USER_GESTURE);
  return bubble;
}

ShowTranslateBubbleResult BrowserView::ShowTranslateBubble(
    content::WebContents* web_contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  if (contents_web_view_->HasFocus() &&
      !GetLocationBarView()->IsMouseHovered() &&
      web_contents->IsFocusedElementEditable()) {
    return ShowTranslateBubbleResult::EDITABLE_FIELD_IS_ACTIVE;
  }

  ChromeTranslateClient::FromWebContents(web_contents)
      ->GetTranslateManager()
      ->GetLanguageState()
      ->SetTranslateEnabled(true);

  if (IsMinimized())
    return ShowTranslateBubbleResult::BROWSER_WINDOW_MINIMIZED;

  PageActionIconView* translate_icon =
      toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kTranslate);
  TranslateBubbleView::ShowBubble(
      toolbar_button_provider()->GetAnchorView(PageActionIconType::kTranslate),
      translate_icon, web_contents, step, source_language, target_language,
      error_type,
      is_user_gesture ? TranslateBubbleView::USER_GESTURE
                      : TranslateBubbleView::AUTOMATIC);

  return ShowTranslateBubbleResult::SUCCESS;
}

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
void BrowserView::ShowOneClickSigninConfirmation(
    const base::string16& email,
    base::OnceCallback<void(bool)> confirmed_callback) {
  std::unique_ptr<OneClickSigninLinksDelegate> delegate(
      new OneClickSigninLinksDelegateImpl(browser()));
  OneClickSigninDialogView::ShowDialog(email, std::move(delegate),
                                       GetNativeWindow(),
                                       std::move(confirmed_callback));
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
  return download_shelf_ && download_shelf_->IsShowing();
}

DownloadShelf* BrowserView::GetDownloadShelf() {
  if (!download_shelf_) {
    download_shelf_ =
        AddChildView(std::make_unique<DownloadShelfView>(browser_.get(), this));
    GetBrowserViewLayout()->set_download_shelf(download_shelf_);
  }
  return download_shelf_;
}

void BrowserView::ConfirmBrowserCloseWithPendingDownloads(
    int download_count,
    Browser::DownloadCloseType dialog_type,
    const base::Callback<void(bool)>& callback) {
  // The dialog eats mouse events which results in the close button
  // getting stuck in the hover state. Reset the window controls to
  // prevent this.
  frame()->non_client_view()->ResetWindowControls();
  DownloadInProgressDialogView::Show(GetNativeWindow(), download_count,
                                     dialog_type, callback);
}

void BrowserView::UserChangedTheme(BrowserThemeChangeType theme_change_type) {
  // kWebAppTheme is triggered by web apps and will only change colors, not the
  // frame type; just refresh the theme on all views in the browser window.
  if (theme_change_type == BrowserThemeChangeType::kWebAppTheme) {
    GetWidget()->ThemeChanged();
    return;
  }

  // When the browser theme changes, the NativeTheme may also change.
  // In Incognito, the usage of dark or normal hinges on the browser theme.
  if (theme_change_type == BrowserThemeChangeType::kBrowserTheme &&
      !IsRegularOrGuestSession()) {
    ui::NativeTheme::GetInstanceForDarkUI()->NotifyObservers();
    ui::NativeTheme::GetInstanceForNativeUi()->NotifyObservers();

    // Early exit. A native theme change will update all the
    // NativeThemeObservers, and then BrowserFrame will re-enter this method
    // with |theme_change_type| == kNativeTheme, doing all the below things.
    return;
  }

  // When the native theme changes in a way that doesn't change the frame type
  // required, we can skip a frame regeneration. Frame regeneration can cause
  // visible flicker (see crbug/945138) so it's best avoided if all that has
  // changed is, for example, the titlebar color, or the user has switched from
  // light to dark mode.
  const bool should_use_native_frame = frame_->ShouldUseNativeFrame();

  bool must_regenerate_frame;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // GTK and user theme changes can both change frame buttons, so the frame
  // always needs to be regenerated on Linux.
  must_regenerate_frame = true;
#else
  must_regenerate_frame = using_native_frame_ != should_use_native_frame;
#endif

#if defined(OS_WIN)
  // On Windows, DWM transtion does not performed for a frame regeneration in
  // fullscreen mode, so do a lighweight theme change to refresh a bookmark bar
  // on new tab. (see crbug/1002480)
  must_regenerate_frame |=
      theme_change_type == BrowserThemeChangeType::kBrowserTheme &&
      !IsFullscreen();
#else
  must_regenerate_frame |=
      theme_change_type == BrowserThemeChangeType::kBrowserTheme;
#endif

  if (must_regenerate_frame) {
    // This is a heavyweight theme change that requires regenerating the frame
    // as well as repainting the browser window.
    frame_->FrameTypeChanged();
  } else {
    // This is a lightweight theme change, so just refresh the theme on all
    // views in the browser window.
    GetWidget()->ThemeChanged();
  }
  using_native_frame_ = should_use_native_frame;
}

void BrowserView::ShowAppMenu() {
  if (!toolbar_button_provider_->GetAppMenuButton())
    return;

  // Keep the top-of-window views revealed as long as the app menu is visible.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller_->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));

  toolbar_button_provider_->GetAppMenuButton()
      ->menu_button_controller()
      ->Activate(nullptr);
}

content::KeyboardEventProcessingResult BrowserView::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if ((event.GetType() != blink::WebInputEvent::Type::kRawKeyDown) &&
      (event.GetType() != blink::WebInputEvent::Type::kKeyUp)) {
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

  if (browser_->deprecated_is_app()) {
    // Let all keys fall through to a v1 app's web content, even accelerators.
    // We don't use NOT_HANDLED_IS_SHORTCUT here. If we do that, the app
    // might not be able to see a subsequent Char event. See OnHandleInputEvent
    // in content/renderer/render_widget.cc for details.
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

#if defined(OS_CHROMEOS)
  if (ash::AcceleratorController::Get()->IsDeprecated(accelerator)) {
    return (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown)
               ? content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT
               : content::KeyboardEventProcessingResult::NOT_HANDLED;
  }
#endif  // defined(OS_CHROMEOS)

  content::KeyboardEventProcessingResult result =
      frame_->PreHandleKeyboardEvent(event);
  if (result != content::KeyboardEventProcessingResult::NOT_HANDLED)
    return result;

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
  DCHECK_EQ(event.GetType(), blink::WebInputEvent::Type::kRawKeyDown);
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
#if defined(OS_MAC)
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
#endif  // defined(OS_MAC)
}

std::unique_ptr<FindBar> BrowserView::CreateFindBar() {
  return std::make_unique<FindBarHost>(this);
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
  // When the selected tab changes, elements in the omnibox can change, which
  // can change its preferred size. Re-lay-out the toolbar to reflect the
  // possible change.
  if (selection.selection_changed())
    toolbar_->InvalidateLayout();

  if (loading_bar_)
    loading_bar_->SetWebContents(GetActiveWebContents());

  if (change.type() != TabStripModelChange::kInserted)
    return;

  for (const auto& contents : change.GetInsert()->contents) {
#if defined(USE_AURA)
    // WebContents inserted in tabs might not have been added to the root
    // window yet. Per http://crbug/342672 add them now since drawing the
    // WebContents requires root window specific data - information about
    // the screen the WebContents is drawn on, for example.
    if (!contents.contents->GetNativeView()->GetRootWindow()) {
      aura::Window* window = contents.contents->GetNativeView();
      aura::Window* root_window = GetNativeWindow()->GetRootWindow();
      aura::client::ParentWindowWithContext(window, root_window,
                                            root_window->GetBoundsInScreen());
      DCHECK(contents.contents->GetNativeView()->GetRootWindow());
    }
#else
    ALLOW_UNUSED_LOCAL(contents);
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

bool BrowserView::CanActivate() const {
  javascript_dialogs::AppModalDialogQueue* queue =
      javascript_dialogs::AppModalDialogQueue::GetInstance();
  if (!queue->active_dialog() || !queue->active_dialog()->view() ||
      !queue->active_dialog()->view()->IsShowing()) {
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
                                weak_ptr_factory_.GetWeakPtr()));
#endif
  return false;
}

base::string16 BrowserView::GetWindowTitle() const {
  base::string16 title =
      browser_->GetWindowTitleForCurrentTab(true /* include_app_name */);
#if defined(OS_MAC)
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
  if (browser()->is_type_normal() || browser()->is_type_popup()) {
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

  // Finally annotate with the user - add Incognito or guest if it's an
  // incognito or guest window, otherwise use the avatar name.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile->IsGuestSession()) {
    title = l10n_util::GetStringFUTF16(IDS_ACCESSIBLE_GUEST_WINDOW_TITLE_FORMAT,
                                       title);
  } else if (profile->IsIncognitoProfile()) {
    title = l10n_util::GetStringFUTF16(
        IDS_ACCESSIBLE_INCOGNITO_WINDOW_TITLE_FORMAT, title);
  } else if (profile->IsRegularProfile() &&
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
  base::string16 title =
      browser_->GetWindowTitleForTab(include_app_name, index);

  base::Optional<tab_groups::TabGroupId> group =
      tabstrip_->tab_at(index)->group();
  if (group.has_value()) {
    base::string16 group_title = tabstrip_->GetGroupTitle(group.value());
    if (group_title.empty()) {
      title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                         title);
    } else {
      title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_NAMED_GROUP_FORMAT,
                                         title, group_title);
    }
  }

  // Tab has crashed.
  if (tabstrip_->IsTabCrashed(index))
    return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_CRASHED_FORMAT, title);

  // Network error interstitial.
  if (tabstrip_->TabHasNetworkError(index)) {
    return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_NETWORK_ERROR_FORMAT,
                                      title);
  }

  // Alert tab states.
  base::Optional<TabAlertState> alert = tabstrip_->GetTabAlertState(index);
  if (!alert.has_value())
    return title;

  switch (alert.value()) {
    case TabAlertState::AUDIO_PLAYING:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_AUDIO_PLAYING_FORMAT,
                                        title);
    case TabAlertState::USB_CONNECTED:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_USB_CONNECTED_FORMAT,
                                        title);
    case TabAlertState::BLUETOOTH_CONNECTED:
      return l10n_util::GetStringFUTF16(
          IDS_TAB_AX_LABEL_BLUETOOTH_CONNECTED_FORMAT, title);
    case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
      return l10n_util::GetStringFUTF16(
          IDS_TAB_AX_LABEL_BLUETOOTH_SCAN_ACTIVE_FORMAT, title);
    case TabAlertState::HID_CONNECTED:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_HID_CONNECTED_FORMAT,
                                        title);
    case TabAlertState::SERIAL_CONNECTED:
      return l10n_util::GetStringFUTF16(
          IDS_TAB_AX_LABEL_SERIAL_CONNECTED_FORMAT, title);
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
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
      return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_VR_PRESENTING, title);
  }

  NOTREACHED();
  return base::string16();
}

std::vector<views::NativeViewHost*>
BrowserView::GetNativeViewHostsForTopControlsSlide() const {
  std::vector<views::NativeViewHost*> results;
  results.push_back(contents_web_view_->holder());

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (webui_tab_strip_)
    results.push_back(webui_tab_strip_->GetNativeViewHost());
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  return results;
}

void BrowserView::ReparentTopContainerForEndOfImmersive() {
  overlay_view_->SetVisible(false);
  top_container()->DestroyLayer();
  AddChildViewAt(top_container(), 0);
  EnsureFocusOrder();
}

void BrowserView::EnsureFocusOrder() {
  // We want the infobar to come before the content pane, but after the bookmark
  // bar (if present) or top container (i.e. toolbar, again if present).
  if (bookmark_bar_view_ && bookmark_bar_view_->parent() == this)
    infobar_container_->InsertAfterInFocusList(bookmark_bar_view_.get());
  else if (top_container_->parent() == this)
    infobar_container_->InsertAfterInFocusList(top_container_);

  // We want the download shelf to come after the contents container (which also
  // contains the debug console, etc.) This prevents it from intruding into the
  // focus order, but makes it easily accessible by using SHIFT-TAB (reverse
  // focus traversal) from the toolbar/omnibox.
  if (download_shelf_ && contents_container_)
    download_shelf_->InsertAfterInFocusList(contents_container_);

#if DCHECK_IS_ON()
  // Make sure we didn't create any cycles in the focus order.
  CheckFocusListForCycles(top_container_);
#endif
}

bool BrowserView::CanChangeWindowIcon() const {
  // The logic of this function needs to be same as GetWindowIcon().
  if (browser_->is_type_devtools())
    return false;
  if (browser_->app_controller())
    return true;
#if defined(OS_CHROMEOS)
  // On ChromeOS, the tabbed browser always use a static image for the window
  // icon. See GetWindowIcon().
  if (browser_->is_type_normal())
    return false;
#endif
  return true;
}

views::View* BrowserView::GetInitiallyFocusedView() {
  return nullptr;
}

#if defined(OS_WIN)
bool BrowserView::CanShowWindowTitle() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR) ||
         WebUITabStripContainerView::SupportsTouchableTabStrip(browser());
}

bool BrowserView::CanShowWindowIcon() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}
#endif

bool BrowserView::ShouldShowWindowTitle() const {
#if defined(OS_CHROMEOS)
  // For Chrome OS only, trusted windows (apps and settings) do not show a
  // title, crbug.com/119411. Child windows (i.e. popups) do show a title.
  if (browser_->is_trusted_source())
    return false;
#elif defined(OS_WIN)
  // On Windows in touch mode we display a window title.
  if (WebUITabStripContainerView::UseTouchableTabStrip(browser()))
    return true;
#endif

  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

gfx::ImageSkia BrowserView::GetWindowAppIcon() {
  web_app::AppBrowserController* app_controller = browser()->app_controller();
  return app_controller ? app_controller->GetWindowAppIcon() : GetWindowIcon();
}

gfx::ImageSkia BrowserView::GetWindowIcon() {
  // Use the default icon for devtools.
  if (browser_->is_type_devtools())
    return gfx::ImageSkia();

  // Hosted apps always show their app icon.
  web_app::AppBrowserController* app_controller = browser()->app_controller();
  if (app_controller)
    return app_controller->GetWindowIcon();

#if defined(OS_CHROMEOS)
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (browser_->is_type_normal()) {
    return rb.GetImageNamed(IDR_CHROME_APP_ICON_192).AsImageSkia();
  }
  auto* window = GetNativeWindow();
  int override_window_icon_resource_id =
      window ? window->GetProperty(kOverrideWindowIconResourceIdKey) : -1;
  if (override_window_icon_resource_id >= 0)
    return rb.GetImageNamed(override_window_icon_resource_id).AsImageSkia();
#endif

  if (browser_->deprecated_is_app() || browser_->is_type_popup())
    return browser_->GetCurrentPageIcon().AsImageSkia();

  return gfx::ImageSkia();
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
    gfx::Rect saved_bounds = bounds;
    if (chrome::SavedBoundsAreContentBounds(browser_.get())) {
      // Invert the transformation done in GetSavedWindowPlacement().
      gfx::Size client_size =
          frame_->GetFrameView()->GetBoundsForClientView().size();
      if (IsToolbarVisible())
        client_size.Enlarge(0, -toolbar_->GetPreferredSize().height());
      saved_bounds.set_size(client_size);
    }
    chrome::SaveWindowPlacement(browser_.get(), saved_bounds, show_state);
  }
}

bool BrowserView::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  chrome::GetSavedWindowBoundsAndShowState(browser_.get(), bounds, show_state);
  // TODO(crbug.com/897300): Generalize this code for app and non-app popups?
  if (chrome::SavedBoundsAreContentBounds(browser_.get()) &&
      browser_->is_type_popup()) {
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

    gfx::Rect rect =
        frame_->non_client_view()->GetWindowBoundsForClientBounds(*bounds);
    rect.set_origin(bounds->origin());

    // When we are given x/y coordinates of 0 on a created popup window,
    // assume none were given by the window.open() command.
    if (rect.origin().IsOrigin())
      rect.set_origin(WindowSizer::GetDefaultPopupOrigin(rect.size()));

    // Constrain the final bounds to the target screen's available area. Bounds
    // enforcement applied earlier does not know the specific frame dimensions,
    // but generally yields bounds on the appropriate screen.
    auto display = display::Screen::GetScreen()->GetDisplayMatching(rect);
    rect.AdjustToFit(display.work_area());

    *bounds = rect;
    *show_state = ui::SHOW_STATE_NORMAL;
  }

  // We return true because we can _always_ locate reasonable bounds using the
  // WindowSizer, and we don't want to trigger the Window's built-in "size to
  // default" handling because the browser window has no default preferred size.
  return true;
}

views::View* BrowserView::GetContentsView() {
  return contents_web_view_;
}

views::ClientView* BrowserView::CreateClientView(views::Widget* widget) {
  return this;
}

views::View* BrowserView::CreateOverlayView() {
  overlay_view_ = new TopContainerOverlayView(weak_ptr_factory_.GetWeakPtr());
  overlay_view_->SetVisible(false);
  overlay_view_targeter_ = std::make_unique<OverlayViewTargeterDelegate>();
  overlay_view_->SetEventTargeter(
      std::make_unique<views::ViewTargeter>(overlay_view_targeter_.get()));
  return overlay_view_;
}

void BrowserView::OnWidgetDestroying(views::Widget* widget) {
  widget_observer_.Remove(widget);
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
    if (active) {
      if (restore_focus_on_activation_.has_value() &&
          restore_focus_on_activation_.value()) {
        restore_focus_on_activation_ = false;

        // Set initial focus change on the first activation if there is no
        // tab modal dialog.
        if (!IsShowingWebContentsModalDialog(GetActiveWebContents()))
          RestoreFocus();
      }

      BrowserList::SetLastActive(browser_.get());
    } else {
      BrowserList::NotifyBrowserNoLongerActive(browser_.get());
    }
  }

  if (!extension_keybinding_registry_ &&
      GetFocusManager()) {  // focus manager can be null in tests.
    extension_keybinding_registry_ =
        std::make_unique<ExtensionKeybindingRegistryViews>(
            browser_->profile(), GetFocusManager(),
            extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS, this);
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

void BrowserView::OnWidgetBoundsChanged(views::Widget* widget,
                                        const gfx::Rect& new_bounds) {
  TryNotifyWindowBoundsChanged(new_bounds);
}

void BrowserView::OnWindowBeginUserBoundsChange() {
  if (interactive_resize_in_progress_)
    return;
  WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return;
  interactive_resize_in_progress_ = true;
  web_contents->GetRenderViewHost()->NotifyMoveOrResizeStarted();
}

void BrowserView::OnWindowEndUserBoundsChange() {
  interactive_resize_in_progress_ = false;
  TryNotifyWindowBoundsChanged(GetWidget()->GetWindowBoundsInScreen());
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

void BrowserView::CreateTabSearchBubble() {
  // Do not spawn the bubble if using the WebUITabStrip.
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (WebUITabStripContainerView::UseTouchableTabStrip(browser_.get()))
    return;
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  DCHECK(GetTabSearchButton());
  if (GetTabSearchButton()->ShowTabSearchBubble()) {
    // Only log the open action if it resulted in creating a new instance of the
    // Tab Search bubble.
    base::UmaHistogramEnumeration("Tabs.TabSearch.OpenAction",
                                  TabSearchOpenAction::kKeyboardShortcut);
  }
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
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (webui_tab_strip_)
    panes->push_back(webui_tab_strip_);
#endif
  panes->push_back(toolbar_button_provider_->GetAsAccessiblePaneView());
  if (tab_strip_region_view_)
    panes->push_back(tab_strip_region_view_);
  if (toolbar_ && toolbar_->custom_tab_bar())
    panes->push_back(toolbar_->custom_tab_bar());
  if (bookmark_bar_view_.get())
    panes->push_back(bookmark_bar_view_.get());
  if (infobar_container_)
    panes->push_back(infobar_container_);
  if (download_shelf_)
    panes->push_back(download_shelf_);
// TODO(crbug.com/1055150): Implement for mac.
#if !defined(OS_MAC)
  // See if there is a caption bubble present.
  views::View* caption_bubble =
      captions::CaptionBubbleControllerViews::GetCaptionBubbleAccessiblePane(
          browser());
  if (caption_bubble)
    panes->push_back(caption_bubble);
#endif
  panes->push_back(contents_web_view_);
  if (devtools_web_view_->GetVisible())
    panes->push_back(devtools_web_view_);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::ClientView overrides:

views::CloseRequestResult BrowserView::OnWindowCloseRequested() {
  // You cannot close a frame for which there is an active originating drag
  // session.
  if (tabstrip_ && !tabstrip_->IsTabStripCloseable())
    return views::CloseRequestResult::kCannotClose;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return views::CloseRequestResult::kCannotClose;

  if (!browser_->tab_strip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    frame_->Hide();
    browser_->OnWindowClosing();
    return views::CloseRequestResult::kCannotClose;
  }

  return views::CloseRequestResult::kCanClose;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
  return GetBrowserViewLayout()->NonClientHitTest(point);
}

gfx::Size BrowserView::GetMinimumSize() const {
  return GetBrowserViewLayout()->GetMinimumSize(this);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::View overrides:

const char* BrowserView::GetClassName() const {
  return kViewClassName;
}

void BrowserView::Layout() {
  TRACE_EVENT0("ui", "BrowserView::Layout");
  if (!initialized_ || in_process_fullscreen_)
    return;

  // Allow only a single layout operation once top controls sliding begins.
  if (top_controls_slide_controller_ &&
      top_controls_slide_controller_->IsEnabled() &&
      top_controls_slide_controller_->IsTopControlsSlidingInProgress()) {
    if (did_first_layout_while_top_controls_are_sliding_)
      return;
    did_first_layout_while_top_controls_are_sliding_ = true;
  } else {
    did_first_layout_while_top_controls_are_sliding_ = false;
  }

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
  if (contents &&
      permissions::PermissionRequestManager::FromWebContents(contents)) {
    permissions::PermissionRequestManager::FromWebContents(contents)
        ->UpdateAnchorPosition();
  }

  feature_promo_controller_->UpdateBubbleForAnchorBoundsChange();
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
    const views::ViewHierarchyChangedDetails& details) {
  // Override here in order to suppress the call to
  // views::ClientView::ViewHierarchyChanged();
}

void BrowserView::AddedToWidget() {
  views::ClientView::AddedToWidget();

  widget_observer_.Add(GetWidget());

  // Stow a pointer to this object onto the window handle so that we can get at
  // it later when all we have is a native view.
  GetWidget()->SetNativeWindowProperty(kBrowserViewKey, this);

  // Stow a pointer to the browser's profile onto the window handle so that we
  // can get it later when all we have is a native view.
  GetWidget()->SetNativeWindowProperty(Profile::kProfileKey,
                                       browser_->profile());

#if defined(USE_AURA)
  // Stow a pointer to the browser's profile onto the window handle so
  // that windows will be styled with the appropriate NativeTheme.
  SetThemeProfileForWindow(GetNativeWindow(), browser_->profile());
#endif

  toolbar_->Init();

#if defined(OS_CHROMEOS)
  // TopControlsSlideController must be initialized here in AddedToWidget()
  // rather than Init() as it depends on the browser frame being ready.
  // It also needs to be after the |toolbar_| had been initialized since it uses
  // the omnibox.
  if (IsBrowserTypeNormal()) {
    DCHECK(frame_);
    DCHECK(toolbar_);
    top_controls_slide_controller_ =
        std::make_unique<TopControlsSlideControllerChromeOS>(this);
  }
#endif

  LoadAccelerators();

  // |immersive_mode_controller_| may depend on the presence of a Widget, so it
  // is initialized here.
  immersive_mode_controller_->Init(this);
  immersive_mode_controller_->AddObserver(this);

  // See https://crbug.com/993502.
  views::View* web_footer_experiment = nullptr;
  if (base::FeatureList::IsEnabled(features::kWebFooterExperiment)) {
    web_footer_experiment = AddChildView(
        std::make_unique<WebFooterExperimentView>(browser_->profile()));
  }

  // TODO(https://crbug.com/1036519): Remove BrowserViewLayout dependence on
  // Widget and move to the constructor.
  auto browser_view_layout = std::make_unique<BrowserViewLayout>(
      std::make_unique<BrowserViewLayoutDelegateImpl>(this),
      GetWidget()->GetNativeView(), this, top_container_,
      tab_strip_region_view_, tabstrip_, toolbar_, infobar_container_,
      contents_container_, immersive_mode_controller_.get(),
      web_footer_experiment, contents_separator_);
  SetLayoutManager(std::move(browser_view_layout));

  EnsureFocusOrder();

  // This browser view may already have a custom button provider set (e.g the
  // hosted app frame).
  if (!toolbar_button_provider_)
    SetToolbarButtonProvider(toolbar_);

  frame_->OnBrowserViewInitViewsComplete();
  frame_->GetFrameView()->UpdateMinimumSize();
  using_native_frame_ = frame_->ShouldUseNativeFrame();

  MaybeInitializeWebUITabStrip();

  feature_promo_controller_->feature_engagement_tracker()
      ->AddOnInitializedCallback(
          base::BindOnce(&BrowserView::OnFeatureEngagementTrackerInitialized,
                         weak_ptr_factory_.GetWeakPtr()));

  initialized_ = true;
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

void BrowserView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kClient;
}

void BrowserView::OnThemeChanged() {
  views::ClientView::OnThemeChanged();
  if (!initialized_)
    return;

  if (status_bubble_)
    status_bubble_->OnThemeChanged();

  MaybeShowInvertBubbleView(this);
}

bool BrowserView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  const bool parent_result =
      views::ClientView::GetDropFormats(formats, format_types);
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (webui_tab_strip_) {
    WebUITabStripContainerView::GetDropFormatsForView(formats, format_types);
    return true;
  } else {
    return parent_result;
  }
#else
  return parent_result;
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
}

bool BrowserView::AreDropTypesRequired() {
  return true;
}

bool BrowserView::CanDrop(const ui::OSExchangeData& data) {
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (!webui_tab_strip_)
    return false;
  return WebUITabStripContainerView::IsDraggedTab(data);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
}

void BrowserView::OnDragEntered(const ui::DropTargetEvent& event) {
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (!webui_tab_strip_)
    return;
  if (WebUITabStripContainerView::IsDraggedTab(event.data()))
    webui_tab_strip_->OpenForTabDrag();
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
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
  return chrome::ExecuteCommand(browser_.get(), command_id,
                                accelerator.time_stamp());
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, infobars::InfoBarContainer::Delegate overrides:

void BrowserView::InfoBarContainerStateChanged(bool is_animating) {
  ToolbarSizeChanged(is_animating);
}

void BrowserView::MaybeInitializeWebUITabStrip() {
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  TRACE_EVENT0("ui", "BrowserView::MaybeInitializeWebUITabStrip");
  if (browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP) &&
      WebUITabStripContainerView::UseTouchableTabStrip(browser_.get())) {
    if (!webui_tab_strip_) {
      // We use |contents_container_| here so that enabling or disabling
      // devtools won't affect the tab sizes. We still use only
      // |contents_web_view_| for screenshotting and will adjust the
      // screenshot accordingly. Ideally, the thumbnails should be sized
      // based on a typical tab size, ignoring devtools or e.g. the
      // downloads bar.
      webui_tab_strip_ = top_container_->AddChildView(
          std::make_unique<WebUITabStripContainerView>(
              this, contents_container_, top_container_,
              GetLocationBarView()->omnibox_view()));
      loading_bar_ = top_container_->AddChildView(
          std::make_unique<TopContainerLoadingBar>(browser_.get()));
      loading_bar_->SetWebContents(GetActiveWebContents());
    }
  } else if (webui_tab_strip_) {
    top_container_->RemoveChildView(webui_tab_strip_);
    delete webui_tab_strip_;
    webui_tab_strip_ = nullptr;

    top_container_->RemoveChildView(loading_bar_);
    delete loading_bar_;
    loading_bar_ = nullptr;
  }
  GetBrowserViewLayout()->set_webui_tab_strip(webui_tab_strip_);
  GetBrowserViewLayout()->set_loading_bar(loading_bar_);
  if (toolbar_)
    toolbar_->UpdateForWebUITabStrip();
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
}

void BrowserView::LoadingAnimationCallback() {
  if (CanSupportTabStrip()) {
    // Loading animations are shown in the tab for tabbed windows. Update them
    // even if the tabstrip isn't currently visible so they're in the right
    // state when it returns.
    tabstrip_->UpdateLoadingAnimations(base::TimeTicks::Now() -
                                       loading_animation_start_);
  }

  if (ShouldShowWindowIcon()) {
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
    bookmark_bar_view_ =
        std::make_unique<BookmarkBarView>(browser_.get(), this);
    bookmark_bar_view_->set_owned_by_client();
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
    new_parent = top_container_;
  }
  if (new_parent != bookmark_bar_view_->parent()) {
    if (new_parent == top_container_) {
      // BookmarkBarView is attached.
      new_parent->AddChildView(bookmark_bar_view_.get());
    } else {
      DCHECK(!new_parent);
      // Bookmark bar is being detached from all views because it is hidden.
      bookmark_bar_view_->parent()->RemoveChildView(bookmark_bar_view_.get());
    }
    needs_layout = true;
  }

  // Check for updates to the desired size.
  if (bookmark_bar_view_->GetPreferredSize().height() !=
      bookmark_bar_view_->height())
    needs_layout = true;

  return needs_layout;
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
  TRACE_EVENT0("ui", "BrowserView::UpdateUIForContents");
  bool needs_layout = MaybeShowBookmarkBar(contents);
  // TODO(jamescook): This function always returns true. Remove it and figure
  // out when layout is actually required.
  needs_layout |= MaybeShowInfoBar(contents);
  if (needs_layout)
    Layout();
}

void BrowserView::ProcessFullscreen(bool fullscreen,
                                    const GURL& url,
                                    ExclusiveAccessBubbleType bubble_type,
                                    const int64_t display_id) {
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

  // Toggle fullscreen mode; move the window between displays as needed.
  // TODO(crbug.com/1034783): Implement at lower layers to avoid transitions.
  if (fullscreen && display_id != display::kInvalidDisplayId) {
    display::Screen* screen = display::Screen::GetScreen();
    display::Display display;
    if (screen && screen->GetDisplayWithDisplayId(display_id, &display)) {
      const gfx::Rect& current_bounds = frame_->GetWindowBoundsInScreen();
      restore_pre_fullscreen_bounds_callback_ = base::BindOnce(
          [](base::WeakPtr<BrowserView> view, const gfx::Rect& bounds) {
            if (view && view->frame())
              view->frame()->SetBounds(bounds);
          },
          weak_ptr_factory_.GetWeakPtr(), current_bounds);
      frame_->SetBounds({display.work_area().origin(), current_bounds.size()});
    }
  }

  frame_->SetFullscreen(fullscreen);

#if !defined(OS_MAC)
  // On Mac, the pre-fullscreen bounds must be restored after an asynchronous
  // transition out of the fullscreen workspace; see http://crbug.com/1039874
  if (!fullscreen && restore_pre_fullscreen_bounds_callback_)
    std::move(restore_pre_fullscreen_bounds_callback_).Run();
#endif  // !OS_MAC

  // Enable immersive before the browser refreshes its list of enabled commands.
  const bool should_stay_in_immersive =
      !fullscreen &&
      immersive_mode_controller_->ShouldStayImmersiveAfterExitingFullscreen();
  // Never use immersive in locked fullscreen as it allows the user to exit the
  // locked mode.
  if (platform_util::IsBrowserLockedFullscreen(browser_.get())) {
    immersive_mode_controller_->SetEnabled(false);
  } else if (ShouldUseImmersiveFullscreenForUrl(url) &&
             !should_stay_in_immersive) {
    immersive_mode_controller_->SetEnabled(fullscreen);
  }

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
  if (chrome::IsRunningInAppMode())
    return false;
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
    if (is_app_mode && !chrome::IsCommandAllowedInAppMode(
                           entry.command_id, browser()->is_type_popup()))
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

  if (command_id == IDC_BOOKMARK_THIS_TAB) {
    UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                              BOOKMARK_ENTRY_POINT_ACCELERATOR,
                              BOOKMARK_ENTRY_POINT_LIMIT);
  }
  if (base::FeatureList::IsEnabled(features::kTabGroups) &&
      command_id == IDC_NEW_TAB &&
      browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP)) {
    TabStripModel* const model = browser_->tab_strip_model();
    const auto group_id = model->GetTabGroupForTab(model->active_index());
    if (group_id.has_value())
      base::RecordAction(base::UserMetricsAction("Accel_NewTabInGroup"));
  }

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

void BrowserView::ShowAvatarBubbleFromAvatarButton(
    AvatarBubbleMode mode,
    signin_metrics::AccessPoint access_point,
    bool focus_first_profile_button) {
  // Do not show avatar bubble if there is no avatar menu button.
  views::Button* avatar_button =
      toolbar_button_provider_->GetAvatarToolbarButton();
  if (!avatar_button)
    return;

  GetAutofillBubbleHandler()->HideSignInPromo();

  profiles::BubbleViewMode bubble_view_mode;
  profiles::BubbleViewModeFromAvatarBubbleMode(mode, GetProfile(),
                                               &bubble_view_mode);
#if !defined(OS_CHROMEOS)
  if (SigninViewController::ShouldShowSigninForMode(bubble_view_mode)) {
    browser_->signin_view_controller()->ShowSignin(bubble_view_mode,
                                                   access_point);
    return;
  }
#endif
  ProfileMenuViewBase::ShowBubble(bubble_view_mode, avatar_button, browser(),
                                  focus_first_profile_button);
}

void BrowserView::ShowHatsBubble(const std::string& site_id) {
  HatsBubbleView::ShowOnContentReady(browser(), site_id);
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

std::string BrowserView::GetWorkspace() const {
  return frame_->GetWorkspace();
}

bool BrowserView::IsVisibleOnAllWorkspaces() const {
  return frame_->IsVisibleOnAllWorkspaces();
}

void BrowserView::ShowEmojiPanel() {
  GetWidget()->ShowEmojiPanel();
}

void BrowserView::ShowCaretBrowsingDialog() {
  CaretBrowsingDialogDelegate::Show(GetNativeWindow(),
                                    GetProfile()->GetPrefs());
}

std::unique_ptr<content::EyeDropper> BrowserView::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return ShowEyeDropper(frame, listener);
}

void BrowserView::ShowInProductHelpPromo(InProductHelpFeature iph_feature) {
  switch (iph_feature) {
    case InProductHelpFeature::kIncognitoWindow:
      break;
    case InProductHelpFeature::kReopenTab:
      reopen_tab_promo_controller_.ShowPromo();
      break;
  }
}

FeaturePromoController* BrowserView::GetFeaturePromoController() {
  return feature_promo_controller_.get();
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
  javascript_dialogs::AppModalDialogController* active_dialog =
      javascript_dialogs::AppModalDialogQueue::GetInstance()->active_dialog();
  if (!active_dialog)
    return;

  Browser* modal_browser =
      chrome::FindBrowserWithWebContents(active_dialog->web_contents());
  if (modal_browser && (browser_.get() != modal_browser)) {
    modal_browser->window()->FlashFrame(true);
    modal_browser->window()->Activate();
  }

  javascript_dialogs::AppModalDialogQueue::GetInstance()->ActivateModalDialog();
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

void BrowserView::ObserveAppBannerManager(
    banners::AppBannerManager* new_manager) {
  app_banner_manager_observer_.RemoveAll();
  app_banner_manager_observer_.Add(new_manager);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ExclusiveAccessContext implementation:
Profile* BrowserView::GetProfile() {
  return browser_->profile();
}

void BrowserView::UpdateUIForTabFullscreen() {
  frame()->GetFrameView()->UpdateFullscreenTopUI();
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
content::WebContents* BrowserView::GetWebContentsForExtension() {
  return GetActiveWebContents();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ImmersiveModeController::Observer implementation:
void BrowserView::OnImmersiveRevealStarted() {
  AppMenuButton* app_menu_button =
      toolbar_button_provider()->GetAppMenuButton();
  if (app_menu_button)
    app_menu_button->CloseMenu();

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

///////////////////////////////////////////////////////////////////////////////
// BrowserView, banners::AppBannerManager::Observer implementation:
void BrowserView::OnInstallableWebAppStatusUpdated() {
  UpdatePageActionIcon(PageActionIconType::kPwaInstall);
}
