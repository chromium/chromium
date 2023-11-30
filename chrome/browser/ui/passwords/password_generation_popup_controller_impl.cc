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
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/text_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using autofill::PopupHidingReason;
#if !BUILDFLAG(IS_ANDROID)
using password_manager::features::kPasswordGenerationExperimentVariationParam;
using password_manager::features::PasswordGenerationVariation;
using password_manager::prefs::kPasswordGenerationNudgePasswordDismissCount;
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {
#if !BUILDFLAG(IS_ANDROID)
constexpr int kMaxAllowedAmountOfNudgePasswordPopupRejections = 4;
#endif  // !BUILDFLAG(IS_ANDROID)
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
    content::RenderFrameHost* frame,
    PrefService* pref_service) {
  if (previous.get() && previous->element_bounds() == bounds &&
      previous->web_contents() == web_contents &&
      previous->driver_.get() == driver.get() &&
      previous->generation_element_id_ == ui_data.generation_element_id) {
    return previous;
  }

  if (previous.get())
    previous->HideImpl();

  PasswordGenerationPopupControllerImpl* controller =
      new PasswordGenerationPopupControllerImpl(
          bounds, ui_data, driver, observer, web_contents, frame, pref_service);
  return controller->GetWeakPtr();
}

PasswordGenerationPopupControllerImpl::PasswordGenerationPopupControllerImpl(
    const gfx::RectF& bounds,
    const autofill::password_generation::PasswordGenerationUIData& ui_data,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    content::RenderFrameHost* frame,
    PrefService* pref_service)
    : content::WebContentsObserver(web_contents),
      view_(nullptr),
      form_data_(ui_data.form_data),
      driver_(driver),
      observer_(observer),
      pref_service_(pref_service),
      form_signature_(autofill::CalculateFormSignature(form_data_)),
      field_signature_(autofill::CalculateFieldSignatureByNameAndType(
          ui_data.generation_element,
          autofill::FormControlType::kInputPassword)),
      generation_element_id_(ui_data.generation_element_id),
      max_length_(ui_data.max_length),
      controller_common_(bounds,
                         ui_data.text_direction,
                         web_contents->GetNativeView()),
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
  bool edit_password_enabled = false;
  // Password generation experiments are defined for Desktop only.
#if !BUILDFLAG(IS_ANDROID)
  PasswordGenerationVariation password_generation_variation =
      kPasswordGenerationExperimentVariationParam.Get();
  if (password_generation_variation ==
      PasswordGenerationVariation::kNudgePassword) {
    return HandleNudgePasswordKeyPressEvent(event);
  }
  edit_password_enabled = password_generation_variation ==
                          PasswordGenerationVariation::kEditPassword;
#endif  // !BUILDFLAG(IS_ANDROID)

  switch (event.windows_key_code) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      if (edit_password_enabled && password_selected()) {
        SelectElement(PasswordGenerationPopupElement::kEditPassword);
        return true;
      }

      SelectElement(PasswordGenerationPopupElement::kUseStrongPassword);
      return true;
    case ui::VKEY_ESCAPE:
      HideImpl();
      return true;
    case ui::VKEY_RETURN:
    case ui::VKEY_TAB:
      // We suppress tab if the password is selected because we will
      // automatically advance focus anyway.
      return PossiblyAcceptSelectedElement();
    default:
      return false;
  }
}

bool PasswordGenerationPopupControllerImpl::HandleNudgePasswordKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  switch (event.windows_key_code) {
    case ui::VKEY_ESCAPE:
      HideImpl();
      return true;
    case ui::VKEY_RETURN:
      PasswordAccepted();
      return true;
    default:
      return false;
  }
}

bool PasswordGenerationPopupControllerImpl::IsVisible() const {
  return view_;
}

bool PasswordGenerationPopupControllerImpl::PossiblyAcceptSelectedElement() {
  switch (selected_element_) {
    case PasswordGenerationPopupElement::kUseStrongPassword:
      PasswordAccepted();
      return true;
    case PasswordGenerationPopupElement::kEditPassword:
      EditPasswordClicked();
      return true;
    default:
      return false;
  }
}

bool PasswordGenerationPopupControllerImpl::IsSelectable() const {
  return state_ == kOfferGeneration;
}

void PasswordGenerationPopupControllerImpl::SelectElement(
    PasswordGenerationPopupElement element) {
  if (!IsSelectable() || selected_element_ == element) {
    return;
  }

  selected_element_ = element;
  view_->PasswordSelectionUpdated();
  view_->EditPasswordSelectionUpdated();
}

