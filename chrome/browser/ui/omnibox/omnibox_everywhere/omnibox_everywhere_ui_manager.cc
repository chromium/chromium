// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_region_select_overlay.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider_key.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/view_type_utils.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "ui/views/win/hwnd_util.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_event_handler_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/window_animations.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_shortcut_win.h"
#endif

namespace omnibox_everywhere {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OmniboxEverywhereUIManager,
                                      kOmniboxEverywhereElementId);

namespace {

class OmniboxEverywhereFileSelectListener : public content::FileSelectListener {
 public:
  OmniboxEverywhereFileSelectListener(
      base::WeakPtr<OmniboxEverywhereUIManager> ui_manager,
      scoped_refptr<content::FileSelectListener> listener)
      : ui_manager_(ui_manager), listener_(std::move(listener)) {
    if (ui_manager_) {
      ui_manager_->OnFileChooserOpened();
    }
  }

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {
    if (!selection_handled_) {
      selection_handled_ = true;
      if (ui_manager_) {
        ui_manager_->OnFileChooserClosed();
      }
    }
    listener_->FileSelected(std::move(files), base_dir, mode);
  }

  void FileSelectionCanceled() override {
    if (!selection_handled_) {
      selection_handled_ = true;
      if (ui_manager_) {
        ui_manager_->OnFileChooserClosed();
      }
    }
    listener_->FileSelectionCanceled();
  }

 protected:
  ~OmniboxEverywhereFileSelectListener() override {
    if (!selection_handled_ && ui_manager_) {
      ui_manager_->OnFileChooserClosed();
    }
  }

 private:
  base::WeakPtr<OmniboxEverywhereUIManager> ui_manager_;
  scoped_refptr<content::FileSelectListener> listener_;
  bool selection_handled_ = false;
};

SkRegion ComputeDraggableRegion(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions) {
  SkRegion draggable_region;
  // First, union all draggable background areas.
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    if (region->draggable) {
      draggable_region.op(
          SkIRect::MakeXYWH(region->bounds.x(), region->bounds.y(),
                            region->bounds.width(), region->bounds.height()),
          SkRegion::kUnion_Op);
    }
  }
  // Next, subtract non-draggable regions so they take precedence over DOM
  // order.
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    if (!region->draggable) {
      draggable_region.op(
          SkIRect::MakeXYWH(region->bounds.x(), region->bounds.y(),
                            region->bounds.width(), region->bounds.height()),
          SkRegion::kDifference_Op);
    }
  }
  return draggable_region;
}

}  // namespace

OmniboxEverywhereUIManager::OmniboxEverywhereUIManager(
    ContentsWrapperFactory contents_wrapper_factory)
    : contents_wrapper_factory_(std::move(contents_wrapper_factory)),
      unhandled_keyboard_event_handler_(
          std::make_unique<views::UnhandledKeyboardEventHandler>()) {
#if defined(USE_AURA)
  event_handler_ = std::make_unique<OmniboxEverywhereEventHandlerAura>(*this);
#endif
  if (g_browser_process && g_browser_process->local_state()) {
    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
    local_state_pref_change_registrar_.Add(
        prefs::kOmniboxEverywhereEphemeralModel,
        base::BindRepeating(
            &OmniboxEverywhereUIManager::OnEphemeralModelPrefChanged,
            base::Unretained(this)));
    local_state_pref_change_registrar_.Add(
        prefs::kOmniboxEverywhereShowShortcuts,
        base::BindRepeating(
            &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
            base::Unretained(this)));
  }
}

OmniboxEverywhereUIManager::~OmniboxEverywhereUIManager() {
  CleanUpWidget();
}

OmniboxEverywhereWidgetDelegate* OmniboxEverywhereUIManager::widget_delegate() {
  return widget_delegate_.get();
}

const OmniboxEverywhereWidgetDelegate*
OmniboxEverywhereUIManager::widget_delegate() const {
  return widget_delegate_.get();
}

bool OmniboxEverywhereUIManager::IsPointInDraggableRegion(
    const gfx::Point& point) const {
  // TODO(b/546065055): There's additional padding on the widget to support the
  // shadow, which is draggable. This should be addressed to prevent dragging
  // this area.
  return draggable_region_ && !draggable_region_->isEmpty() &&
         draggable_region_->contains(point.x(), point.y());
}

content::WebContents* OmniboxEverywhereUIManager::web_contents() const {
  return contents_wrapper_ ? contents_wrapper_->web_contents() : nullptr;
}

