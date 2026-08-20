// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include <algorithm>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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

#if defined(USE_AURA)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_event_handler_aura.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_animations.h"
#endif

namespace omnibox_everywhere {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OmniboxEverywhereUIManager,
                                      kOmniboxEverywhereElementId);

namespace {

bool IsEphemeral() {
  bool is_ephemeral = false;
  if (g_browser_process && g_browser_process->local_state()) {
    is_ephemeral = g_browser_process->local_state()->GetBoolean(
        prefs::kOmniboxEverywhereEphemeralModel);
  }
  return is_ephemeral;
}

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
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    draggable_region.op(
        SkIRect::MakeXYWH(region->bounds.x(), region->bounds.y(),
                          region->bounds.width(), region->bounds.height()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
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
#if !BUILDFLAG(IS_ANDROID)
      profile_pref_change_registrar_.Add(
          ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
          base::BindRepeating(
              &OmniboxEverywhereUIManager::OnMostVisitedPrefChanged,
              base::Unretained(this)));
#endif
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

void OmniboxEverywhereUIManager::CreateAndInitWidget(
    gfx::NativeWindow context) {
  if (widget_) {
    return;
  }
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.remove_standard_frame = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  bool is_ephemeral = IsEphemeral();
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
  widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);
  widget_->Show();
  widget_->Activate();

  if (widget_->GetContentsView()) {
    widget_->GetContentsView()->RequestFocus();
  }
  if (web_contents()) {
    web_contents()->Focus();
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

void OmniboxEverywhereUIManager::Close() {
  if (widget_) {
    if (is_file_chooser_open_ || is_drive_picker_open_) {
      CleanUpWidget();
      return;
    }
    if (is_context_menu_open_ && context_menu_runner_) {
      context_menu_runner_->Cancel();
      is_context_menu_open_ = false;
    }
    widget_->Hide();
  }
}

void OmniboxEverywhereUIManager::CleanUpWidget() {
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
  is_screenshare_picker_open_ = false;
  is_dragging_ = false;
  pending_auto_resize_size_.reset();
  draggable_region_.reset();
  browser_collection_observation_.Reset();
}

void OmniboxEverywhereUIManager::Shutdown() {
  browser_collection_observation_.Reset();
  profile_pref_change_registrar_.Reset();
  CleanUpWidget();
  profile_ = nullptr;
}

bool OmniboxEverywhereUIManager::IsVisible() const {
  return widget_ && widget_->IsVisible();
}

bool OmniboxEverywhereUIManager::IsActive() const {
  return widget_ && widget_->IsActive();
}

bool OmniboxEverywhereUIManager::HasModalDialogOpen() const {
  return is_file_chooser_open_ || is_drive_picker_open_ ||
         is_screenshare_picker_open_;
}

void OmniboxEverywhereUIManager::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (!active && !is_context_menu_open_ && !HasModalDialogOpen()) {
    if (IsEphemeral()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&OmniboxEverywhereUIManager::Close,
                                    weak_factory_.GetWeakPtr()));
    } else if (widget_) {
      widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
    }
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
  if (widget_ && !widget_->IsActive() && !HasModalDialogOpen()) {
    if (IsEphemeral()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&OmniboxEverywhereUIManager::Close,
                                    weak_factory_.GetWeakPtr()));
    } else {
      widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
    }
  }
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
  Close();
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
}

void OmniboxEverywhereUIManager::OnScreensharePickerClosed() {
  is_screenshare_picker_open_ = false;
  if (widget_) {
    ActivateAndFocus();
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

  // Only show a context menu for editable elements (e.g. search input box)
  // or when text is selected. Suppress context menus when right-clicking on
  // non-editable background or container padding of the widget.
  if (!params.is_editable && params.selection_text.empty()) {
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
  if (params.is_editable) {
    context_menu_model_->AddItemWithStringId(kCut, IDS_APP_CUT);
    context_menu_model_->AddItemWithStringId(kCopy, IDS_APP_COPY);
    context_menu_model_->AddItemWithStringId(kPaste, IDS_APP_PASTE);
    context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_model_->AddItemWithStringId(kSelectAll, IDS_APP_SELECT_ALL);
  } else {
    CHECK(!params.selection_text.empty());
    context_menu_model_->AddItemWithStringId(kCopy, IDS_APP_COPY);
    context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_model_->AddItemWithStringId(kSelectAll, IDS_APP_SELECT_ALL);
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
    case kCut:
      web_contents()->Cut();
      break;
    case kCopy:
      web_contents()->Copy();
      break;
    case kPaste:
      web_contents()->Paste();
      break;
    case kSelectAll:
      web_contents()->SelectAll();
      break;
    default:
      break;
  }
}

// Evaluates whether a context menu command should be enabled.
// Note: When right-clicking an editable element without focusing it first,
// Blink populates `ContextMenuParams::is_editable = true` but may not set
// `ContextMenuDataEditFlags::kCanPaste` in `edit_flags` (as focus controller
// has not yet focused the element). Therefore, Paste and Select All are always
// enabled for Omnibox Everywhere, and Cut / Copy check for selected text in
// addition to Blink edit flags.
bool OmniboxEverywhereUIManager::IsCommandIdEnabled(int command_id) const {
  if (!web_contents()) {
    return false;
  }
  switch (command_id) {
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
      return true;
    case kSelectAll:
      return true;
    default:
      return false;
  }
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
