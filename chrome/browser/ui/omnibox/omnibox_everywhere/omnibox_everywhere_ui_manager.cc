// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include <algorithm>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#if defined(USE_AURA)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_event_handler_aura.h"
#endif
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/view_type_utils.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_mac_utils.h"
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
    : contents_wrapper_factory_(std::move(contents_wrapper_factory)) {
#if defined(USE_AURA)
  event_handler_ = std::make_unique<OmniboxEverywhereEventHandlerAura>(*this);
#endif
}

OmniboxEverywhereUIManager::~OmniboxEverywhereUIManager() = default;

OmniboxEverywhereWidgetDelegate* OmniboxEverywhereUIManager::widget_delegate() {
  return widget_delegate_.get();
}

const OmniboxEverywhereWidgetDelegate*
OmniboxEverywhereUIManager::widget_delegate() const {
  return widget_delegate_.get();
}

bool OmniboxEverywhereUIManager::IsPointInDraggableRegion(
    const gfx::Point& point) const {
  return draggable_region_ && !draggable_region_->isEmpty() &&
         draggable_region_->contains(point.x(), point.y());
}

content::WebContents* OmniboxEverywhereUIManager::web_contents() const {
  return contents_wrapper_ ? contents_wrapper_->web_contents() : nullptr;
}

void OmniboxEverywhereUIManager::ShowForProfile(Profile* profile,
                                                gfx::NativeWindow context) {
  if (widget_ && profile_ == profile && widget_->IsVisible()) {
    ActivateAndFocus();
    return;
  }

  if (widget_) {
    // If a different profile (or a hidden/closing widget) is present, clean up
    // first.
    CleanUpWidget();
  }

  // Ensure any previous browser collection observation is cleanly reset if the
  // active profile is changing.
  if (profile_ != profile) {
    browser_collection_observation_.Reset();
  }
  profile_ = profile;
  is_navigating_ = false;

  EnsureContentsWrapperInitialized(profile_);
  CreateAndInitWidget(context);
  ActivateAndFocus();

  if (web_contents()) {
    if (auto* rwhv = web_contents()->GetRenderWidgetHostView()) {
      constexpr gfx::Size kAutoResizeMinSize(800, 50);
      constexpr gfx::Size kAutoResizeMaxSize(800, 800);
      rwhv->EnableAutoResize(kAutoResizeMinSize, kAutoResizeMaxSize);
    }
  }
}

void OmniboxEverywhereUIManager::EnsureContentsWrapperInitialized(
    Profile* profile) {
  if (contents_wrapper_) {
    return;
  }
  contents_wrapper_ = CreateContentsWrapper(profile);

  if (web_contents()) {
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
#if BUILDFLAG(IS_WIN)
  params.dont_show_in_taskbar = true;
#endif  // BUILDFLAG(IS_WIN)
  widget_delegate_ = std::make_unique<OmniboxEverywhereWidgetDelegate>();
  if (draggable_region_) {
    widget_delegate_->SetDraggableRegion(draggable_region_);
  }
  params.delegate = widget_delegate_.get();
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  if (context) {
    params.context = context;
  }

  display::Display target_display =
      display::Screen::Get()->GetDisplayNearestPoint(
          display::Screen::Get()->GetCursorScreenPoint());
  gfx::Rect screen_bounds = target_display.bounds();
  constexpr gfx::Size kDefaultPopupSize(864, 632);
  params.bounds =
      gfx::Rect(screen_bounds.x() +
                    (screen_bounds.width() - kDefaultPopupSize.width()) / 2,
                screen_bounds.y() +
                    (screen_bounds.height() - kDefaultPopupSize.height()) / 2,
                kDefaultPopupSize.width(), kDefaultPopupSize.height());
  auto web_view = std::make_unique<views::WebView>(profile_);
  web_view->SetProperty(views::kElementIdentifierKey,
                        kOmniboxEverywhereElementId);
  web_view->SetWebContents(web_contents());
  web_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  if (web_contents()) {
    if (auto* rwhv = web_contents()->GetRenderWidgetHostView()) {
      rwhv->SetBackgroundColor(SK_ColorTRANSPARENT);
    }
  }
  widget_delegate_->SetContentsView(std::move(web_view));

  widget_->Init(std::move(params));
  widget_->MakeCloseSynchronous(base::BindOnce(
      &OmniboxEverywhereUIManager::OnWidgetClosed, base::Unretained(this)));
  widget_observation_.Observe(widget_.get());

  CHECK(widget_->non_client_view() && widget_->non_client_view()->frame_view());
  widget_->non_client_view()->frame_view()->set_non_client_hit_test_callback(
      base::BindRepeating(&OmniboxEverywhereWidgetDelegate::NonClientHitTest,
                          base::Unretained(widget_delegate_.get())));

#if defined(USE_AURA)
  CHECK(widget_->GetNativeView());
  widget_->GetNativeView()->AddPreTargetHandler(event_handler_.get());
#endif
}

void OmniboxEverywhereUIManager::ActivateAndFocus() {
  if (!widget_) {
    return;
  }
  widget_->Show();
#if BUILDFLAG(IS_MAC)
  OrderOmniboxEverywhereFrontOnMac(widget_.get());
#else
  widget_->Activate();
#endif

  if (widget_->GetContentsView()) {
    widget_->GetContentsView()->RequestFocus();
  }
  if (web_contents()) {
    web_contents()->Focus();
  }
}

void OmniboxEverywhereUIManager::Close() {
  if (widget_) {
    widget_->Close();
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
  is_file_chooser_open_ = false;
  is_drive_picker_open_ = false;
  is_navigating_ = false;
  draggable_region_.reset();
  browser_collection_observation_.Reset();
}

void OmniboxEverywhereUIManager::Shutdown() {
  browser_collection_observation_.Reset();
  CleanUpWidget();
  profile_ = nullptr;
}

bool OmniboxEverywhereUIManager::IsVisible() const {
  return widget_ && widget_->IsVisible();
}

void OmniboxEverywhereUIManager::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (!active && !is_file_chooser_open_ && !is_drive_picker_open_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&OmniboxEverywhereUIManager::Close,
                                  weak_factory_.GetWeakPtr()));
  }
}

void OmniboxEverywhereUIManager::OnWidgetDestroying(views::Widget* widget) {
  CleanUpWidget();
}

void OmniboxEverywhereUIManager::OnWidgetClosed(
    views::Widget::ClosedReason reason) {
  CleanUpWidget();
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
  if (widget_) {
    constexpr int kAutoResizeHeightPadding = 96;
    constexpr int kAutoResizeMinHeight = 56;
    gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
    bounds.set_height(std::max(new_size.height() + kAutoResizeHeightPadding,
                               kAutoResizeMinHeight));
    widget_->SetBounds(bounds);
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