void OmniboxEverywhereUIManager::ShowForProfile(Profile* profile,
                                                gfx::NativeWindow context) {
  deactivation_task_.Cancel();
  last_shown_time_ = base::TimeTicks::Now();
  if (widget_ && profile_ == profile) {
    ActivateAndFocus();
    return;
  }

  if (widget_) {
    // If a different profile is present, clean up first.
    CleanUpWidget();
  }

  // Ensure any previous browser collection observation is cleanly reset if the
  // active profile is changing.
  if (profile_ != profile) {
    browser_collection_observation_.Reset();
    profile_pref_change_registrar_.Reset();
    if (profile && profile->GetPrefs()) {
      profile_pref_change_registrar_.Init(profile->GetPrefs());
      profile_pref_change_registrar_.Add(
          ntp_prefs::kNtpCustomLinksVisible,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
      profile_pref_change_registrar_.Add(
          ntp_prefs::kNtpEnterpriseShortcutsVisible,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
      // TODO(crbug.com/546555215): Only register this if omnibox everywhere
      // MVT pref is not set.
      profile_pref_change_registrar_.Add(
          ntp_prefs::kNtpPersonalShortcutsVisible,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
      profile_pref_change_registrar_.Add(
          ntp_prefs::kNtpShortcutsVisible,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
      profile_pref_change_registrar_.Add(
          ntp_tiles::prefs::kCustomLinksList,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
      profile_pref_change_registrar_.Add(
          ntp_tiles::prefs::kCustomLinksInitialized,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
      profile_pref_change_registrar_.Add(
          ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
    }
  }
  profile_ = profile;

  EnsureContentsWrapperInitialized(profile_);
  CreateAndInitWidget(context);
  ActivateAndFocus();

  if (web_contents()) {
    if (auto* rwhv = web_contents()->GetRenderWidgetHostView()) {
      constexpr gfx::Size kAutoResizeMinSize(kPopupFixedWidth, 50);
      constexpr gfx::Size kAutoResizeMaxSize(kPopupFixedWidth, 800);
      rwhv->EnableAutoResize(kAutoResizeMinSize, kAutoResizeMaxSize);
    }
  }
}

gfx::Rect OmniboxEverywhereUIManager::CalculateWidgetBounds(int height) {
  display::Display target_display =
      display::Screen::Get()->GetDisplayNearestPoint(
          display::Screen::Get()->GetCursorScreenPoint());
  gfx::Rect work_area = target_display.work_area();
  int width = std::min(kPopupFixedWidth, work_area.width());
  int clamped_height = std::min(height, work_area.height());
  int x = work_area.x() + (work_area.width() - width) / 2;
  int y = work_area.y() + (work_area.height() - clamped_height) / 2;
  return gfx::Rect(x, y, width, clamped_height);
}

void OmniboxEverywhereUIManager::EnsureContentsWrapperInitialized(
    Profile* profile) {
  if (contents_wrapper_) {
    return;
  }
  contents_wrapper_ = CreateContentsWrapper(profile);

  if (web_contents()) {
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        web_contents(), SK_ColorTRANSPARENT);
    OmniboxPopupWebContentsHelper::CreateForWebContents(web_contents());
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    // Set ViewType::kComponent so `ChromeSpeechRecognitionManagerDelegate`
    // allows speech recognition in `CheckRenderFrameType()`.
    extensions::SetViewType(web_contents(),
                            extensions::mojom::ViewType::kComponent);
#endif
    // Create PermissionRequestManager explicitly for this WebContents.
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  }

  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());

  // Since the Omnibox Everywhere widget is a standalone popup without a
  // native browser window, in order to support Google Drive picker
  // integration, we need to manually set the BrowserWindowInterface for the
  // WebContents.
  ProfileBrowserCollection* profile_collection =
      ProfileBrowserCollection::GetForProfile(profile);
  CHECK(profile_collection);
  BrowserWindowInterface* active_bwi =
      profile_collection->GetLastActiveBrowser();
  if (active_bwi && web_contents()) {
    webui::SetBrowserWindowInterface(web_contents(), active_bwi);
  }
  browser_collection_observation_.Observe(profile_collection);
}

bool OmniboxEverywhereUIManager::AcquireKeepAlives() {
  KeepAliveRegistry* const keep_alive_registry =
      KeepAliveRegistry::GetInstance();
  if (!keep_alive_registry || keep_alive_registry->IsShuttingDown()) {
    return false;
  }
  if (!keep_alive_) {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::OMNIBOX_EVERYWHERE_UI,
        KeepAliveRestartOption::DISABLED);
  }

  auto* service = OmniboxEverywhereServiceFactory::GetForProfile(profile_);
  CHECK(service);

  if (!service->AcquireProfileKeepAlive()) {
    return false;
  }

  return true;
}

bool OmniboxEverywhereUIManager::TryAcquireKeepAlives() {
  if (AcquireKeepAlives()) {
    return true;
  }

  ReleaseKeepAlives();
  return false;
}

void OmniboxEverywhereUIManager::ReleaseKeepAlives() {
  if (profile_) {
    if (auto* service =
            OmniboxEverywhereServiceFactory::GetForProfile(profile_)) {
      service->ReleaseProfileKeepAlive();
    }
  }
  keep_alive_.reset();
}

void OmniboxEverywhereUIManager::CreateAndInitWidget(
    gfx::NativeWindow context) {
  if (widget_) {
    return;
  }

  if (!TryAcquireKeepAlives()) {
    return;
  }

  widget_ = std::make_unique<views::Widget>();

  // TODO(crbug.com/542731882): Remove once dark mode for loomnibox is
  // implemented.
  widget_->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kLight);

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.remove_standard_frame = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  bool is_ephemeral = prefs::IsEphemeralModelEnabled();
#if BUILDFLAG(IS_WIN)
  params.dont_show_in_taskbar = is_ephemeral;
#endif  // BUILDFLAG(IS_WIN)
  widget_delegate_ = std::make_unique<OmniboxEverywhereWidgetDelegate>();
  if (draggable_region_) {
    widget_delegate_->SetDraggableRegion(draggable_region_);
  }
  params.delegate = widget_delegate_.get();
  params.z_order = is_ephemeral ? ui::ZOrderLevel::kFloatingWindow
                                : ui::ZOrderLevel::kNormal;
  if (context) {
    params.context = context;
  }

  params.bounds = CalculateWidgetBounds(kDefaultRestingHeight);

  auto web_view = std::make_unique<views::WebView>(profile_);
  web_view->SetProperty(views::kElementIdentifierKey,
                        kOmniboxEverywhereElementId);
  web_view->set_context_menu_controller(this);
  // Allow the WebContents host to route unhandled accelerator keys through
  // the Views focus/accelerator system.
  web_view->set_allow_accelerators(true);
  web_view->SetWebContents(web_contents());
  widget_delegate_->SetContentsView(std::move(web_view));

  widget_->Init(std::move(params));
#if BUILDFLAG(IS_MAC)
  widget_->SetActivationIndependence(is_ephemeral);
  widget_->SetVisibleOnAllWorkspaces(true);
  widget_->SetCanAppearInExistingFullscreenSpaces(true);
#endif
#if BUILDFLAG(IS_WIN)
  SetWindowProperties(views::HWNDForWidget(widget_.get()), is_ephemeral);
#endif  // BUILDFLAG(IS_WIN)
  widget_->MakeCloseSynchronous(base::BindOnce(
      &OmniboxEverywhereUIManager::OnWidgetClosed, base::Unretained(this)));
  widget_observation_.Observe(widget_.get());

  CHECK(widget_->non_client_view() && widget_->non_client_view()->frame_view());
  widget_->non_client_view()->frame_view()->set_non_client_hit_test_callback(
      base::BindRepeating(&OmniboxEverywhereWidgetDelegate::NonClientHitTest,
                          base::Unretained(widget_delegate_.get())));

#if defined(USE_AURA)
  CHECK(widget_->GetNativeView());
  wm::SetWindowVisibilityAnimationTransition(widget_->GetNativeView(),
                                             wm::ANIMATE_NONE);
  widget_->GetNativeView()->AddPreTargetHandler(event_handler_.get());
#endif
}

void OmniboxEverywhereUIManager::ActivateAndFocus() {
  if (!widget_) {
    return;
  }

  if (!TryAcquireKeepAlives()) {
    return;
  }

  is_demoted_ = false;
  widget_->Show();
  widget_->Activate();

  if (widget_->GetContentsView()) {
    widget_->GetContentsView()->RequestFocus();
  }
  if (web_contents()) {
    web_contents()->Focus();
  }

  if (profile_) {
    if (auto* service =
            OmniboxEverywhereServiceFactory::GetForProfile(profile_)) {
      service->MaybeShowLensPromo();
    }
  }
}

void OmniboxEverywhereUIManager::OnEphemeralModelPrefChanged() {
  if (!widget_) {
    return;
  }
  bool was_visible = IsVisible();
  Profile* profile = profile_;
  CleanUpWidget();
  if (was_visible) {
    CHECK(profile);
    ShowForProfile(profile);
  }
}

void OmniboxEverywhereUIManager::OnMostVisitedPrefChanged() {
  if (!widget_ || IsVisible()) {
    return;
  }
  // Clean up the widget when the pref changes so there is not flicker when the
  // omnibox is re-invoked.
  CleanUpWidget();
}

void OmniboxEverywhereUIManager::RecordFreImpression() {
  if (!profile_ || !profile_->GetPrefs() ||
      !base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre)) {
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  if (prefs->GetBoolean(omnibox_everywhere::prefs::kFreDismissed)) {
    return;
  }

  int impressions =
      prefs->GetInteger(omnibox_everywhere::prefs::kFreImpressionCount) + 1;
  prefs->SetInteger(omnibox_everywhere::prefs::kFreImpressionCount,
                    impressions);
  if (impressions >= omnibox_everywhere::prefs::kMaxFreImpressions) {
    prefs->SetBoolean(omnibox_everywhere::prefs::kFreDismissed, true);
  }
}

void OmniboxEverywhereUIManager::Close() {
  RecordFreImpression();
  last_shown_time_.reset();
  deactivation_task_.Cancel();
  if (widget_) {
    if (HasOpenModalDialog()) {
      CleanUpWidget();
      return;
    }
    if (is_context_menu_open_ && context_menu_runner_) {
      context_menu_runner_->Cancel();
      is_context_menu_open_ = false;
    }
    widget_->Hide();
  }
  ReleaseKeepAlives();
}

void OmniboxEverywhereUIManager::Demote() {
  last_shown_time_.reset();
  deactivation_task_.Cancel();
  if (!widget_ || !widget_->IsVisible() || is_demoted_ ||
      HasOpenModalDialog()) {
    return;
  }
  if (is_context_menu_open_ && context_menu_runner_) {
    context_menu_runner_->Cancel();
    is_context_menu_open_ = false;
  }
  is_demoted_ = true;
  widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  // Deactivate only if the widget is currently active to avoid deactivating
  // other windows in the application.
  if (widget_->IsActive()) {
    widget_->Deactivate();
  }
  // TODO(b/532195081): Add support for macOS to demote/send the widget to the
  // background in persistent mode.
#if BUILDFLAG(IS_WIN)
  HWND hwnd = views::HWNDForWidget(widget_.get());
  if (hwnd) {
    ::SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
#endif
}

void OmniboxEverywhereUIManager::CleanUpWidget() {
  deactivation_task_.Cancel();
  if (widget_) {
    widget_observation_.Reset();
#if defined(USE_AURA)
    if (auto* native_view = widget_->GetNativeView()) {
      native_view->RemovePreTargetHandler(event_handler_.get());
    }
#endif
    if (auto* contents_view = widget_delegate_->GetContentsView()) {
      if (auto* web_view = views::AsViewClass<views::WebView>(contents_view)) {
        web_view->SetWebContents(nullptr);
      }
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<views::Widget> widget,
               std::unique_ptr<OmniboxEverywhereWidgetDelegate> delegate) {
              widget.reset();
              delegate.reset();
            },
            std::move(widget_), std::move(widget_delegate_)));
  }
  contents_wrapper_.reset();
  if (context_menu_runner_) {
    context_menu_runner_->Cancel();
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(context_menu_runner_));
  }
  if (context_menu_model_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(context_menu_model_));
  }
  last_context_menu_params_ = content::ContextMenuParams();
  is_file_chooser_open_ = false;
  is_drive_picker_open_ = false;
  is_context_menu_open_ = false;
  is_demoted_ = false;
  is_screenshare_picker_open_ = false;
  is_dragging_ = false;
  pending_auto_resize_size_.reset();
  draggable_region_.reset();
  region_select_overlay_.reset();
  browser_collection_observation_.Reset();
  last_shown_time_.reset();
  ReleaseKeepAlives();
}

