// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/password_generation_popup_observer.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/text_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using autofill::PopupHidingReason;

namespace {

// Minimum number of characters of the typed password to display a minimized
// version of the generation popup.
constexpr int kMinCharsForMinimizedPopup = 6;

bool IsPasswordGenerationSuggestionsPreviewEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordGenerationPreviewOnHover);
}

}  // namespace

// Handles registration for key events with RenderFrameHost.
class PasswordGenerationPopupControllerImpl::KeyPressRegistrator {
 public:
  explicit KeyPressRegistrator(content::RenderFrameHost* frame)
      : frame_(frame) {}
  KeyPressRegistrator(const KeyPressRegistrator& rhs) = delete;
  KeyPressRegistrator& operator=(const KeyPressRegistrator& rhs) = delete;

  ~KeyPressRegistrator() = default;

  void RegisterKeyPressHandler(
      content::RenderWidgetHost::KeyPressEventCallback handler) {
    DCHECK(callback_.is_null());
    content::RenderWidgetHostView* view = frame_->GetView();
    if (!view)
      return;
    view->GetRenderWidgetHost()->AddKeyPressEventCallback(handler);
    callback_ = std::move(handler);
  }

  void RemoveKeyPressHandler() {
    if (!callback_.is_null()) {
      content::RenderWidgetHostView* view = frame_->GetView();
      if (view)
        view->GetRenderWidgetHost()->RemoveKeyPressEventCallback(callback_);
      callback_.Reset();
    }
  }

 private:
  const raw_ptr<content::RenderFrameHost, DanglingUntriaged> frame_;
  content::RenderWidgetHost::KeyPressEventCallback callback_;
};

base::WeakPtr<PasswordGenerationPopupControllerImpl>
PasswordGenerationPopupControllerImpl::GetOrCreate(
    base::WeakPtr<PasswordGenerationPopupControllerImpl> previous,
    const gfx::RectF& bounds,
    const autofill::password_generation::PasswordGenerationUIData& ui_data,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    content::RenderFrameHost* frame) {
  if (previous.get() && previous->element_bounds() == bounds &&
      previous->web_contents() == web_contents &&
      previous->driver_.get() == driver.get() &&
      previous->generation_element_id_ == ui_data.generation_element_id) {
    return previous;
  }

  if (previous.get())
    previous->HideImpl();

  PasswordGenerationPopupControllerImpl* controller =
      new PasswordGenerationPopupControllerImpl(bounds, ui_data, driver,
                                                observer, web_contents, frame);
  return controller->GetWeakPtr();
}

PasswordGenerationPopupControllerImpl::PasswordGenerationPopupControllerImpl(
    const gfx::RectF& bounds,
    const autofill::password_generation::PasswordGenerationUIData& ui_data,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    content::RenderFrameHost* frame)
    : content::WebContentsObserver(web_contents),
      view_(nullptr),
      form_data_(ui_data.form_data),
      driver_(driver),
      observer_(observer),
      form_signature_(autofill::CalculateFormSignature(form_data_)),
      field_signature_(autofill::CalculateFieldSignatureByNameAndType(
          ui_data.generation_element,
          "password")),
      generation_element_id_(ui_data.generation_element_id),
      max_length_(ui_data.max_length),
      // TODO(estade): use correct text direction.
      controller_common_(bounds,
                         base::i18n::LEFT_TO_RIGHT,
                         web_contents->GetNativeView()),
      password_selected_(false),
      state_(kOfferGeneration),
      key_press_handler_manager_(new KeyPressRegistrator(frame)) {
#if !BUILDFLAG(IS_ANDROID)
  // There may not always be a ZoomController, e.g. in tests.
  if (auto* zoom_controller =
          zoom::ZoomController::FromWebContents(web_contents)) {
    zoom_observation_.Observe(zoom_controller);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  help_text_ = l10n_util::GetStringUTF16(
      IDS_PASSWORD_GENERATION_PROMPT_GOOGLE_PASSWORD_MANAGER);
#else
  help_text_ = l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROMPT);
#endif  // !BUILDFLAG(IS_ANDROID)
}

PasswordGenerationPopupControllerImpl::
    ~PasswordGenerationPopupControllerImpl() = default;

