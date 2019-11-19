// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/i18n/rtl.h"
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
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/text_utils.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/preferences/preferences_launcher.h"
#endif

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
  content::RenderFrameHost* const frame_;
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
    previous->Hide();

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
      form_(ui_data.password_form),
      driver_(driver),
      observer_(observer),
      form_signature_(autofill::CalculateFormSignature(form_.form_data)),
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
#if !defined(OS_ANDROID)
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  // There may not always be a ZoomController, e.g. in tests.
  if (zoom_controller) {
    zoom_controller->AddObserver(this);
  }
#endif  // !defined(OS_ANDROID)

  help_text_ = l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROMPT);
}

PasswordGenerationPopupControllerImpl::
    ~PasswordGenerationPopupControllerImpl() {
#if !defined(OS_ANDROID)
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents());
  if (zoom_controller) {
    zoom_controller->RemoveObserver(this);
  }
#endif  // !defined(OS_ANDROID)
}

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
      Hide();
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
  driver_->GeneratedPasswordAccepted(form_.form_data, generation_element_id_,
                                     current_password_);
  // |this| can be destroyed here because GeneratedPasswordAccepted pops up
  // another UI and generates some event to close the dropdown.
  if (weak_this)
    weak_this->Hide();
}

void PasswordGenerationPopupControllerImpl::Show(GenerationUIState state) {
  // When switching from editing to generation state, regenerate the password.
  if (state == kOfferGeneration &&
      (state_ != state || current_password_.empty())) {
    uint32_t spec_priority = 0;
    current_password_ =
        driver_->GetPasswordGenerationHelper()->GeneratePassword(
            web_contents()->GetLastCommittedURL().GetOrigin(), form_signature_,
            field_signature_, max_length_, &spec_priority);
  }
  state_ = state;

  if (!view_) {
    view_ = PasswordGenerationPopupView::Create(this);

    // Treat popup as being hidden if creation fails.
    if (!view_) {
      Hide();
      return;
    }
    key_press_handler_manager_->RegisterKeyPressHandler(base::BindRepeating(
        &PasswordGenerationPopupControllerImpl::HandleKeyPressEvent,
        base::Unretained(this)));
    view_->Show();
  } else {
    view_->UpdateState();
    view_->UpdateBoundsAndRedrawPopup();
  }

  if (observer_)
    observer_->OnPopupShown(state_);
}

void PasswordGenerationPopupControllerImpl::UpdatePassword(
    base::string16 new_password) {
  current_password_ = std::move(new_password);
  if (view_)
    view_->UpdatePasswordValue();
}

void PasswordGenerationPopupControllerImpl::FrameWasScrolled() {
  Hide();
}

void PasswordGenerationPopupControllerImpl::GenerationElementLostFocus() {
  Hide();
}

void PasswordGenerationPopupControllerImpl::GeneratedPasswordRejected() {
  Hide();
}

void PasswordGenerationPopupControllerImpl::DidAttachInterstitialPage() {
  Hide();
}

void PasswordGenerationPopupControllerImpl::WebContentsDestroyed() {
  Hide();
}

#if !defined(OS_ANDROID)
void PasswordGenerationPopupControllerImpl::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  Hide();
}
#endif  // !defined(OS_ANDROID)

void PasswordGenerationPopupControllerImpl::Hide() {
  // Detach if the frame is still alive.
  if (driver_)
    key_press_handler_manager_->RemoveKeyPressHandler();

  if (view_)
    view_->Hide();

  if (observer_)
    observer_->OnPopupHidden();

  delete this;
}

void PasswordGenerationPopupControllerImpl::ViewDestroyed() {
  view_ = NULL;

  Hide();
}

void PasswordGenerationPopupControllerImpl::SetSelectionAtPoint(
    const gfx::Point& point) {
  PasswordSelected(view_->IsPointInPasswordBounds(point));
}

bool PasswordGenerationPopupControllerImpl::AcceptSelectedLine() {
  if (!password_selected_)
    return false;

  PasswordAccepted();
  return true;
}

void PasswordGenerationPopupControllerImpl::SelectionCleared() {
  PasswordSelected(false);
}

bool PasswordGenerationPopupControllerImpl::HasSelection() const {
  return password_selected();
}

gfx::NativeView PasswordGenerationPopupControllerImpl::container_view() const {
  return controller_common_.container_view;
}

gfx::Rect PasswordGenerationPopupControllerImpl::popup_bounds() const {
  NOTREACHED();
  return gfx::Rect();
}

const gfx::RectF& PasswordGenerationPopupControllerImpl::element_bounds()
    const {
  return controller_common_.element_bounds;
}

bool PasswordGenerationPopupControllerImpl::IsRTL() const {
  return base::i18n::IsRTL();
}

const std::vector<autofill::Suggestion>
PasswordGenerationPopupControllerImpl::GetSuggestions() {
  return std::vector<autofill::Suggestion>();
}

#if !defined(OS_ANDROID)
int PasswordGenerationPopupControllerImpl::GetElidedValueWidthForRow(int row) {
  return 0;
}

int PasswordGenerationPopupControllerImpl::GetElidedLabelWidthForRow(int row) {
  return 0;
}
#endif

PasswordGenerationPopupController::GenerationUIState
PasswordGenerationPopupControllerImpl::state() const {
  return state_;
}

bool PasswordGenerationPopupControllerImpl::password_selected() const {
  return password_selected_;
}

const base::string16& PasswordGenerationPopupControllerImpl::password() const {
  return current_password_;
}

base::string16 PasswordGenerationPopupControllerImpl::SuggestedText() {
  return l10n_util::GetStringUTF16(
      state_ == kOfferGeneration ? IDS_PASSWORD_GENERATION_SUGGESTION
                                 : IDS_PASSWORD_GENERATION_EDITING_SUGGESTION);
}

const base::string16& PasswordGenerationPopupControllerImpl::HelpText() {
  return help_text_;
}
