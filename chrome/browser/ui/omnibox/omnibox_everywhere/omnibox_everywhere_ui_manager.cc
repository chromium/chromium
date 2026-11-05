// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_fre_controller.h"
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
#include "components/ntp_tiles/custom_links_manager.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/color/color_provider_key.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/view_type_utils.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <shobjidl.h>
#include <windows.h>

#include <wrl/client.h>

#include "base/task/thread_pool.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_shortcut_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_event_handler_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/window_animations.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/mac_window_util.h"
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
  // Non-draggable rects take precedence regardless of DOM order. Build each
  // set with SkRegion::setRects(); one SkRegion::op() per rect is quadratic.
  std::vector<SkIRect> draggable_rects;
  std::vector<SkIRect> non_draggable_rects;
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    (region->draggable ? draggable_rects : non_draggable_rects)
        .push_back(gfx::RectToSkIRect(region->bounds));
  }
  SkRegion draggable_region;
  draggable_region.setRects(draggable_rects);
  SkRegion non_draggable_region;
  non_draggable_region.setRects(non_draggable_rects);
  draggable_region.op(non_draggable_region, SkRegion::kDifference_Op);
  return draggable_region;
}

gfx::Rect AdjustBoundsToWorkArea(gfx::Rect bounds,
                                 const gfx::Point& display_reference_point) {
  if (const display::Screen* const screen = display::Screen::Get()) {
    const display::Display display =
        screen->GetDisplayNearestPoint(display_reference_point);
    const gfx::Rect work_area = display.work_area();
    if (!work_area.IsEmpty()) {
      bounds.AdjustToFit(work_area);
    }
  }
  return bounds;
}

class OmniboxEverywhereContentsWrapper
    : public WebUIContentsWrapperT<OmniboxEverywhereUI> {
 public:
  using WebUIContentsWrapperT<OmniboxEverywhereUI>::WebUIContentsWrapperT;

  // content::WebContentsDelegate:
  void PreHandleDragUpdate(const content::DropData& /*drop_data*/,
                           const gfx::PointF& /*client_pt*/) override {
    if (auto* ui_manager =
            static_cast<OmniboxEverywhereUIManager*>(GetHost().get())) {
      ui_manager->OnDragEntered();
    }
  }

  void PreHandleDragExit() override {
    if (auto* ui_manager =
            static_cast<OmniboxEverywhereUIManager*>(GetHost().get())) {
      ui_manager->OnDragExited();
    }
  }
};

// Top-level widget hosting the Omnibox Everywhere WebUI. Because this widget
// has no parent BrowserWidget, a plain views::Widget only queries
// ui::NativeTheme (the OS theme) and ignores the active Profile's ThemeService.
// Subclassing views::Widget and observing ThemeService ensures the widget's
// ColorProviderKey reflects Chrome's BrowserColorScheme (Light/Dark/System)
// while always using the unthemed GM3 baseline palette.
class OmniboxEverywhereWidget : public views::Widget,
                                public ThemeServiceObserver {
 public:
  explicit OmniboxEverywhereWidget(Profile* profile) {
    if (profile) {
      if (auto* theme_service = ThemeServiceFactory::GetForProfile(profile)) {
        theme_service_observation_.Observe(theme_service);
      }
    }
  }
  OmniboxEverywhereWidget(const OmniboxEverywhereWidget&) = delete;
  OmniboxEverywhereWidget& operator=(const OmniboxEverywhereWidget&) = delete;
  ~OmniboxEverywhereWidget() override = default;

  void ResetThemeObserver() { theme_service_observation_.Reset(); }

  // ThemeServiceObserver:
  void OnThemeChanged() override {
    NotifyColorProviderChanged();
    ThemeChanged();
  }

  // views::Widget:
  ui::ColorProviderKey GetColorProviderKey() const override {
    ui::ColorProviderKey key = views::Widget::GetColorProviderKey();
    if (const auto* theme_service = theme_service_observation_.GetSource()) {
      const auto browser_color_scheme =
          theme_service->GetBrowserColorScheme();
      if (browser_color_scheme != ThemeService::BrowserColorScheme::kSystem) {
        key.color_mode =
            browser_color_scheme == ThemeService::BrowserColorScheme::kLight
                ? ui::ColorProviderKey::ColorMode::kLight
                : ui::ColorProviderKey::ColorMode::kDark;
      }
    }
    if (color_mode_override().has_value()) {
      key.color_mode = color_mode_override().value();
    }
    // Always use the unthemed GM3 baseline palette so Omnibox Everywhere
    // respects Light/Dark mode (from chrome://settings/appearance or the
    // OS) without inheriting Chrome browser color themes or OS accent
    // tinting.
    key.user_color_source = ui::ColorProviderKey::UserColorSource::kBaseline;
    key.user_color = std::nullopt;
    return key;
  }

 private:
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
};

#if BUILDFLAG(IS_WIN)
bool IsOnCurrentVirtualDesktop(HWND hwnd) {
  Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager;
  if (FAILED(::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr,
                                CLSCTX_ALL,
                                IID_PPV_ARGS(&virtual_desktop_manager)))) {
    return true;
  }
  // If this can't be determined, assume the window is on the current virtual
  // desktop.
  return gfx::IsWindowOnCurrentVirtualDesktop(hwnd, virtual_desktop_manager)
      .value_or(true);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