void PasswordGenerationPopupControllerImpl::PasswordAccepted() {
  if (state_ != kOfferGeneration)
    return;

#if !BUILDFLAG(IS_ANDROID)
  if (kPasswordGenerationExperimentVariationParam.Get() ==
          PasswordGenerationVariation::kNudgePassword &&
      pref_service_) {
    pref_service_->SetInteger(kPasswordGenerationNudgePasswordDismissCount, 0);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  base::WeakPtr<PasswordGenerationPopupControllerImpl> weak_this = GetWeakPtr();
  if (driver_) {
    // See https://crbug.com/1133635 for when `driver_` might be null due to a
    // compromised renderer.
    driver_->GeneratedPasswordAccepted(form_data_, generation_element_id_,
                                       current_generated_password_);
  }
  // |this| can be destroyed here because GeneratedPasswordAccepted pops up
  // another UI and generates some event to close the dropdown.
  if (weak_this) {
    driver_->FocusNextFieldAfterPasswords();
    weak_this->HideImpl();
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

  // In `kNudgePassword` Desktop experiment password is previewed straight away
  // in offer generation state.
#if !BUILDFLAG(IS_ANDROID)
  if (state == kOfferGeneration &&
      kPasswordGenerationExperimentVariationParam.Get() ==
          PasswordGenerationVariation::kNudgePassword) {
    driver_->PreviewGenerationSuggestion(current_generated_password_);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  if (observer_)
    observer_->OnPopupShown(state_);
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
#if !BUILDFLAG(IS_ANDROID)
  if (ShouldShowNudgePassword()) {
    pref_service_->SetInteger(
        kPasswordGenerationNudgePasswordDismissCount,
        pref_service_->GetInteger(
            kPasswordGenerationNudgePasswordDismissCount) +
            1);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  HideImpl();
}

void PasswordGenerationPopupControllerImpl::ViewDestroyed() {
  view_ = nullptr;

  HideImpl();
}

void PasswordGenerationPopupControllerImpl::SelectionCleared() {
  SelectElement(PasswordGenerationPopupElement::kNone);
  driver_->ClearPreviewedForm();
}

void PasswordGenerationPopupControllerImpl::SetSelected() {
  if (!IsSelectable()) {
    return;
  }
  SelectElement(PasswordGenerationPopupElement::kUseStrongPassword);
  driver_->PreviewGenerationSuggestion(current_generated_password_);
}

void PasswordGenerationPopupControllerImpl::EditPasswordClicked() {
  driver_->GeneratedPasswordAccepted(form_data_, generation_element_id_,
                                     current_generated_password_);
  Show(kEditGeneratedPassword);
}

void PasswordGenerationPopupControllerImpl::EditPasswordHovered(bool hovered) {
  SelectElement(hovered ? PasswordGenerationPopupElement::kEditPassword
                        : PasswordGenerationPopupElement::kNone);
  if (hovered) {
    driver_->PreviewGenerationSuggestion(current_generated_password_);
  } else {
    driver_->ClearPreviewedForm();
  }
}

#if !BUILDFLAG(IS_ANDROID)
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

bool PasswordGenerationPopupControllerImpl::ShouldShowNudgePassword() const {
  return state_ == kOfferGeneration &&
         kPasswordGenerationExperimentVariationParam.Get() ==
             PasswordGenerationVariation::kNudgePassword &&
         pref_service_ &&
         pref_service_->GetInteger(
             kPasswordGenerationNudgePasswordDismissCount) <
             kMaxAllowedAmountOfNudgePasswordPopupRejections;
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

base::i18n::TextDirection
PasswordGenerationPopupControllerImpl::GetElementTextDirection() const {
  return controller_common_.text_direction;
}

void PasswordGenerationPopupControllerImpl::HideImpl() {
  // Detach if the frame is still alive.
  if (driver_) {
    key_press_handler_manager_->RemoveKeyPressHandler();
    // The preview might have not been cleared (e.g. popup closed with ESC).
    driver_->ClearPreviewedForm();
  }

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
  return selected_element_ ==
         PasswordGenerationPopupElement::kUseStrongPassword;
}

bool PasswordGenerationPopupControllerImpl::edit_password_selected() const {
  return selected_element_ == PasswordGenerationPopupElement::kEditPassword;
}

const std::u16string& PasswordGenerationPopupControllerImpl::password() const {
  return current_generated_password_;
}

std::u16string PasswordGenerationPopupControllerImpl::SuggestedText() const {
  if (state_ == kEditGeneratedPassword) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_GENERATION_EDITING_SUGGESTION);
  }

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordGenerationExperiment)) {
    switch (kPasswordGenerationExperimentVariationParam.Get()) {
      case PasswordGenerationVariation::kTrustedAdvice:
        return l10n_util::GetStringUTF16(
            IDS_PASSWORD_GENERATION_SUGGESTION_TRUSTED_ADVICE);
      case PasswordGenerationVariation::kSafetyFirst:
        return l10n_util::GetStringUTF16(
            IDS_PASSWORD_GENERATION_SUGGESTION_SAFETY_FIRST);
      case PasswordGenerationVariation::kTrySomethingNew:
        return l10n_util::GetStringUTF16(
            IDS_PASSWORD_GENERATION_SUGGESTION_TRY_SOMETHING_NEW);
      case PasswordGenerationVariation::kConvenience:
        return l10n_util::GetStringUTF16(
            IDS_PASSWORD_GENERATION_SUGGESTION_CONVENIENCE);
      default:
        break;
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION_GPM);
}

const std::u16string& PasswordGenerationPopupControllerImpl::HelpText() const {
  return help_text_;
}