void OmniboxEverywhereUIManager::Shutdown() {
  deactivation_task_.Cancel();
  last_shown_time_.reset();
  browser_collection_observation_.Reset();
  profile_pref_change_registrar_.Reset();
  CleanUpWidget();
  profile_ = nullptr;
}

bool OmniboxEverywhereUIManager::IsVisible() const {
  return widget_ && widget_->IsVisible();
}

bool OmniboxEverywhereUIManager::IsActive() const {
  return widget_ && widget_->IsActive() && !is_demoted_;
}

bool OmniboxEverywhereUIManager::HasOpenModalDialog() const {
  return is_file_chooser_open_ || is_drive_picker_open_ ||
         is_screenshare_picker_open_ || region_select_overlay_ != nullptr;
}

void OmniboxEverywhereUIManager::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (active) {
    is_demoted_ = false;
    return;
  }
  if (!active && !HasOpenModalDialog() && !is_context_menu_open_ &&
      prefs::IsEphemeralModelEnabled()) {
    HandleWidgetDeactivated();
  }
}

void OmniboxEverywhereUIManager::OnContextMenuClosed() {
  is_context_menu_open_ = false;
  if (context_menu_runner_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(context_menu_runner_));
  }
  if (context_menu_model_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(context_menu_model_));
  }
  if (widget_ && !widget_->IsActive() && !HasOpenModalDialog() &&
      prefs::IsEphemeralModelEnabled()) {
    HandleWidgetDeactivated();
  }
}