base::WeakPtr<PasswordGenerationPopupControllerImpl>
PasswordGenerationPopupControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool PasswordGenerationPopupControllerImpl::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  switch (event.windows_key_code) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      PasswordSelected(true);
      return true;
    case ui::VKEY_ESCAPE:
      HideImpl();
      return true;
    case ui::VKEY_RETURN:
    case ui::VKEY_TAB:
      // We suppress tab if the password is selected because we will
      // automatically advance focus anyway.
      return PossiblyAcceptPassword();
    default:
      return false;
  }
}

bool PasswordGenerationPopupControllerImpl::IsVisible() const {
  return view_;
}

bool PasswordGenerationPopupControllerImpl::PossiblyAcceptPassword() {
  if (password_selected_) {
    PasswordAccepted();  // This will delete |this|.
    return true;
  }

  return false;
}

void PasswordGenerationPopupControllerImpl::PasswordSelected(bool selected) {
  if (state_ == kEditGeneratedPassword || selected == password_selected_)
    return;

  password_selected_ = selected;
  view_->PasswordSelectionUpdated();
}

void PasswordGenerationPopupControllerImpl::PasswordAccepted() {
  if (state_ != kOfferGeneration)
    return;

  base::WeakPtr<PasswordGenerationPopupControllerImpl> weak_this = GetWeakPtr();
  if (driver_) {
    // See https://crbug.com/1133635 for when `driver_` might be null due to a
    // compromised renderer.
    driver_->GeneratedPasswordAccepted(form_data_, generation_element_id_,
                                       current_generated_password_);
  }
  // |this| can be destroyed here because GeneratedPasswordAccepted pops up
  // another UI and generates some event to close the dropdown.
  if (weak_this)
    weak_this->HideImpl();
}

// TODO(crbug.com/1345766): Add test checking that delayed call to this function
// does not hide generation popup triggered by an empty password field.
void PasswordGenerationPopupControllerImpl::OnWeakCheckComplete(
    const std::string& checked_password,
    bool is_weak) {
  user_typed_password_is_weak_ = is_weak;
  state_minimized_ =
      is_weak && checked_password.length() >= kMinCharsForMinimizedPopup &&
      password_manager::features::kPasswordStrengthIndicatorWithMinimizedState
          .Get();

  if (is_weak) {
    Show(kOfferGeneration);
  } else if (!user_typed_password_.empty()) {
    HideImpl();
  }
}

void PasswordGenerationPopupControllerImpl::Show(GenerationUIState state) {
  // When switching from editing to generation state, regenerate the password.
  if (state == kOfferGeneration &&
      (state_ != state || current_generated_password_.empty())) {
    current_generated_password_ =
        driver_->GetPasswordGenerationHelper()->GeneratePassword(
            web_contents()->GetLastCommittedURL().DeprecatedGetOriginAsURL(),
            form_signature_, field_signature_, max_length_);
  }
  state_ = state;

  if (!view_) {
    view_ = PasswordGenerationPopupView::Create(GetWeakPtr());

    // Treat popup as being hidden if creation fails.
    if (!view_) {
      HideImpl();
      return;
    }
    key_press_handler_manager_->RegisterKeyPressHandler(base::BindRepeating(
        [](base::WeakPtr<PasswordGenerationPopupControllerImpl> weak_this,
           const content::NativeWebKeyboardEvent& event) {
          return weak_this && weak_this->HandleKeyPressEvent(event);
        },
        GetWeakPtr()));
    if (!view_->Show()) {
      // The instance is deleted after this point.
      return;
    }
  } else {
    view_->UpdateState();
    if (!view_->UpdateBoundsAndRedrawPopup()) {
      // The instance is deleted after this point.
      return;
    }
  }

  if (observer_)
    observer_->OnPopupShown(state_);
}

