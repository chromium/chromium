// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/presentation_receiver_window_view.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window_delegate.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/media_router/presentation_receiver_window_frame.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/window_properties.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/native_widget_types.h"
#endif

using content::WebContents;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Observes the NativeWindow hosting the receiver view to look for fullscreen
// state changes.  This helps monitor fullscreen changes that don't go through
// the normal key accelerator to display and hide the location bar.
class FullscreenWindowObserver : public aura::WindowObserver {
 public:
  FullscreenWindowObserver(aura::Window* observed_window,
                           base::RepeatingClosure on_fullscreen_change)
      : on_fullscreen_change_(on_fullscreen_change) {
    window_observation_.Observe(observed_window);
  }

  FullscreenWindowObserver(const FullscreenWindowObserver&) = delete;
  FullscreenWindowObserver& operator=(const FullscreenWindowObserver&) = delete;

  ~FullscreenWindowObserver() override = default;

 private:
  // aura::WindowObserver overrides.
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == aura::client::kShowStateKey) {
      ui::mojom::WindowShowState new_state =
          window->GetProperty(aura::client::kShowStateKey);
      ui::mojom::WindowShowState old_state =
          static_cast<ui::mojom::WindowShowState>(old);
      if (old_state == ui::mojom::WindowShowState::kFullscreen ||
          new_state == ui::mojom::WindowShowState::kFullscreen) {
        on_fullscreen_change_.Run();
      }
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(window_observation_.IsObserving());
    window_observation_.Reset();
  }

  base::RepeatingClosure on_fullscreen_change_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
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
  SetHasWindowSizeControls(true);

  // TODO(pbos): See if this can retain SetOwnedByWidget(true) and get deleted
  // through WidgetDelegate::DeleteDelegate(). This requires confirming that
  // delegate_->WindowClosed() is safe to call before this deletes.
  SetOwnedByWidget(false);
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](PresentationReceiverWindowView* dialog) {
        auto* const delegate = dialog->delegate_.get();
        delete dialog;
        delegate->WindowClosed();
      },
      this));

  DCHECK(frame);
  DCHECK(delegate);
}

PresentationReceiverWindowView::~PresentationReceiverWindowView() = default;

void PresentationReceiverWindowView::Init() {
#if BUILDFLAG(IS_MAC)
  // On macOS, the mapping between accelerators and commands is dynamic and user
  // configurable. We fetch and use the default mapping.
  bool result = GetDefaultMacAcceleratorForCommandId(IDC_FULLSCREEN,
                                                     &fullscreen_accelerator_);
  DCHECK(result);
#else
  const auto accelerators = GetAcceleratorList();
  const auto fullscreen_accelerator = base::ranges::find(
      accelerators, IDC_FULLSCREEN, &AcceleratorMapping::command_id);
  CHECK(fullscreen_accelerator != accelerators.end(),
        base::NotFatalUntil::M130);
  fullscreen_accelerator_ = ui::Accelerator(fullscreen_accelerator->keycode,
                                            fullscreen_accelerator->modifiers);
#endif

  auto* const focus_manager = GetFocusManager();
  DCHECK(focus_manager);
  focus_manager->RegisterAccelerator(
      fullscreen_accelerator_, ui::AcceleratorManager::kNormalPriority, this);

  auto* const web_contents = GetWebContents();
  DCHECK(web_contents);

  // ContentInfoBarManager comes before common tab helpers since
  // ContentSubresourceFilterThrottleManager has it as a dependency.
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents);

  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);
  ChromeTranslateClient::CreateForWebContents(web_contents);
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents);
  ChromePasswordManagerClient::CreateForWebContents(web_contents);
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(web_contents);
  ManagePasswordsUIController::CreateForWebContents(web_contents);
  SearchTabHelper::CreateForWebContents(web_contents);
  TabDialogs::CreateForWebContents(web_contents);
  FramebustBlockTabHelper::CreateForWebContents(web_contents);
  CreateSubresourceFilterWebContentsHelper(web_contents);
  MixedContentSettingsTabHelper::CreateForWebContents(web_contents);
  blocked_content::PopupBlockerTabHelper::CreateForWebContents(web_contents);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));

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
  AddChildView(location_bar_view_.get());
  box->SetFlexForView(location_bar_view_, 0);
  AddChildView(web_view);
  box->SetFlexForView(web_view, 1);

  location_bar_view_->Init();

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
}

void PresentationReceiverWindowView::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  NOTREACHED();
}

WebContents* PresentationReceiverWindowView::GetActiveWebContents() const {
  return delegate_->web_contents();
}

std::u16string PresentationReceiverWindowView::GetWindowTitle() const {
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
    ExclusiveAccessBubbleType bubble_type,
    const int64_t display_id) {
  frame_->SetFullscreen(true);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  OnFullscreenChanged();
#endif
  UpdateExclusiveAccessBubble({.url = url, .type = bubble_type},
                              base::NullCallback());
}

void PresentationReceiverWindowView::ExitFullscreen() {
  frame_->SetFullscreen(false);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  OnFullscreenChanged();
#endif
}

void PresentationReceiverWindowView::UpdateExclusiveAccessBubble(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, we will not show the toast for the normal browser fullscreen
  // mode.  The 'F11' text is confusing since how to access F11 on a Chromebook
  // is not common knowledge and there is also a dedicated fullscreen toggle
  // button available.
  if ((!params.has_download &&
       params.type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) ||
      params.url.is_empty()) {
#else
  if (!params.has_download &&
      params.type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) {
#endif
    // |exclusive_access_bubble_.reset()| will trigger callback for current
    // bubble with |ExclusiveAccessBubbleHideReason::kInterrupted| if available.
    exclusive_access_bubble_.reset();
    if (first_hide_callback) {
      std::move(first_hide_callback)
          .Run(ExclusiveAccessBubbleHideReason::kNotShown);
    }
    return;
  }

  if (exclusive_access_bubble_) {
    exclusive_access_bubble_->Update(params, std::move(first_hide_callback));
    return;
  }

  exclusive_access_bubble_ = std::make_unique<ExclusiveAccessBubbleViews>(
      this, params, std::move(first_hide_callback));
}

bool PresentationReceiverWindowView::IsExclusiveAccessBubbleDisplayed() const {
  return exclusive_access_bubble_ && exclusive_access_bubble_->IsShowing();
}

void PresentationReceiverWindowView::OnExclusiveAccessUserInput() {}

content::WebContents*
PresentationReceiverWindowView::GetWebContentsForExclusiveAccess() {
  return delegate_->web_contents();
}

bool PresentationReceiverWindowView::CanUserExitFullscreen() const {
  return true;
}

ExclusiveAccessManager*
PresentationReceiverWindowView::GetExclusiveAccessManager() {
  return &exclusive_access_manager_;
}

ui::AcceleratorProvider*
PresentationReceiverWindowView::GetAcceleratorProvider() {
  return this;
}

gfx::NativeView PresentationReceiverWindowView::GetBubbleParentView() const {
  return frame_->GetNativeView();
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
    DeprecatedLayoutImmediately();
}

BEGIN_METADATA(PresentationReceiverWindowView)
END_METADATA