void OmniboxEverywhereUIManager::HandleWidgetDeactivated() {
  if (!widget_ || !widget_->IsVisible() || !prefs::IsEphemeralModelEnabled()) {
    return;
  }
  if (last_shown_time_.has_value() &&
      base::TimeTicks::Now() - *last_shown_time_ < kActivationGracePeriod) {
    deactivation_task_.Reset(
        base::BindOnce(&OmniboxEverywhereUIManager::ActivateAndFocus,
                       weak_factory_.GetWeakPtr()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, deactivation_task_.callback());
    return;
  }
  deactivation_task_.Reset(base::BindOnce(&OmniboxEverywhereUIManager::Close,
                                          weak_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, deactivation_task_.callback());
}

void OmniboxEverywhereUIManager::OnWidgetDestroying(views::Widget* widget) {
  CleanUpWidget();
}

void OmniboxEverywhereUIManager::OnWidgetClosed(
    views::Widget::ClosedReason reason) {
  CleanUpWidget();
}

void OmniboxEverywhereUIManager::OnWidgetUserDragStarted(
    views::Widget* widget) {
  is_dragging_ = true;
}

void OmniboxEverywhereUIManager::OnWidgetUserDragEnded(views::Widget* widget) {
  is_dragging_ = false;
  if (pending_auto_resize_size_.has_value()) {
    gfx::Size size = *pending_auto_resize_size_;
    pending_auto_resize_size_.reset();
    ResizeDueToAutoResize(web_contents(), size);
  }
}

void OmniboxEverywhereUIManager::CloseUI() {
  if (prefs::IsEphemeralModelEnabled()) {
    Close();
  } else {
    Demote();
  }
}

void OmniboxEverywhereUIManager::ShowUI() {
  ActivateAndFocus();
}

void OmniboxEverywhereUIManager::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& new_size) {
  if (!widget_) {
    return;
  }
  if (is_dragging_) {
    pending_auto_resize_size_ = new_size;
    return;
  }
  constexpr int kAutoResizeMinHeight = 56;
  gfx::Size target_size(kPopupFixedWidth,
                        std::max(new_size.height(), kAutoResizeMinHeight));
  if (widget_->GetSize() != target_size) {
    widget_->SetSize(target_size);
  }
}

void OmniboxEverywhereUIManager::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

void OmniboxEverywhereUIManager::OnFileChooserOpened() {
  is_file_chooser_open_ = true;
}

void OmniboxEverywhereUIManager::OnFileChooserClosed() {
  is_file_chooser_open_ = false;
}

void OmniboxEverywhereUIManager::OnDrivePickerOpened() {
  is_drive_picker_open_ = true;
}

void OmniboxEverywhereUIManager::OnDrivePickerClosed() {
  is_drive_picker_open_ = false;
}

void OmniboxEverywhereUIManager::OnScreensharePickerOpened() {
  is_screenshare_picker_open_ = true;
  if (widget_) {
    widget_->Hide();
  }
}

void OmniboxEverywhereUIManager::OnScreensharePickerClosed() {
  is_screenshare_picker_open_ = false;
  if (widget_) {
    ActivateAndFocus();
  }
}

void OmniboxEverywhereUIManager::ShowRegionSelectOverlay(
    const SkBitmap& screenshot,
    const RegionCaptureSource& source,
    RegionSelectedCallback callback) {
  if (region_select_overlay_) {
    auto old_overlay = std::move(region_select_overlay_);
    old_overlay.reset();
  }
  gfx::NativeWindow context =
      widget_ ? widget_->GetNativeWindow() : gfx::NativeWindow();
  region_select_overlay_ = OmniboxEverywhereRegionSelectOverlay::Create(
      screenshot, source,
      base::BindOnce(&OmniboxEverywhereUIManager::OnRegionSelectOverlayClosed,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      context);
}

void OmniboxEverywhereUIManager::OnRegionSelectOverlayClosed(
    RegionSelectedCallback callback,
    const SkBitmap& result_bitmap) {
  if (region_select_overlay_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(region_select_overlay_));
  }
  std::move(callback).Run(result_bitmap);
}

void OmniboxEverywhereUIManager::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  if (web_contents()) {
    webui::SetBrowserWindowInterface(web_contents(), browser);
  }
}