OmniboxEverywhereUIManager::OmniboxEverywhereUIManager(
    ContentsWrapperFactory contents_wrapper_factory)
    : contents_wrapper_factory_(std::move(contents_wrapper_factory)),
      unhandled_keyboard_event_handler_(
          std::make_unique<views::UnhandledKeyboardEventHandler>()),
      context_menu_controller_(
          std::make_unique<OmniboxEverywhereContextMenuController>(*this))
#if BUILDFLAG(IS_WIN)
      ,
      shortcut_helper_(base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      is_on_current_virtual_desktop_(
          base::BindRepeating(&IsOnCurrentVirtualDesktop))
#endif
{
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
  }
}

OmniboxEverywhereUIManager::~OmniboxEverywhereUIManager() {
  DestroyWidgetAndWebContents();
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

#if BUILDFLAG(IS_WIN)
void OmniboxEverywhereUIManager::CreateStartMenuShortcut(
    base::OnceCallback<void(bool)> callback) {
  shortcut_helper_
      .AsyncCall(&OmniboxEverywhereShortcutHelperWin::CreateStartMenuShortcut)
      .Then(std::move(callback));
}

void OmniboxEverywhereUIManager::DisableTaskbarPinning() {
  taskbar_pinning_disabled_ = true;
}

void OmniboxEverywhereUIManager::OnStartMenuShortcutChecked(
    bool shortcut_exists) {
  // TODO(crbug.com/562064992): The widget that triggered the check keeps its
  // AUMID, so it stays pinnable; only later widgets are covered.
  if (!shortcut_exists) {
    DisableTaskbarPinning();
    // Reset so that transient failures can be retried on the next widget.
    start_menu_shortcut_requested_ = false;
  } else {
    taskbar_pinning_disabled_ = false;
  }
}
#endif  // BUILDFLAG(IS_WIN)

content::WebContents* OmniboxEverywhereUIManager::web_contents() const {
  return contents_wrapper_ ? contents_wrapper_->web_contents() : nullptr;
}

void OmniboxEverywhereUIManager::ShowForProfile(Profile* profile,
                                                gfx::NativeWindow context) {
  CancelPendingDeactivation();
  std::optional<gfx::Rect> widget_bounds;
#if BUILDFLAG(IS_WIN)
  // Windows keeps a window on the virtual desktop it was last shown on, so if
  // the widget is on another desktop, recreate it, which opens it on the
  // current one. This falls through to the creation path below, which keeps
  // its position and reuses the `WebContents`. Leave it alone while a dialog is
  // open, since recreating it would dismiss the dialog.
  if (widget_ && profile_ == profile && !HasOpenModalDialog() &&
      !is_on_current_virtual_desktop_.Run(
          views::HWNDForWidget(widget_.get()))) {
    widget_bounds = widget_->GetRestoredBounds();
    DestroyWidget();
  }
#endif
  last_shown_time_ = base::TimeTicks::Now();
  if (widget_ && profile_ == profile) {
    ActivateAndFocus();
    return;
  }

  if (widget_ || (contents_wrapper_ && profile_ != profile)) {
    // If a different profile is present, clean up first.
    DestroyWidgetAndWebContents();
  }

  // Ensure any previous browser collection observation is cleanly reset if the
  // active profile is changing.
  if (profile_ != profile) {
    browser_collection_observation_.Reset();
    profile_pref_change_registrar_.Reset();
    if (profile && profile->GetPrefs()) {
      profile_pref_change_registrar_.Init(profile->GetPrefs());
      profile_pref_change_registrar_.Add(
          prefs::kOmniboxEverywhereShowShortcuts,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
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
      ntp_tiles::MostVisitedSites::AddCustomLinksObserver(
          profile_pref_change_registrar_, ntp_tiles::CustomLinksScope::kDesktop,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
      ntp_tiles::MostVisitedSites::AddEnterpriseShortcutsObserver(
          profile_pref_change_registrar_,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
    }
  }
  profile_ = profile;

  EnsureContentsWrapperInitialized(profile_);
  CreateAndInitWidget(context, widget_bounds);
  ActivateAndFocus();

  if (web_contents()) {
    if (auto* rwhv = web_contents()->GetRenderWidgetHostView()) {
      const gfx::Size kAutoResizeMinSize(GetPopupFixedWidth(), 50);
      const gfx::Size kAutoResizeMaxSize(GetPopupExpandedFixedWidth(), 800);
      rwhv->EnableAutoResize(kAutoResizeMinSize, kAutoResizeMaxSize);
    }
  }
}

// static
int OmniboxEverywhereUIManager::GetPopupFixedWidth() {
  if (omnibox::kOmniboxEverywhereDynamicSizingParam.Get()) {
    return prefs::IsEphemeralModelEnabled() ? GetPopupExpandedFixedWidth()
                                            : kDynamicPopupSmallFixedWidth;
  }
  return omnibox::kOmniboxEverywhereSmallLoomniboxParam.Get()
             ? kPopupSmallFixedWidth
             : kPopupFixedWidth;
}

// static
int OmniboxEverywhereUIManager::GetPopupExpandedFixedWidth() {
  if (omnibox::kOmniboxEverywhereDynamicSizingParam.Get()) {
    return omnibox::kOmniboxEverywhereDynamicSizingWidthParam.Get() == 600
               ? kDynamicPopupMediumFixedWidth
               : kDynamicPopupLargeFixedWidth;
  }
  return GetPopupFixedWidth();
}

gfx::Rect OmniboxEverywhereUIManager::CalculateWidgetBounds(int height) {
  display::Display target_display =
      display::Screen::Get()->GetDisplayNearestPoint(
          display::Screen::Get()->GetCursorScreenPoint());
  gfx::Rect work_area = target_display.work_area();
  int width = std::min(GetPopupFixedWidth(), work_area.width());
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
    PermissionPromptObserver::CreateForWebContents(web_contents());
    if (auto* observer =
            PermissionPromptObserver::FromWebContents(web_contents())) {
      permission_prompt_observation_.Reset();
      permission_prompt_observation_.Observe(observer);
    }
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

  auto* service = GetOmniboxEverywhereService();
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
  if (auto* service = GetOmniboxEverywhereService()) {
    service->ReleaseProfileKeepAlive();
  }
  keep_alive_.reset();
}

void OmniboxEverywhereUIManager::CreateAndInitWidget(
    gfx::NativeWindow context,
    std::optional<gfx::Rect> bounds) {
  if (widget_) {
    return;
  }

  if (!TryAcquireKeepAlives()) {
    return;
  }

  widget_ = std::make_unique<OmniboxEverywhereWidget>(profile_);

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
  widget_delegate_ = std::make_unique<OmniboxEverywhereWidgetDelegate>(
      TemplateURLServiceFactory::GetForProfile(profile_));
  if (draggable_region_) {
    widget_delegate_->SetDraggableRegion(draggable_region_);
  }
  params.delegate = widget_delegate_.get();
  params.z_order = is_ephemeral ? ui::ZOrderLevel::kFloatingWindow
                                : ui::ZOrderLevel::kNormal;
  if (context) {
    params.context = context;
  }

  if (bounds) {
    params.bounds = *bounds;
  } else {
    auto* service = GetOmniboxEverywhereService();
    const bool is_fre_intro_modal =
        service && service->fre_controller() &&
        service->fre_controller()->GetCurrentStage() == FreStage::kIntroModal;
    params.bounds = CalculateWidgetBounds(
        is_fre_intro_modal ? kFreModalRestingHeight : kDefaultRestingHeight);
  }

  auto web_view = std::make_unique<views::WebView>(profile_);
  web_view->SetProperty(views::kElementIdentifierKey,
                        kOmniboxEverywhereElementId);
  web_view->set_context_menu_controller(context_menu_controller_.get());
  // Allow the WebContents host to route unhandled accelerator keys through
  // the Views focus/accelerator system.
  web_view->set_allow_accelerators_deprecated(true);
  web_view->SetWebContents(web_contents());
  widget_delegate_->SetContentsView(std::move(web_view));

  widget_->Init(std::move(params));

  if (!is_ephemeral) {
    // Views unconditionally strips WS_MINIMIZEBOX during Init() for frameless
    // widgets (remove_standard_frame), ignoring CanMinimize(). Setting the
    // constraint now that the Widget exists re-applies it via
    // SizeConstraintsChanged(), so the shell offers taskbar minimization.
    CHECK(!widget_delegate_->CanMinimize());
    widget_delegate_->SetCanMinimize(true);
  }
#if BUILDFLAG(IS_MAC)
  widget_->SetActivationIndependence(is_ephemeral);
  widget_->SetCanAppearInExistingFullscreenSpaces(true);
#endif
#if BUILDFLAG(IS_WIN)
  const HWND hwnd = views::HWNDForWidget(widget_.get());
  SetWindowProperties(hwnd, is_ephemeral,
                      /*allow_pinning=*/!taskbar_pinning_disabled_);
  // Ephemeral widgets are never pinnable, so they do not need the Start Menu
  // shortcut that the Shell requires for pinning.
  if (!is_ephemeral && !std::exchange(start_menu_shortcut_requested_, true)) {
    CreateStartMenuShortcut(
        base::BindOnce(&OmniboxEverywhereUIManager::OnStartMenuShortcutChecked,
                       weak_factory_.GetWeakPtr()));
  }
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
  UpdateModalInteractionState();
}

void OmniboxEverywhereUIManager::ActivateAndFocus() {
  if (!widget_) {
    return;
  }

  if (!TryAcquireKeepAlives()) {
    return;
  }

  is_demoted_ = false;
  if (widget_->IsMinimized()) {
    widget_->Restore();
    ApplyPendingAutoResize();
  }
#if BUILDFLAG(IS_MAC)
  widget_->SetCanAppearInExistingFullscreenSpaces(true);
  widget_->MoveToActiveFullscreenSpace();
#endif
  // The widget is reused across shows, and the default search engine may have
  // changed since its window icon was last set.
  widget_delegate_->UpdateWindowIconForSearchEngine(
      TemplateURLServiceFactory::GetForProfile(profile_));
  widget_->Show();
  capture_release_timer_.Stop();
  widget_->Activate();

  if (widget_->GetContentsView()) {
    widget_->GetContentsView()->RequestFocus();
  }
  if (web_contents()) {
    web_contents()->Focus();
  }

  MaybeShowLensPromo();
}

void OmniboxEverywhereUIManager::OnEphemeralModelPrefChanged() {
  if (!widget_) {
    return;
  }
  bool was_visible = IsVisible() || widget_->IsMinimized();
  Profile* profile = profile_;
  DestroyWidgetAndWebContents();
  if (was_visible) {
    CHECK(profile);
    ShowForProfile(profile);
  }
}

void OmniboxEverywhereUIManager::OnMostVisitedPrefChanged() {
  // Don't tear down the widget if it is visible on screen or minimized on the
  // taskbar.
  if (!widget_ || IsVisible() || widget_->IsMinimized()) {
    return;
  }
  // Clean up the widget when the pref changes so there is not flicker when the
  // omnibox is re-invoked, while keeping the WebContents alive in case a Mojo
  // handler on `contents_wrapper_` (such as `MostVisitedHandler`) is on the
  // call stack.
  DestroyWidget();
  ReleaseKeepAlives();
}

OmniboxEverywhereService*
OmniboxEverywhereUIManager::GetOmniboxEverywhereService() {
  return profile_ ? OmniboxEverywhereServiceFactory::GetForProfile(profile_)
                  : nullptr;
}

void OmniboxEverywhereUIManager::EndFreSession() {
  if (auto* service = GetOmniboxEverywhereService()) {
    if (service->fre_controller()) {
      service->fre_controller()->EndSession();
    }
  }
}

void OmniboxEverywhereUIManager::MaybeShowLensPromo() {
  if (auto* service = GetOmniboxEverywhereService()) {
    service->MaybeShowLensPromo();
  }
}

void OmniboxEverywhereUIManager::EndLensPromo() {
  if (auto* service = GetOmniboxEverywhereService()) {
    service->EndLensPromo();
  }
}

void OmniboxEverywhereUIManager::Close() {
  if (is_closing_) {
    return;
  }
  base::AutoReset<bool> closing_reset(&is_closing_, true);
  last_shown_time_.reset();
  CancelPendingDeactivation();
  if (widget_) {
    if (HasOpenModalDialog()) {
      // TODO(crbug.com/567673370): Ideally, we should only close the open
      // modals and not destroy the widget or the WebContents here (preserving
      // the widget's position). For now, destroy the widget to close dialogs
      // that can't be dismissed directly (e.g. native file choosers), while
      // keeping the WebContents alive in case Close() is reached from inside a
      // call on it (e.g. OpenURLFromTab).
      DestroyWidget();
      EndFreSession();
      ReleaseKeepAlives();
      return;
    }
    context_menu_controller_->Cancel();
#if BUILDFLAG(IS_MAC)
    // On macOS, reset auxiliary collection behaviors before hiding so the
    // Window Server stops associating Chrome with the current Space. This
    // ensures smooth Space transitions when activating browser windows.
    if (widget_->GetNativeWindow()) {
      omnibox_everywhere::DisassociatePopupOnMac(widget_->GetNativeWindow());
    }
#endif
    if (widget_->IsVisible() && web_contents()) {
      // Temporarily keep the WebContents painting (`kHiddenButPainting`) across
      // `widget_->Hide()` so Blink finishes collapsing the dropdown and
      // running AutoResize offscreen before entering `kHidden`.
      if (capture_release_timer_.IsRunning()) {
        capture_release_timer_.Reset();
      } else {
        capture_release_timer_.Start(
            FROM_HERE, kPostHideCaptureDuration,
            base::DoNothingWithBoundArgs(web_contents()->IncrementCapturerCount(
                gfx::Size(), /*stay_hidden=*/true, /*stay_awake=*/false,
                /*is_activity=*/false)));
      }
    }
#if defined(USE_AURA)
    // Aura does not deliver events to hidden windows, so the handler may never
    // see the release or capture loss that ends a pending drag hold.
    event_handler_->ResetDragState();
#endif
    widget_->Hide();
  }
  EndFreSession();
  ReleaseKeepAlives();
}

void OmniboxEverywhereUIManager::Demote() {
  CancelTransientUiState();
  if (!widget_ || !widget_->IsVisible() || is_demoted_ ||
      HasOpenModalDialog()) {
    return;
  }
  is_demoted_ = true;
  EndLensPromo();
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
    ::SetWindowPos(
        hwnd, HWND_BOTTOM, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
  }
#endif
}

#if BUILDFLAG(IS_WIN)
// The delegate reports `CanMinimize() == false`, but that only gates the
// `WS_MINIMIZEBOX` caption button and the `SC_MINIMIZE` system menu item, not
// `HWNDMessageHandler::Minimize()`.
void OmniboxEverywhereUIManager::Minimize() {
  if (!widget_ || !widget_->IsVisible() || widget_->IsMinimized()) {
    return;
  }
  CancelTransientUiState();
  if (!std::exchange(is_demoted_, true)) {
    EndLensPromo();
  }
  widget_->Minimize();
}
#endif  // BUILDFLAG(IS_WIN)

void OmniboxEverywhereUIManager::DestroyWidgetAndWebContents() {
  DestroyWidget();
  permission_prompt_observation_.Reset();
  contents_wrapper_.reset();
  draggable_region_.reset();
  browser_collection_observation_.Reset();
  EndFreSession();
  ReleaseKeepAlives();
}

void OmniboxEverywhereUIManager::DestroyWidget() {
  capture_release_timer_.Stop();
  CancelPendingDeactivation();
  if (disclosure_dialog_widget_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(disclosure_dialog_widget_));
  }
  scoped_ignore_input_events_.reset();
  if (widget_) {
    static_cast<OmniboxEverywhereWidget*>(widget_.get())->ResetThemeObserver();
    widget_observation_.Reset();
#if defined(USE_AURA)
    if (auto* native_view = widget_->GetNativeView()) {
      native_view->RemovePreTargetHandler(event_handler_.get());
    }
    // The handler outlives the widget and, once detached, can no longer see
    // the release or capture loss that ends a pending drag hold.
    event_handler_->ResetDragState();
#endif
    if (auto* contents_view = widget_delegate_->GetContentsView()) {
      if (auto* web_view = views::AsViewClass<views::WebView>(contents_view)) {
        web_view->SetWebContents(nullptr);
        // The widget is destroyed asynchronously and may outlive `this`.
        web_view->set_context_menu_controller(nullptr);
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
  context_menu_controller_->Reset();
  open_modals_.Clear();
  is_demoted_ = false;
  is_dragging_ = false;
  is_adjusting_bounds_ = false;
  pending_auto_resize_size_.reset();
  region_select_overlay_.reset();
  last_shown_time_.reset();
}

void OmniboxEverywhereUIManager::CancelPendingDeactivation() {
  deactivation_task_.Cancel();
  hotkey_dropdown_deactivation_task_.Cancel();
  pending_dismissal_timer_.Stop();
  is_drag_over_web_contents_ = false;
}

void OmniboxEverywhereUIManager::CancelTransientUiState() {
  last_shown_time_.reset();
  CancelPendingDeactivation();
  context_menu_controller_->Cancel();
}

void OmniboxEverywhereUIManager::Shutdown() {
  CancelPendingDeactivation();
  last_shown_time_.reset();
  permission_prompt_observation_.Reset();
  browser_collection_observation_.Reset();
  profile_pref_change_registrar_.Reset();
  DestroyWidgetAndWebContents();
  profile_ = nullptr;
}

bool OmniboxEverywhereUIManager::IsVisible() const {
  return widget_ && widget_->IsVisible() && !widget_->IsMinimized();
}

bool OmniboxEverywhereUIManager::IsActive() const {
  return widget_ && widget_->IsActive() && !widget_->IsMinimized() &&
         !is_demoted_;
}

bool OmniboxEverywhereUIManager::HasOpenModalDialog() const {
  return !open_modals_.empty() || region_select_overlay_ != nullptr;
}

bool OmniboxEverywhereUIManager::IsScreenshareCaptureInProgress() const {
  return open_modals_.Has(Modal::kScreensharePicker) ||
         region_select_overlay_ != nullptr;
}

void OmniboxEverywhereUIManager::ShowOverflowMenu(
    const gfx::Rect& anchor_screen_rect,
    ui::mojom::MenuSourceType source_type,
    base::OnceClosure on_closed) {
  // Clicking the button can queue a deactivation (e.g. the WebContents losing
  // focus); cancel it so the popup isn't dismissed as the menu opens.
  CancelPendingDeactivation();
  context_menu_controller_->ShowOverflowMenu(anchor_screen_rect, source_type,
                                             std::move(on_closed));
}

void OmniboxEverywhereUIManager::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (active) {
    CancelPendingDeactivation();
    const bool was_demoted = std::exchange(is_demoted_, false);
    if (!HasOpenModalDialog() && !context_menu_controller_->IsOpen() &&
        web_contents()) {
      web_contents()->Focus();
    }
    if (was_demoted) {
      MaybeShowLensPromo();
    }
    return;
  }
  if (!HasOpenModalDialog() && !context_menu_controller_->IsOpen()) {
    HandleWidgetDeactivated();
  }
}

void OmniboxEverywhereUIManager::OnWidgetVisibilityOnScreenChanged(
    views::Widget* widget,
    bool visible) {
  if (!visible && !HasOpenModalDialog() &&
      !context_menu_controller_->IsOpen() && prefs::IsEphemeralModelEnabled()) {
    Close();
  }
}

void OmniboxEverywhereUIManager::OnWidgetShowStateChanged(
    views::Widget* widget) {
  if (is_closing_ || !widget_ || widget != widget_.get()) {
    return;
  }
  if (!widget_->IsMinimized()) {
    ApplyPendingAutoResize();
    if (std::exchange(is_demoted_, false)) {
      MaybeShowLensPromo();
    }
    return;
  }
  CancelTransientUiState();
  if (!std::exchange(is_demoted_, true)) {
    EndLensPromo();
  }
  if (prefs::IsEphemeralModelEnabled() && !HasOpenModalDialog()) {
    Close();
  }
}

void OmniboxEverywhereUIManager::OnContextMenuClosed() {
  if (widget_ && !widget_->IsActive() && !HasOpenModalDialog()) {
    HandleWidgetDeactivated();
  }
}

void OmniboxEverywhereUIManager::HandleWidgetDeactivated() {
  // A minimized widget is deactivated but must stay alive to be restorable.
  if (is_closing_ || !widget_ || !widget_->IsVisible() || is_demoted_ ||
      widget_->IsMinimized()) {
    return;
  }
  if (last_shown_time_.has_value() &&
      base::TimeTicks::Now() - *last_shown_time_ < kActivationGracePeriod) {
    pending_dismissal_timer_.Stop();
    is_drag_over_web_contents_ = false;
    deactivation_task_.Reset(
        base::BindOnce(&OmniboxEverywhereUIManager::ActivateAndFocus,
                       weak_factory_.GetWeakPtr()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, deactivation_task_.callback());
    return;
  }
  // In ephemeral mode, a deactivation that arrives while a mouse button is
  // held was caused by a press in another window. That press may be the start
  // of a drag (e.g. a file from Finder) whose target is this widget, which
  // cannot be known until the button is released; hiding now would destroy the
  // drop target. Defer the decision until release.
  if (prefs::IsEphemeralModelEnabled() && widget_->IsMouseButtonDown()) {
    deactivation_task_.Cancel();
    if (!pending_dismissal_timer_.IsRunning()) {
      is_drag_over_web_contents_ = false;
      pending_dismissal_timer_.Start(
          FROM_HERE, kPendingDismissalPollInterval, this,
          &OmniboxEverywhereUIManager::OnPendingDismissalPoll);
    }
    return;
  }
  pending_dismissal_timer_.Stop();
  is_drag_over_web_contents_ = false;
  deactivation_task_.Reset(base::BindOnce(&OmniboxEverywhereUIManager::CloseUI,
                                          weak_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, deactivation_task_.callback());
}

void OmniboxEverywhereUIManager::OnPendingDismissalPoll() {
  if (is_closing_ || !widget_ || !widget_->IsVisible() ||
      widget_->IsMinimized() || is_demoted_ || HasOpenModalDialog() ||
      context_menu_controller_->IsOpen()) {
    pending_dismissal_timer_.Stop();
    is_drag_over_web_contents_ = false;
    return;
  }
  if (widget_->IsMouseButtonDown()) {
    return;
  }
  pending_dismissal_timer_.Stop();
  // If an external drag was over the WebContents when the mouse button was
  // released (`PreHandleDragUpdate()` fired without a subsequent
  // `PreHandleDragExit()`), the press ended as a drop onto this widget.
  // Dropping does not make the window key, so re-focus it to let the user keep
  // typing.
  if (std::exchange(is_drag_over_web_contents_, false)) {
    ActivateAndFocus();
    return;
  }
  CloseUI();
}

void OmniboxEverywhereUIManager::OnWidgetDestroying(views::Widget* widget) {
  DestroyWidgetAndWebContents();
}

void OmniboxEverywhereUIManager::OnWidgetClosed(
    views::Widget::ClosedReason reason) {
  DestroyWidgetAndWebContents();
}

void OmniboxEverywhereUIManager::ApplyPendingAutoResize() {
  if (!pending_auto_resize_size_.has_value()) {
    return;
  }
  const gfx::Size size = *pending_auto_resize_size_;
  pending_auto_resize_size_.reset();
  ResizeDueToAutoResize(web_contents(), size);
}

void OmniboxEverywhereUIManager::AdjustWidgetBoundsToWorkArea() {
  if (is_closing_ || is_adjusting_bounds_ || !widget_ || is_dragging_ ||
      widget_->IsMinimized()) {
    return;
  }
  const gfx::Rect current_bounds = widget_->GetWindowBoundsInScreen();
  const gfx::Rect adjusted_bounds =
      AdjustBoundsToWorkArea(current_bounds, current_bounds.CenterPoint());
  if (adjusted_bounds != current_bounds) {
    base::AutoReset<bool> adjusting_reset(&is_adjusting_bounds_, true);
    widget_->SetBounds(adjusted_bounds);
  }
}

void OmniboxEverywhereUIManager::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  AdjustWidgetBoundsToWorkArea();
}

void OmniboxEverywhereUIManager::OnWidgetUserDragStarted(
    views::Widget* widget) {
  is_dragging_ = true;
}

void OmniboxEverywhereUIManager::OnWidgetUserDragEnded(views::Widget* widget) {
  is_dragging_ = false;
  ApplyPendingAutoResize();
  AdjustWidgetBoundsToWorkArea();
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
  if (is_dragging_ || open_modals_.Has(Modal::kScreensharePicker) ||
      widget_->IsMinimized()) {
    pending_auto_resize_size_ = new_size;
    return;
  }
  constexpr int kAutoResizeMinHeight = 56;
  const int target_width =
      (omnibox::kOmniboxEverywhereDynamicSizingParam.Get() &&
       new_size.width() > GetPopupFixedWidth())
          ? GetPopupExpandedFixedWidth()
          : GetPopupFixedWidth();
  const gfx::Size target_size(
      target_width, std::max(new_size.height(), kAutoResizeMinHeight));
  if (widget_->GetSize() != target_size) {
    const gfx::Rect current_bounds = widget_->GetWindowBoundsInScreen();
    gfx::Rect bounds = current_bounds;
    if (omnibox::kOmniboxEverywhereDynamicSizingParam.Get()) {
      // Shift `x` by half the width delta so the popup expands and collapses
      // symmetrically from its horizontal center rather than growing only to
      // the right.
      bounds.set_x(bounds.x() - (target_size.width() - bounds.width()) / 2);
    }
    bounds.set_size(target_size);
    const gfx::Rect adjusted_bounds =
        AdjustBoundsToWorkArea(bounds, current_bounds.CenterPoint());
    if (adjusted_bounds != current_bounds) {
      base::AutoReset<bool> adjusting_reset(&is_adjusting_bounds_, true);
      widget_->SetBounds(adjusted_bounds);
    }
  }
}

void OmniboxEverywhereUIManager::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

void OmniboxEverywhereUIManager::OnPermissionPromptChanged(
    bool is_showing,
    const gfx::Size& prompt_size) {
  SetModalOpen(Modal::kPermissionPrompt, is_showing);
}

void OmniboxEverywhereUIManager::OnFileChooserOpened() {
  SetModalOpen(Modal::kFileChooser, true);
}

void OmniboxEverywhereUIManager::OnFileChooserClosed() {
  SetModalOpen(Modal::kFileChooser, false);
}

void OmniboxEverywhereUIManager::OnDrivePickerOpened() {
  SetModalOpen(Modal::kDrivePicker, true);
}

void OmniboxEverywhereUIManager::OnDrivePickerClosed() {
  SetModalOpen(Modal::kDrivePicker, false);
}

void OmniboxEverywhereUIManager::OnHotkeyDropdownOpened() {
  hotkey_dropdown_deactivation_task_.Cancel();
  SetModalOpen(Modal::kHotkeyDropdown, true);
}

void OmniboxEverywhereUIManager::OnHotkeyDropdownClosed() {
  SetModalOpen(Modal::kHotkeyDropdown, false);
  // Delay check by a tick to allow native window activation to settle if widget
  // was reactivated.
  hotkey_dropdown_deactivation_task_.Reset(base::BindOnce(
      &OmniboxEverywhereUIManager::CheckDeactivationAfterHotkeyDropdownClosed,
      weak_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, hotkey_dropdown_deactivation_task_.callback());
}

void OmniboxEverywhereUIManager::CheckDeactivationAfterHotkeyDropdownClosed() {
  if (widget_ && !widget_->IsActive() && !HasOpenModalDialog() &&
      !context_menu_controller_->IsOpen() && prefs::IsEphemeralModelEnabled()) {
    HandleWidgetDeactivated();
  }
}

void OmniboxEverywhereUIManager::OnDragEntered() {
  if (pending_dismissal_timer_.IsRunning()) {
    is_drag_over_web_contents_ = true;
  }
}

void OmniboxEverywhereUIManager::OnDragExited() {
  is_drag_over_web_contents_ = false;
}

void OmniboxEverywhereUIManager::OnScreensharePickerOpened() {
  SetModalOpen(Modal::kScreensharePicker, true);
  if (widget_) {
    widget_->Hide();
  }
}

void OmniboxEverywhereUIManager::OnScreensharePickerClosed() {
  SetModalOpen(Modal::kScreensharePicker, false);
  ApplyPendingAutoResize();
  if (widget_ && !suppress_restore_on_screenshare_picker_closed_) {
    ActivateAndFocus();
  }
}

bool OmniboxEverywhereUIManager::CancelChromeDefaultPicker(
    Profile* target_profile) {
  // Never cancel during region selection overlay: it already owns the screen
  // with fullscreen overlays across all displays, so it cannot be occluded by
  // another window, and invocations should continue to be ignored.
  if (region_select_overlay_) {
    return false;
  }

  // If the invocation was targeted for a different profile, suppress
  // restoring and focusing the current profile's widget when the picker closes
  // to prevent visual flicker before the new profile is shown.
  std::optional<base::AutoReset<bool>> suppress_restore;
  if (target_profile && target_profile != profile_) {
    suppress_restore.emplace(&suppress_restore_on_screenshare_picker_closed_,
                             true);
  }

  auto* web_contents = this->web_contents();
  auto* webui = web_contents ? web_contents->GetWebUI() : nullptr;
  auto* ui = (webui && webui->GetController())
                 ? webui->GetController()->GetAs<OmniboxEverywhereUI>()
                 : nullptr;

  if (ui && ui->CancelChromeDefaultPicker()) {
    if (open_modals_.Has(Modal::kScreensharePicker)) {
      OnScreensharePickerClosed();
    } else if (widget_ && !widget_->IsVisible() &&
               !suppress_restore_on_screenshare_picker_closed_) {
      ActivateAndFocus();
    }
    return true;
  }

  // Fallback only for test harnesses (e.g. TestWebUIContentsWrapper) where
  // WebUI is mocked. In production, a failed cancellation (e.g. native OS
  // picker) must return false so the invocation is ignored.
  if (!ui && open_modals_.Has(Modal::kScreensharePicker)) {
    OnScreensharePickerClosed();
    return true;
  }
  return false;
}

void OmniboxEverywhereUIManager::ShowScreenshotDisclosureDialog(
    base::OnceClosure on_accepted,
    base::OnceClosure on_cancelled) {
  if (disclosure_dialog_widget_ && !disclosure_dialog_widget_->IsClosed()) {
    disclosure_dialog_widget_->Activate();
    if (on_cancelled) {
      std::move(on_cancelled).Run();
    }
    return;
  }

  if (!widget_ || !widget_->GetContentsView()) {
    if (on_cancelled) {
      std::move(on_cancelled).Run();
    }
    return;
  }

  SetModalOpen(Modal::kScreenshareDisclosure, true);

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_OMNIBOX_EVERYWHERE_SCREENSHOT_DISCLOSURE_TITLE))
          .OverrideShowCloseButton(true)
          .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
              IDS_OMNIBOX_EVERYWHERE_SCREENSHOT_DISCLOSURE_BODY)))
          .AddOkButton(
              base::DoNothing(),
              ui::DialogModel::Button::Params()
                  .SetId(views::DialogClientView::kOkButtonElementId)
                  .SetLabel(l10n_util::GetStringUTF16(IDS_APP_CONTINUE))
                  .SetStyle(ui::ButtonStyle::kProminent))
          .AddCancelButton(base::DoNothing(),
                           ui::DialogModel::Button::Params().SetId(
                               views::DialogClientView::kCancelButtonElementId))
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), widget_->GetContentsView(),
      views::BubbleBorder::FLOAT);
  bubble->set_fixed_width(600);
  bubble->set_corner_radius(16);
  bubble->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  disclosure_dialog_widget_ =
      base::WrapUnique(views::DialogDelegate::CreateDialogWidget(
          bubble.release(), /*context=*/gfx::NativeWindow(),
          /*parent=*/widget_->GetNativeView()));
  disclosure_dialog_widget_->SetZOrderLevel(widget_->GetZOrderLevel());
  disclosure_dialog_widget_->MakeCloseSynchronous(
      base::BindOnce(&OmniboxEverywhereUIManager::OnScreenshotDisclosureClosed,
                     weak_factory_.GetWeakPtr(), std::move(on_accepted),
                     std::move(on_cancelled)));
  disclosure_dialog_widget_->Show();
}