void PasswordGenerationPopupControllerImpl::
    UpdatePopupBasedOnTypedPasswordStrength() {
  if (user_typed_password_.empty()) {
    user_typed_password_is_weak_ = false;
    state_minimized_ = false;
    Show(kOfferGeneration);
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (!password_strength_calculation_) {
    password_strength_calculation_ =
        std::make_unique<password_manager::PasswordStrengthCalculation>();
  }
  const std::string user_typed_password =
      base::UTF16ToUTF8(user_typed_password_);
  password_manager::PasswordStrengthCalculation::CompletionCallback completion =
      base::BindOnce(
          &PasswordGenerationPopupControllerImpl::OnWeakCheckComplete,
          weak_ptr_factory_.GetWeakPtr(), user_typed_password);
  password_strength_calculation_->CheckPasswordWeakInSandbox(
      user_typed_password, std::move(completion));
#endif  // !BUILDFLAG(IS_ANDROID)
}

void PasswordGenerationPopupControllerImpl::UpdateTypedPassword(
    const std::u16string& new_user_typed_password) {
  user_typed_password_ = new_user_typed_password;
}

void PasswordGenerationPopupControllerImpl::UpdateGeneratedPassword(
    std::u16string new_password) {
  current_generated_password_ = std::move(new_password);
  if (view_)
    view_->UpdateGeneratedPasswordValue();
}

void PasswordGenerationPopupControllerImpl::FrameWasScrolled() {
  HideImpl();
}

void PasswordGenerationPopupControllerImpl::GenerationElementLostFocus() {
  HideImpl();
}

void PasswordGenerationPopupControllerImpl::GeneratedPasswordRejected() {
  HideImpl();
}

void PasswordGenerationPopupControllerImpl::WebContentsDestroyed() {
  HideImpl();
}

void PasswordGenerationPopupControllerImpl::PrimaryPageChanged(
    content::Page& page) {
  HideImpl();
}

#if !BUILDFLAG(IS_ANDROID)
void PasswordGenerationPopupControllerImpl::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  zoom_observation_.Reset();
}

void PasswordGenerationPopupControllerImpl::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  HideImpl();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void PasswordGenerationPopupControllerImpl::Hide(PopupHidingReason) {
  HideImpl();
}

void PasswordGenerationPopupControllerImpl::ViewDestroyed() {
  view_ = nullptr;

  HideImpl();
}

void PasswordGenerationPopupControllerImpl::SelectionCleared() {
  PasswordSelected(false);
  if (IsPasswordGenerationSuggestionsPreviewEnabled()) {
    driver_->ClearPreviewedForm();
  }
}

void PasswordGenerationPopupControllerImpl::SetSelected() {
  PasswordSelected(true);
  if (IsPasswordGenerationSuggestionsPreviewEnabled()) {
    driver_->PreviewGenerationSuggestion(current_generated_password_);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void PasswordGenerationPopupControllerImpl::
    OnGooglePasswordManagerLinkClicked() {
  NavigateToManagePasswordsPage(
      chrome::FindBrowserWithWebContents(GetWebContents()),
      password_manager::ManagePasswordsReferrer::kPasswordGenerationPrompt);
}

std::u16string PasswordGenerationPopupControllerImpl::GetPrimaryAccountEmail() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return std::u16string();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return std::u16string();
  return base::UTF8ToUTF16(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}
#endif  // !BUILDFLAG(IS_ANDROID)

gfx::NativeView PasswordGenerationPopupControllerImpl::container_view() const {
  return controller_common_.container_view;
}

content::WebContents* PasswordGenerationPopupControllerImpl::GetWebContents()
    const {
  return WebContentsObserver::web_contents();
}

const gfx::RectF& PasswordGenerationPopupControllerImpl::element_bounds()
    const {
  return controller_common_.element_bounds;
}

bool PasswordGenerationPopupControllerImpl::IsRTL() const {
  return base::i18n::IsRTL();
}

void PasswordGenerationPopupControllerImpl::HideImpl() {
  // Detach if the frame is still alive.
  if (driver_)
    key_press_handler_manager_->RemoveKeyPressHandler();

  if (view_)
    view_->Hide();

  if (observer_)
    observer_->OnPopupHidden();

  delete this;
}

PasswordGenerationPopupController::GenerationUIState
PasswordGenerationPopupControllerImpl::state() const {
  return state_;
}

bool PasswordGenerationPopupControllerImpl::password_selected() const {
  return password_selected_;
}

const std::u16string& PasswordGenerationPopupControllerImpl::password() const {
  return current_generated_password_;
}

std::u16string PasswordGenerationPopupControllerImpl::SuggestedText() {
  if (state_ == kOfferGeneration)
    return l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION_GPM);

  return l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_EDITING_SUGGESTION);
}

const std::u16string& PasswordGenerationPopupControllerImpl::HelpText() {
  return help_text_;
}

bool PasswordGenerationPopupControllerImpl::IsUserTypedPasswordWeak() const {
  return user_typed_password_is_weak_;
}

bool PasswordGenerationPopupControllerImpl::IsStateMinimized() const {
  return state_minimized_;
}
