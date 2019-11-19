// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/presentation_receiver_window_view.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window_delegate.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/media_router/presentation_receiver_window_frame.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

#if defined(OS_MACOSX)
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/window_properties.h"
#include "base/callback.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/native_widget_types.h"
#endif

using content::WebContents;

#if defined(OS_CHROMEOS)
// Observes the NativeWindow hosting the receiver view to look for fullscreen
// state changes.  This helps monitor fullscreen changes that don't go through
// the normal key accelerator to display and hide the location bar.
class FullscreenWindowObserver : public aura::WindowObserver {
 public:
  FullscreenWindowObserver(aura::Window* observed_window,
                           base::RepeatingClosure on_fullscreen_change)
      : on_fullscreen_change_(on_fullscreen_change) {
    observed_window_.Add(observed_window);
  }

  ~FullscreenWindowObserver() override = default;

 private:
  // aura::WindowObserver overrides.
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == aura::client::kShowStateKey) {
      ui::WindowShowState new_state =
          window->GetProperty(aura::client::kShowStateKey);
      ui::WindowShowState old_state = static_cast<ui::WindowShowState>(old);
      if (old_state == ui::SHOW_STATE_FULLSCREEN ||
          new_state == ui::SHOW_STATE_FULLSCREEN) {
        on_fullscreen_change_.Run();
      }
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    observed_window_.Remove(window);
  }

  base::RepeatingClosure on_fullscreen_change_;

  ScopedObserver<aura::Window, aura::WindowObserver> observed_window_{this};

  DISALLOW_COPY_AND_ASSIGN(FullscreenWindowObserver);
};

#endif

PresentationReceiverWindowView::PresentationReceiverWindowView(
    PresentationReceiverWindowFrame* frame,
    PresentationReceiverWindowDelegate* delegate)
    : frame_(frame),
      delegate_(delegate),
      location_bar_model_(
          std::make_unique<LocationBarModelImpl>(this,
                                                 content::kMaxURLDisplayChars)),
      command_updater_(this),
      exclusive_access_manager_(this) {
  DCHECK(frame);
  DCHECK(delegate);
}

PresentationReceiverWindowView::~PresentationReceiverWindowView() = default;

void PresentationReceiverWindowView::Init() {
#if defined(OS_MACOSX)
  // On macOS, the mapping between accelerators and commands is dynamic and user
  // configurable. We fetch and use the default mapping.
  bool result = GetDefaultMacAcceleratorForCommandId(IDC_FULLSCREEN,
                                                     &fullscreen_accelerator_);
  DCHECK(result);
#else
  const auto accelerators = GetAcceleratorList();
  const auto fullscreen_accelerator =
      std::find_if(accelerators.begin(), accelerators.end(),
                   [](const AcceleratorMapping& accelerator) {
                     return accelerator.command_id == IDC_FULLSCREEN;
                   });
  DCHECK(fullscreen_accelerator != accelerators.end());
  fullscreen_accelerator_ = ui::Accelerator(fullscreen_accelerator->keycode,
                                            fullscreen_accelerator->modifiers);
#endif

  auto* const focus_manager = GetFocusManager();
  DCHECK(focus_manager);
  focus_manager->RegisterAccelerator(
      fullscreen_accelerator_, ui::AcceleratorManager::kNormalPriority, this);

  auto* const web_contents = GetWebContents();
  DCHECK(web_contents);

  SecurityStateTabHelper::CreateForWebContents(web_contents);
  ChromeTranslateClient::CreateForWebContents(web_contents);
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));
  ManagePasswordsUIController::CreateForWebContents(web_contents);
  SearchTabHelper::CreateForWebContents(web_contents);
  TabDialogs::CreateForWebContents(web_contents);
  FramebustBlockTabHelper::CreateForWebContents(web_contents);
  ChromeSubresourceFilterClient::CreateForWebContents(web_contents);
  InfoBarService::CreateForWebContents(web_contents);
  MixedContentSettingsTabHelper::CreateForWebContents(web_contents);
  PopupBlockerTabHelper::CreateForWebContents(web_contents);
  TabSpecificContentSettings::CreateForWebContents(web_contents);

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* web_view = new views::WebView(profile);
  web_view->SetWebContents(web_contents);
  web_view->set_allow_accelerators(true);
  location_bar_view_ =
      new LocationBarView(nullptr, profile, &command_updater_, this, true);

  auto box_owner = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  box_owner->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  auto* box = SetLayoutManager(std::move(box_owner));
  AddChildView(location_bar_view_);
  box->SetFlexForView(location_bar_view_, 0);
  AddChildView(web_view);
  box->SetFlexForView(web_view, 1);

  location_bar_view_->Init();

#if defined(OS_CHROMEOS)
  window_observer_ = std::make_unique<FullscreenWindowObserver>(
      GetWidget()->GetNativeWindow(),
      base::BindRepeating(&PresentationReceiverWindowView::OnFullscreenChanged,
                          base::Unretained(this)));
#endif
}

void PresentationReceiverWindowView::Close() {
  frame_->Close();
}

bool PresentationReceiverWindowView::IsWindowActive() const {
  return frame_->IsActive();
}

bool PresentationReceiverWindowView::IsWindowFullscreen() const {
  return frame_->IsFullscreen();
}

gfx::Rect PresentationReceiverWindowView::GetWindowBounds() const {
  return frame_->GetWindowBoundsInScreen();
}