void OmniboxEverywhereUIManager::OnScreenshotDisclosureClosed(
    base::OnceClosure on_accepted,
    base::OnceClosure on_cancelled,
    views::Widget::ClosedReason reason) {
  CancelPendingDeactivation();
  if (disclosure_dialog_widget_) {
    disclosure_dialog_widget_->Hide();
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(disclosure_dialog_widget_));
  SetModalOpen(Modal::kScreenshareDisclosure, false);

  if (reason == views::Widget::ClosedReason::kAcceptButtonClicked) {
    if (on_accepted) {
      // Defer dispatching on_accepted to ensure the disclosure dialog window
      // is completely closed and removed from display before any screenshot
      // capture begins.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(
                         [](base::WeakPtr<OmniboxEverywhereUIManager> self,
                            base::OnceClosure on_accepted) {
                           if (!self || !self->widget_) {
                             return;
                           }
                           std::move(on_accepted).Run();
                         },
                         weak_factory_.GetWeakPtr(), std::move(on_accepted)));
    }
  } else {
    if (on_cancelled) {
      std::move(on_cancelled).Run();
    }
    if (widget_ && !widget_->IsClosed() &&
        !open_modals_.Has(Modal::kScreensharePicker) &&
        !region_select_overlay_) {
      ActivateAndFocus();
    }
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
  UpdateModalInteractionState();
}