void OmniboxEverywhereUIManager::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (web_contents()) {
    if (webui::GetBrowserWindowInterface(web_contents()) == browser) {
      BrowserWindowInterface* active_bwi = nullptr;
      // Get the profile collection from the scoped observation object directly
      // rather than performing a manual lookup on the profile pointer.
      if (browser_collection_observation_.IsObserving()) {
        ProfileBrowserCollection* profile_collection =
            browser_collection_observation_.GetSource();
        CHECK(profile_collection);
        active_bwi = profile_collection->GetLastActiveBrowser();
      }
      webui::SetBrowserWindowInterface(web_contents(), active_bwi);
    }
  }
}

content::WebContents* OmniboxEverywhereUIManager::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  auto* service = OmniboxEverywhereServiceFactory::GetForProfile(profile_);
  if (service) {
    service->OpenUrl(params.url, params.disposition, params.transition);
  }
  return nullptr;
}

void OmniboxEverywhereUIManager::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  auto wrapped_listener =
      base::MakeRefCounted<OmniboxEverywhereFileSelectListener>(
          weak_factory_.GetWeakPtr(), std::move(listener));
  FileSelectHelper::RunFileChooser(render_frame_host,
                                   std::move(wrapped_listener), params);
}

void OmniboxEverywhereUIManager::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* /*contents*/) {
  draggable_region_ = ComputeDraggableRegion(regions);
  if (widget_delegate_) {
    widget_delegate_->SetDraggableRegion(draggable_region_);
  }
}