void PresentationReceiverWindowView::ShowInactiveFullscreen() {
  frame_->ShowInactive();
  exclusive_access_manager_.fullscreen_controller()
      ->ToggleBrowserFullscreenMode();
}

void PresentationReceiverWindowView::UpdateWindowTitle() {
  frame_->UpdateWindowTitle();
}

void PresentationReceiverWindowView::UpdateLocationBar() {
  DCHECK(location_bar_view_);
  location_bar_view_->Update(nullptr);
}

WebContents* PresentationReceiverWindowView::GetWebContents() {
  return delegate_->web_contents();
}

LocationBarModel* PresentationReceiverWindowView::GetLocationBarModel() {
  return location_bar_model_.get();
}

const LocationBarModel* PresentationReceiverWindowView::GetLocationBarModel()
    const {
  return location_bar_model_.get();
}

ContentSettingBubbleModelDelegate*
PresentationReceiverWindowView::GetContentSettingBubbleModelDelegate() {
  NOTREACHED();
  return nullptr;
}

void PresentationReceiverWindowView::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  NOTREACHED();
}

WebContents* PresentationReceiverWindowView::GetActiveWebContents() const {
  return delegate_->web_contents();
}

bool PresentationReceiverWindowView::CanResize() const {
  return true;
}

bool PresentationReceiverWindowView::CanMaximize() const {
  return true;
}

bool PresentationReceiverWindowView::CanMinimize() const {
  return true;
}

void PresentationReceiverWindowView::DeleteDelegate() {
  auto* const delegate = delegate_;
  delete this;
  delegate->WindowClosed();
}

base::string16 PresentationReceiverWindowView::GetWindowTitle() const {
  return delegate_->web_contents()->GetTitle();
}

bool PresentationReceiverWindowView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  exclusive_access_manager_.fullscreen_controller()
      ->ToggleBrowserFullscreenMode();
  return true;
}

Profile* PresentationReceiverWindowView::GetProfile() {
  return Profile::FromBrowserContext(
      delegate_->web_contents()->GetBrowserContext());
}

bool PresentationReceiverWindowView::IsFullscreen() const {
  return frame_->IsFullscreen();
}

void PresentationReceiverWindowView::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {
  frame_->SetFullscreen(true);
#if !defined(OS_CHROMEOS)
  OnFullscreenChanged();
#endif
  UpdateExclusiveAccessExitBubbleContent(url, bubble_type,
                                         ExclusiveAccessBubbleHideCallback(),
                                         /*force_update=*/false);
}

void PresentationReceiverWindowView::ExitFullscreen() {
  frame_->SetFullscreen(false);
#if !defined(OS_CHROMEOS)
  OnFullscreenChanged();
#endif
}

void PresentationReceiverWindowView::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
    bool force_update) {
#if defined(CHROMEOS)
  // On Chrome OS, we will not show the toast for the normal browser fullscreen
  // mode.  The 'F11' text is confusing since how to access F11 on a Chromebook
  // is not common knowledge and there is also a dedicated fullscreen toggle
  // button available.
  if (bubble_type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE || url.is_empty()) {
#else
  if (bubble_type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) {
#endif
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

void PresentationReceiverWindowView::OnExclusiveAccessUserInput() {}

content::WebContents* PresentationReceiverWindowView::GetActiveWebContents() {
  return delegate_->web_contents();
}

void PresentationReceiverWindowView::UnhideDownloadShelf() {}

void PresentationReceiverWindowView::HideDownloadShelf() {}

bool PresentationReceiverWindowView::CanUserExitFullscreen() const {
  return true;
}

ExclusiveAccessManager*
PresentationReceiverWindowView::GetExclusiveAccessManager() {
  return &exclusive_access_manager_;
}

views::Widget* PresentationReceiverWindowView::GetBubbleAssociatedWidget() {
  return frame_;
}

ui::AcceleratorProvider*
PresentationReceiverWindowView::GetAcceleratorProvider() {
  return this;
}

gfx::NativeView PresentationReceiverWindowView::GetBubbleParentView() const {
  return frame_->GetNativeView();
}

gfx::Point PresentationReceiverWindowView::GetCursorPointInParent() const {
  gfx::Point cursor_pos = display::Screen::GetScreen()->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(GetWidget()->GetRootView(), &cursor_pos);
  return cursor_pos;
}

gfx::Rect PresentationReceiverWindowView::GetClientAreaBoundsInScreen() const {
  return GetWidget()->GetClientAreaBoundsInScreen();
}

bool PresentationReceiverWindowView::IsImmersiveModeEnabled() const {
  return false;
}

gfx::Rect PresentationReceiverWindowView::GetTopContainerBoundsInScreen() {
  return GetBoundsInScreen();
}

void PresentationReceiverWindowView::DestroyAnyExclusiveAccessBubble() {
  exclusive_access_bubble_.reset();
}

bool PresentationReceiverWindowView::CanTriggerOnMouse() const {
  return true;
}

bool PresentationReceiverWindowView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id != IDC_FULLSCREEN)
    return false;
  *accelerator = fullscreen_accelerator_;
  return true;
}

void PresentationReceiverWindowView::OnFullscreenChanged() {
  const bool fullscreen = IsFullscreen();
  if (!fullscreen)
    exclusive_access_bubble_.reset();
  location_bar_view_->SetVisible(!fullscreen);
  if (fullscreen == (location_bar_view_->height() > 0))
    Layout();
}