void OmniboxEverywhereUIManager::OnRegionSelectOverlayClosed(
    RegionSelectedCallback callback,
    const SkBitmap& result_bitmap) {
  if (region_select_overlay_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(region_select_overlay_));
  }
  UpdateModalInteractionState();
  std::move(callback).Run(result_bitmap);
}

void OmniboxEverywhereUIManager::SetModalOpen(Modal modal, bool open) {
  open_modals_.PutOrRemove(modal, open);
  UpdateModalInteractionState();
}

void OmniboxEverywhereUIManager::UpdateModalInteractionState() {
  // Unlike external modal dialogs, the hotkey dropdown is rendered inside the
  // WebUI and requires input events to remain enabled.
  ModalSet input_blocking_modals = open_modals_;
  input_blocking_modals.Remove(Modal::kHotkeyDropdown);
  if (!input_blocking_modals.empty() || region_select_overlay_ != nullptr) {
    if (!scoped_ignore_input_events_ && web_contents()) {
      scoped_ignore_input_events_ =
          web_contents()->IgnoreInputEvents(std::nullopt);
    }
  } else {
    scoped_ignore_input_events_.reset();
  }
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
  if (auto* service = GetOmniboxEverywhereService()) {
    service->OpenUrl(params.url, params.disposition, params.transition,
                     std::move(navigation_handle_callback));
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

// Shows the native right-click context menu for editable controls, selected
// text and the widget background. See `OmniboxEverywhereContextMenuController`.
bool OmniboxEverywhereUIManager::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  context_menu_controller_->HandleContextMenu(params);
  return true;
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

views::Widget* OmniboxEverywhereUIManager::GetContextMenuHostWidget() {
  return widget_.get();
}

content::WebContents* OmniboxEverywhereUIManager::GetContextMenuWebContents() {
  return web_contents();
}

Profile* OmniboxEverywhereUIManager::GetContextMenuProfile() {
  return profile_;
}

#if BUILDFLAG(IS_WIN)
void OmniboxEverywhereUIManager::MinimizeFromContextMenu() {
  Minimize();
}

void OmniboxEverywhereUIManager::CloseFromContextMenu() {
  Close();
}
#endif  // BUILDFLAG(IS_WIN)

std::unique_ptr<WebUIContentsWrapper>
OmniboxEverywhereUIManager::CreateContentsWrapper(Profile* profile) {
  if (contents_wrapper_factory_) {
    return contents_wrapper_factory_.Run(profile);
  }
  // Do not close UI immediately on Escape in PreHandleKeyboardEvent so that the
  // WebUI searchbox and composebox can staged-unwind (clear input or context)
  // before dismissing the popup.
  return std::make_unique<OmniboxEverywhereContentsWrapper>(
      GURL(chrome::kChromeUIOmniboxEverywhereURL), profile,
      IDS_TASK_MANAGER_OMNIBOX, /*esc_closes_ui=*/false,
      /*supports_draggable_regions=*/true);
}

}  // namespace omnibox_everywhere