// WebUIContentsWrapper::Host:
// Omnibox Everywhere is a standalone popup WebUI window without a default
// browser-frame context menu controller. HandleContextMenu creates and displays
// a lightweight context menu for standard text editing actions (Cut, Copy,
// Paste, Select All) exclusively for editable controls (query input) or
// selected text. Non-editable background/padding areas suppress the context
// menu.
bool OmniboxEverywhereUIManager::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  if (!widget_ || !widget_->GetContentsView()) {
    return true;
  }

  // Cancel and clean up any existing context menu before creating a new one.
  if (context_menu_runner_) {
    context_menu_runner_->Cancel();
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(context_menu_runner_));
  }
  if (context_menu_model_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(context_menu_model_));
  }

  last_context_menu_params_ = params;
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  // 1. Query input box / editable text
  if (params.is_editable) {
    BuildInputContextMenu(params);
    // 2. Highlighted text
  } else if (!params.selection_text.empty()) {
    BuildSelectionContextMenu(params);
    // 3. Non-editable background / anywhere else on the widget
  } else {
    BuildBackgroundContextMenu(params);
  }

  if (context_menu_model_->GetItemCount() == 0) {
    return true;
  }

  is_context_menu_open_ = true;
  auto on_closed_callback =
      base::BindRepeating(&OmniboxEverywhereUIManager::OnContextMenuClosed,
                          weak_factory_.GetWeakPtr());
  if (menu_runner_factory_) {
    context_menu_runner_ =
        menu_runner_factory_.Run(context_menu_model_.get(), on_closed_callback);
  } else {
    context_menu_runner_ = std::make_unique<views::MenuRunner>(
        context_menu_model_.get(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
        on_closed_callback);
  }

  gfx::Point screen_point(params.x, params.y);
  views::View::ConvertPointToScreen(widget_->GetContentsView(), &screen_point);

  context_menu_runner_->RunMenuAt(
      widget_.get(), nullptr, gfx::Rect(screen_point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, params.source_type);
  return true;
}

void OmniboxEverywhereUIManager::BuildInputContextMenu(
    const content::ContextMenuParams& params) {
  context_menu_model_->AddItemWithStringId(kUndo, IDS_APP_UNDO);
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

  context_menu_model_->AddItemWithStringId(kCut, IDS_APP_CUT);
  context_menu_model_->AddItemWithStringId(kCopy, IDS_APP_COPY);
  context_menu_model_->AddItemWithStringId(kPaste, IDS_APP_PASTE);
  context_menu_model_->AddItemWithStringId(kPasteAndSearch,
                                           IDS_PASTE_AND_GO_EMPTY);
  context_menu_model_->AddItemWithStringId(kDelete, IDS_APP_DELETE);
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

  context_menu_model_->AddItemWithStringId(kSelectAll, IDS_APP_SELECT_ALL);
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  AppendSettingsContextMenu();
}

void OmniboxEverywhereUIManager::BuildSelectionContextMenu(
    const content::ContextMenuParams& params) {
  if (params.is_editable) {
    context_menu_model_->AddItemWithStringId(kUndo, IDS_APP_UNDO);
    context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_model_->AddItemWithStringId(kCut, IDS_APP_CUT);
  }
  context_menu_model_->AddItemWithStringId(kCopy, IDS_APP_COPY);
  if (params.is_editable) {
    context_menu_model_->AddItemWithStringId(kPaste, IDS_APP_PASTE);
    context_menu_model_->AddItemWithStringId(kDelete, IDS_APP_DELETE);
  }
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  context_menu_model_->AddItemWithStringId(kSelectAll, IDS_APP_SELECT_ALL);
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  AppendSettingsContextMenu();
}

void OmniboxEverywhereUIManager::BuildBackgroundContextMenu(
    const content::ContextMenuParams& params) {
  AppendSettingsContextMenu();
}

void OmniboxEverywhereUIManager::AppendSettingsContextMenu() {
  context_menu_model_->AddItemWithStringId(
      kManageSearchEngines,
      base::FeatureList::IsEnabled(switches::kSearchSettingsUpdate)
          ? IDS_MANAGE_SEARCH_ENGINES_AND_SHORTCUTS
          : IDS_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH);

  if (omnibox::ShouldShowAimContextMenuOption(profile_)) {
    if (auto* service = AiModeButtonServiceFactory::GetForProfile(profile_)) {
      if (const AiModeButtonUiConfig* config = service->GetCurrentConfig()) {
        context_menu_model_->AddCheckItem(kAlwaysShowAiMode,
                                          config->context_menu_label);
      }
    }
  }

  // Loomnibox settings.
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  if (prefs::AreShortcutsAvailableForProfile(profile_)) {
    context_menu_model_->AddCheckItemWithStringId(
        kShowShortcuts, IDS_SETTINGS_OMNIBOX_EVERYWHERE_SHOW_SHORTCUTS_TITLE);
  }
  context_menu_model_->AddItemWithStringId(
      kCustomizeKeyboardShortcut,
      IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT);
  context_menu_model_->AddItemWithStringId(
      kSettings, IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_SETTINGS);
}

// Forwards unhandled keyboard events from the renderer process (such as
// keyboard shortcuts) to the Views FocusManager so that accelerators and focus
// traversal work as expected.
bool OmniboxEverywhereUIManager::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_->HandleKeyboardEvent(
      event, widget_ ? widget_->GetFocusManager() : nullptr);
}

// ui::SimpleMenuModel::Delegate:
// Dispatches standard text editing commands from the context menu to the
// underlying WebContents. Explicitly focuses the WebContents beforehand so
// that focus temporarily acquired by the context menu UI runner is returned
// to the WebContents and its focused frame input handler.
void OmniboxEverywhereUIManager::ExecuteCommand(int command_id,
                                                int event_flags) {
  if (!web_contents()) {
    return;
  }
  web_contents()->Focus();
  switch (command_id) {
    case kUndo:
      web_contents()->Undo();
      break;
    case kCut:
      web_contents()->Cut();
      break;
    case kCopy:
      web_contents()->Copy();
      break;
    case kPaste:
      web_contents()->Paste();
      break;
    case kPasteAndSearch: {
      GetClipboardText(
          /*notify_if_restricted=*/true,
          base::BindOnce(
              [](base::WeakPtr<OmniboxEverywhereUIManager> self,
                 Profile* profile, std::u16string clipboard_text) {
                if (!self || !profile || clipboard_text.empty()) {
                  return;
                }
                auto* classifier =
                    AutocompleteClassifierFactory::GetForProfile(profile);
                if (!classifier) {
                  return;
                }
                AutocompleteMatch match;
                classifier->Classify(
                    clipboard_text, /*in_keyword_mode=*/false,
                    /*allow_exact_keyword_match=*/true,
                    metrics::OmniboxEventProto::OMNIBOX_EVERYWHERE, &match,
                    nullptr);
                if (match.destination_url.is_valid()) {
                  if (auto* service =
                          OmniboxEverywhereServiceFactory::GetForProfile(
                              profile)) {
                    service->OpenUrl(match.destination_url,
                                     WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                     ui::PAGE_TRANSITION_GENERATED);
                  }
                }
              },
              weak_factory_.GetWeakPtr(), profile_));
      break;
    }
    case kDelete:
      web_contents()->Delete();
      break;
    case kSelectAll:
      web_contents()->SelectAll();
      break;
    case kManageSearchEngines: {
      auto* service = OmniboxEverywhereServiceFactory::GetForProfile(profile_);
      if (service) {
        service->OpenUrl(chrome::GetSettingsUrl(chrome::kSearchEnginesSubPage),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB,
                         ui::PAGE_TRANSITION_AUTO_BOOKMARK);
      }
      break;
    }
    case kAlwaysShowAiMode: {
      if (profile_ && profile_->GetPrefs()) {
        PrefService* prefs = profile_->GetPrefs();
        prefs->SetBoolean(
            prefs::kOmniboxEverywhereShowAiMode,
            !prefs->GetBoolean(prefs::kOmniboxEverywhereShowAiMode));
      }
      break;
    }
    case kShowShortcuts:
      if (g_browser_process && g_browser_process->local_state()) {
        PrefService* local_state = g_browser_process->local_state();
        const bool is_currently_visible =
            prefs::IsOmniboxEverywhereShortcutsVisible(profile_, local_state);
        local_state->SetInteger(
            prefs::kOmniboxEverywhereShowShortcuts,
            static_cast<int>(is_currently_visible
                                 ? prefs::ShowShortcutsPrefValue::kDisabled
                                 : prefs::ShowShortcutsPrefValue::kEnabled));
      }
      break;
    // TODO(b/543460015): Differentiate settings and shortcut
    // customize URLs once dedicated deep-link routing / subpage anchors land.
    case kCustomizeKeyboardShortcut: {
      auto* service = OmniboxEverywhereServiceFactory::GetForProfile(profile_);
      if (service) {
        service->OpenUrl(chrome::GetSettingsUrl(chrome::kSearchSubPage),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB,
                         ui::PAGE_TRANSITION_AUTO_BOOKMARK);
      }
      break;
    }
    case kSettings: {
      auto* service = OmniboxEverywhereServiceFactory::GetForProfile(profile_);
      if (service) {
        service->OpenUrl(chrome::GetSettingsUrl(chrome::kSearchSubPage),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB,
                         ui::PAGE_TRANSITION_AUTO_BOOKMARK);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

// Evaluates whether a context menu command should be enabled.
// Note: When right-clicking an editable element without focusing it first,
// Blink populates `ContextMenuParams::is_editable = true` but may not set
// `ContextMenuDataEditFlags::kCanPaste` in `edit_flags` (as focus controller
// has not yet focused the element). Therefore, Paste is enabled when either
// kCanPaste edit flag is set or the element is editable. PasteAndSearch
// requires a valid Profile with an active OmniboxEverywhereService. Cut / Copy
// check for selected text in addition to Blink edit flags.
bool OmniboxEverywhereUIManager::IsCommandIdEnabled(int command_id) const {
  if (!web_contents()) {
    return false;
  }
  switch (command_id) {
    case kUndo:
      return (last_context_menu_params_.edit_flags &
              blink::ContextMenuDataEditFlags::kCanUndo) != 0;
    case kCut:
      return ((last_context_menu_params_.edit_flags &
               blink::ContextMenuDataEditFlags::kCanCut) != 0) ||
             (last_context_menu_params_.is_editable &&
              !last_context_menu_params_.selection_text.empty());
    case kCopy:
      return ((last_context_menu_params_.edit_flags &
               blink::ContextMenuDataEditFlags::kCanCopy) != 0) ||
             !last_context_menu_params_.selection_text.empty();
    case kPaste:
      return ((last_context_menu_params_.edit_flags &
               blink::ContextMenuDataEditFlags::kCanPaste) != 0) ||
             last_context_menu_params_.is_editable;
    case kPasteAndSearch:
      return profile_ && (OmniboxEverywhereServiceFactory::GetForProfile(
                              profile_) != nullptr);
    case kDelete:
      return ((last_context_menu_params_.edit_flags &
               blink::ContextMenuDataEditFlags::kCanDelete) != 0) ||
             !last_context_menu_params_.selection_text.empty();
    case kSelectAll:
      return true;
    case kManageSearchEngines:
      return profile_ && (OmniboxEverywhereServiceFactory::GetForProfile(
                              profile_) != nullptr);
    case kAlwaysShowAiMode:
      return true;
    case kShowShortcuts:
      return prefs::AreShortcutsAvailableForProfile(profile_);
    case kCustomizeKeyboardShortcut:
      return true;
    case kSettings:
      return true;
    default:
      return false;
  }
}

bool OmniboxEverywhereUIManager::IsCommandIdChecked(int command_id) const {
  if (command_id == kAlwaysShowAiMode) {
    if (profile_ && profile_->GetPrefs()) {
      return profile_->GetPrefs()->GetBoolean(
          prefs::kOmniboxEverywhereShowAiMode);
    }
    return true;
  }
  if (command_id == kShowShortcuts) {
    return g_browser_process && g_browser_process->local_state()
               ? prefs::IsOmniboxEverywhereShortcutsVisible(
                     profile_, g_browser_process->local_state())
               : true;
  }
  return false;
}

void OmniboxEverywhereUIManager::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (!widget_ || !widget_->GetContentsView() || !web_contents() ||
      !web_contents()->GetPrimaryMainFrame()) {
    return;
  }
  gfx::Point point_in_contents = point;
  views::View::ConvertPointFromScreen(widget_->GetContentsView(),
                                      &point_in_contents);
  content::ContextMenuParams params;
  params.x = point_in_contents.x();
  params.y = point_in_contents.y();
  params.source_type = source_type;
  params.is_editable = false;
  HandleContextMenu(*web_contents()->GetPrimaryMainFrame(), params);
}

std::unique_ptr<WebUIContentsWrapper>
OmniboxEverywhereUIManager::CreateContentsWrapper(Profile* profile) {
  if (contents_wrapper_factory_) {
    return contents_wrapper_factory_.Run(profile);
  }
  return std::make_unique<WebUIContentsWrapperT<OmniboxEverywhereUI>>(
      GURL(chrome::kChromeUIOmniboxEverywhereURL), profile,
      IDS_TASK_MANAGER_OMNIBOX, /*esc_closes_ui=*/true,
      /*supports_draggable_regions=*/true);
}

}  // namespace omnibox_everywhere
