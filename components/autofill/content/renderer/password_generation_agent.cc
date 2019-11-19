// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_generation_agent.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/rect.h"

using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;

namespace autofill {

namespace {

using autofill::form_util::GetTextDirectionForElement;
using Logger = autofill::SavePasswordProgressLogger;

// Returns the renderer id of the next password field in |control_elements|
// after |new_password|. This field is likely to be the confirmation field.
// Returns FormFieldData::kNotSetFormControlRendererId if there is no such
// field.
uint32_t FindConfirmationPasswordFieldId(
    const std::vector<WebFormControlElement>& control_elements,
    const WebFormControlElement& new_password) {
  auto iter =
      std::find(control_elements.begin(), control_elements.end(), new_password);

  if (iter == control_elements.end())
    return FormFieldData::kNotSetFormControlRendererId;

  ++iter;
  for (; iter != control_elements.end(); ++iter) {
    const WebInputElement* input_element = ToWebInputElement(&(*iter));
    if (input_element && input_element->IsPasswordFieldForAutofill())
      return input_element->UniqueRendererFormControlId();
  }
  return FormFieldData::kNotSetFormControlRendererId;
}

void CopyElementValueToOtherInputElements(
    const WebInputElement* element,
    std::vector<WebInputElement>* elements) {
  for (WebInputElement& it : *elements) {
    if (*element != it) {
      it.SetAutofillValue(element->Value());
    }
    it.SetAutofillState(WebAutofillState::kAutofilled);
  }
}

}  // namespace

// Contains information about generation status for an element for the
// lifetime of the possible interaction.
struct PasswordGenerationAgent::GenerationItemInfo {
  GenerationItemInfo(WebInputElement generation_element,
                     PasswordForm form,
                     std::vector<blink::WebInputElement> password_elements)
      : generation_element_(std::move(generation_element)),
        form_(std::move(form)),
        password_elements_(std::move(password_elements)) {}
  ~GenerationItemInfo() = default;

  // Element where we want to trigger password generation UI.
  blink::WebInputElement generation_element_;

  // Password form for the generation element.
  PasswordForm form_;

  // All the password elements in the form.
  std::vector<blink::WebInputElement> password_elements_;

  // If the password field at |generation_element_| contains a generated
  // password.
  bool password_is_generated_ = false;

  // True if the last password generation was manually triggered.
  bool is_manually_triggered_ = false;

  // True if a password was generated and the user edited it. Used for UMA
  // stats.
  bool password_edited_ = false;

  // True if the generation popup was shown during this navigation. Used to
  // track UMA stats per page visit rather than per display, since the former
  // is more interesting.
  // TODO(crbug.com/845458): Remove this or change the description of the
  // logged event as calling AutomaticgenerationStatusChanged will no longer
  // imply that a popup is shown. This could instead be logged with the
  // metrics collected on the browser process.
  bool generation_popup_shown_ = false;

  // True if the editing popup was shown during this navigation. Used to track
  // UMA stats per page rather than per display, since the former is more
  // interesting.
  bool editing_popup_shown_ = false;

  // True when PasswordGenerationAgent updates other password fields on the page
  // due to the generated password being edited. It's used to suppress the fake
  // blur events coming from there.
  bool updating_other_password_fileds_ = false;

  DISALLOW_COPY_AND_ASSIGN(GenerationItemInfo);
};

PasswordGenerationAgent::PasswordGenerationAgent(
    content::RenderFrame* render_frame,
    PasswordAutofillAgent* password_agent,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      mark_generation_element_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kShowAutofillSignatures)),
      password_agent_(password_agent) {
  registry->AddInterface(base::BindRepeating(
      &PasswordGenerationAgent::BindPendingReceiver, base::Unretained(this)));
  password_agent_->SetPasswordGenerationAgent(this);
}

PasswordGenerationAgent::~PasswordGenerationAgent() = default;

void PasswordGenerationAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::PasswordGenerationAgent>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void PasswordGenerationAgent::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  if (is_same_document_navigation)
    return;
  // Update stats for main frame navigation.
  if (!render_frame()->GetWebFrame()->Parent()) {
    if (current_generation_item_) {
      if (current_generation_item_->password_edited_) {
        password_generation::LogPasswordGenerationEvent(
            password_generation::PASSWORD_EDITED);
      }
      if (current_generation_item_->generation_popup_shown_) {
        password_generation::LogPasswordGenerationEvent(
            password_generation::GENERATION_POPUP_SHOWN);
      }
      if (current_generation_item_->editing_popup_shown_) {
        password_generation::LogPasswordGenerationEvent(
            password_generation::EDITING_POPUP_SHOWN);
      }
    }
  }
  current_generation_item_.reset();
  last_focused_password_element_.Reset();
  generation_enabled_fields_.clear();
}

void PasswordGenerationAgent::DidChangeScrollOffset() {
  if (!current_generation_item_)
    return;
  GetPasswordGenerationDriver()->FrameWasScrolled();
}

void PasswordGenerationAgent::OnDestruct() {
  receiver_.reset();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void PasswordGenerationAgent::OnFieldAutofilled(
    const WebInputElement& password_element) {
  if (current_generation_item_ &&
      current_generation_item_->password_is_generated_ &&
      current_generation_item_->generation_element_ == password_element) {
    password_generation::LogPasswordGenerationEvent(
        password_generation::PASSWORD_DELETED_BY_AUTOFILLING);
    PasswordNoLongerGenerated();
    current_generation_item_->generation_element_.SetShouldRevealPassword(
        false);
  }
}

bool PasswordGenerationAgent::ShouldIgnoreBlur() const {
  return current_generation_item_ &&
         current_generation_item_->updating_other_password_fileds_;
}

void PasswordGenerationAgent::GeneratedPasswordAccepted(
    const base::string16& password) {
  // static cast is workaround for linker error.
  DCHECK_LE(static_cast<size_t>(kMinimumLengthForEditedPassword),
            password.size());
  DCHECK(current_generation_item_);
  current_generation_item_->password_is_generated_ = true;
  current_generation_item_->password_edited_ = false;
  password_generation::LogPasswordGenerationEvent(
      password_generation::PASSWORD_ACCEPTED);
  LogMessage(Logger::STRING_GENERATION_RENDERER_GENERATED_PASSWORD_ACCEPTED);
  for (auto& password_element : current_generation_item_->password_elements_) {
    base::AutoReset<bool> auto_reset_update_confirmation_password(
        &current_generation_item_->updating_other_password_fileds_, true);
    password_element.SetAutofillValue(blink::WebString::FromUTF16(password));
    // setAutofillValue() above may have resulted in JavaScript closing the
    // frame.
    if (!render_frame())
      return;
    password_element.SetAutofillState(WebAutofillState::kAutofilled);
    // Advance focus to the next input field. We assume password fields in
    // an account creation form are always adjacent.
    render_frame()->GetRenderView()->GetWebView()->AdvanceFocus(false);
  }
  std::unique_ptr<PasswordForm> presaved_form(CreatePasswordFormToPresave());
  if (presaved_form) {
    DCHECK_NE(base::string16(), presaved_form->password_value);
    GetPasswordGenerationDriver()->PresaveGeneratedPassword(*presaved_form);
  }

  // Call UpdateStateForTextChange after the corresponding PasswordFormManager
  // is notified that the password was generated.
  for (auto& password_element : current_generation_item_->password_elements_) {
    // Needed to notify password_autofill_agent that the content of the field
    // has changed. Without this we will overwrite the generated
    // password with an Autofilled password when saving.
    // https://crbug.com/493455
    password_agent_->UpdateStateForTextChange(password_element);
  }
}

std::unique_ptr<PasswordForm>
PasswordGenerationAgent::CreatePasswordFormToPresave() {
  DCHECK(current_generation_item_);
  DCHECK(!current_generation_item_->generation_element_.IsNull());
  // Since the form for presaving should match a form in the browser, create it
  // with the same algorithm (to match html attributes, action, etc.), but
  // change username and password values.
  std::unique_ptr<PasswordForm> password_form;
  if (!current_generation_item_->generation_element_.Form().IsNull()) {
    password_form = password_agent_->GetSimplifiedPasswordFormFromWebForm(
        current_generation_item_->generation_element_.Form());
  } else {
    password_form =
        password_agent_->GetSimplifiedPasswordFormFromUnownedInputElements();
  }
  if (password_form) {
    password_form->type = PasswordForm::Type::kGenerated;
    password_form->password_value =
        current_generation_item_->generation_element_.Value().Utf16();
  }

  return password_form;
}

void PasswordGenerationAgent::FoundFormEligibleForGeneration(
    const PasswordFormGenerationData& form) {
  generation_enabled_fields_[form.new_password_renderer_id] = form;

  // Mark the input element as |has_been_password_for_autofill_|.
  if (mark_generation_element_) {
    if (!render_frame())
      return;
    WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
    if (doc.IsNull())
      return;
    WebFormControlElement new_password_input =
        form_util::FindFormControlElementByUniqueRendererId(
            doc, form.new_password_renderer_id);
    if (!new_password_input.IsNull()) {
      // Mark the input element with renderer id
      // |form.new_password_renderer_id|.
      new_password_input.SetAttribute("password_creation_field", "1");
    }
  }
}

void PasswordGenerationAgent::UserTriggeredGeneratePassword(
    UserTriggeredGeneratePasswordCallback callback) {
  if (SetUpUserTriggeredGeneration()) {
    LogMessage(Logger::STRING_GENERATION_RENDERER_SHOW_MANUAL_GENERATION_POPUP);
    // If the field is not |type=password|, the list of suggestions
    // should not be populated with passwords to avoid filling them in a
    // clear-text field.
    // |IsPasswordFieldForAutofill()| is deliberately not used.
    bool is_generation_element_password_type =
        current_generation_item_->generation_element_.IsPasswordField();
    autofill::password_generation::PasswordGenerationUIData
        password_generation_ui_data(
            render_frame()->ElementBoundsInWindow(
                current_generation_item_->generation_element_),
            current_generation_item_->generation_element_.MaxLength(),
            current_generation_item_->generation_element_.NameForAutofill()
                .Utf16(),
            current_generation_item_->generation_element_
                .UniqueRendererFormControlId(),
            is_generation_element_password_type,
            GetTextDirectionForElement(
                current_generation_item_->generation_element_),
            current_generation_item_->form_);
    std::move(callback).Run(std::move(password_generation_ui_data));
    current_generation_item_->generation_popup_shown_ = true;
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

bool PasswordGenerationAgent::SetUpUserTriggeredGeneration() {
  if (last_focused_password_element_.IsNull() || !render_frame())
    return false;

  uint32_t last_focused_password_element_id =
      last_focused_password_element_.UniqueRendererFormControlId();

  bool is_automatic_generation_available = base::Contains(
      generation_enabled_fields_, last_focused_password_element_id);

  if (!is_automatic_generation_available) {
    WebFormElement form = last_focused_password_element_.Form();
    std::vector<WebFormControlElement> control_elements;
    if (!form.IsNull()) {
      control_elements = form_util::ExtractAutofillableElementsInForm(form);
    } else {
      const WebLocalFrame& frame = *render_frame()->GetWebFrame();
      blink::WebDocument doc = frame.GetDocument();
      if (doc.IsNull())
        return false;
      control_elements =
          form_util::GetUnownedFormFieldElements(doc.All(), nullptr);
    }

    MaybeCreateCurrentGenerationItem(
        last_focused_password_element_,
        FindConfirmationPasswordFieldId(control_elements,
                                        last_focused_password_element_));
  } else {
    auto it = generation_enabled_fields_.find(last_focused_password_element_id);
    MaybeCreateCurrentGenerationItem(
        last_focused_password_element_,
        it->second.confirmation_password_renderer_id);
  }

  if (!current_generation_item_)
    return false;

  if (current_generation_item_->generation_element_ !=
      last_focused_password_element_) {
    return false;
  }

  current_generation_item_->is_manually_triggered_ =
      !is_automatic_generation_available;
  return true;
}

bool PasswordGenerationAgent::FocusedNodeHasChanged(
    const blink::WebNode& node) {
  if (node.IsNull() || !node.IsElementNode()) {
    return false;
  }

  const blink::WebElement web_element = node.ToConst<blink::WebElement>();
  if (!web_element.GetDocument().GetFrame()) {
    return false;
  }

  const WebInputElement* element = ToWebInputElement(&web_element);
  if (!element)
    return false;

  if (element->IsPasswordFieldForAutofill())
    last_focused_password_element_ = *element;

  auto it =
      generation_enabled_fields_.find(element->UniqueRendererFormControlId());
  if (it != generation_enabled_fields_.end()) {
    MaybeCreateCurrentGenerationItem(
        *element, it->second.confirmation_password_renderer_id);
  }
  if (!current_generation_item_ ||
      *element != current_generation_item_->generation_element_) {
    return false;
  }

  if (current_generation_item_->password_is_generated_) {
    if (current_generation_item_->generation_element_.Value().length() <
        kMinimumLengthForEditedPassword) {
      PasswordNoLongerGenerated();
      MaybeOfferAutomaticGeneration();
      if (current_generation_item_->generation_element_.Value().IsEmpty())
        current_generation_item_->generation_element_.SetShouldRevealPassword(
            false);
    } else {
      current_generation_item_->generation_element_.SetShouldRevealPassword(
          true);
      ShowEditingPopup();
    }
    return true;
  }

  // Assume that if the password field has less than
  // |kMaximumCharsForGenerationOffer| characters then the user is not finished
  // typing their password and display the password suggestion.
  if (!element->IsReadOnly() && element->IsEnabled() &&
      element->Value().length() <= kMaximumCharsForGenerationOffer) {
    MaybeOfferAutomaticGeneration();
    return true;
  }

  return false;
}

void PasswordGenerationAgent::DidEndTextFieldEditing(
    const blink::WebInputElement& element) {
  if (!element.IsNull() && current_generation_item_ &&
      element == current_generation_item_->generation_element_) {
    GetPasswordGenerationDriver()->GenerationElementLostFocus();
    current_generation_item_->generation_element_.SetShouldRevealPassword(
        false);
  }
}

bool PasswordGenerationAgent::TextDidChangeInTextField(
    const WebInputElement& element) {
  if (!(current_generation_item_ &&
        current_generation_item_->generation_element_ == element)) {
    // Presave the username if it has been changed.
    if (current_generation_item_ &&
        current_generation_item_->password_is_generated_ && !element.IsNull() &&
        element.Form() ==
            current_generation_item_->generation_element_.Form()) {
      std::unique_ptr<PasswordForm> presaved_form(
          CreatePasswordFormToPresave());
      if (presaved_form)
        GetPasswordGenerationDriver()->PresaveGeneratedPassword(*presaved_form);
    }
    return false;
  }

  if (element.Value().IsEmpty()) {
    current_generation_item_->generation_element_.SetShouldRevealPassword(
        false);
  }

  if (!current_generation_item_->password_is_generated_ &&
      element.Value().length() > kMaximumCharsForGenerationOffer) {
    // User has rejected the feature and has started typing a password.
    GenerationRejectedByTyping();
  } else {
    const bool leave_editing_state =
        current_generation_item_->password_is_generated_ &&
        element.Value().length() < kMinimumLengthForEditedPassword;
    if (!current_generation_item_->password_is_generated_ ||
        leave_editing_state) {
      // The call may pop up a generation prompt, replacing the editing prompt
      // if it was previously shown.
      MaybeOfferAutomaticGeneration();
    }
    if (leave_editing_state) {
      // Tell the browser that the state isn't "editing" anymore. The browser
      // should hide the editing prompt if it wasn't replaced above.
      PasswordNoLongerGenerated();
    } else if (current_generation_item_->password_is_generated_) {
      current_generation_item_->password_edited_ = true;
      base::AutoReset<bool> auto_reset_update_confirmation_password(
          &current_generation_item_->updating_other_password_fileds_, true);
      // Mirror edits to any confirmation password fields.
      CopyElementValueToOtherInputElements(
          &element, &current_generation_item_->password_elements_);
      std::unique_ptr<PasswordForm> presaved_form(
          CreatePasswordFormToPresave());
      if (presaved_form) {
        GetPasswordGenerationDriver()->PresaveGeneratedPassword(*presaved_form);
      }
    }
  }
  return true;
}

void PasswordGenerationAgent::MaybeOfferAutomaticGeneration() {
  // TODO(crbug.com/852309): Add this check to the generation element class.
  if (!current_generation_item_->is_manually_triggered_) {
    AutomaticGenerationAvailable();
  }
}

void PasswordGenerationAgent::AutomaticGenerationAvailable() {
  if (!render_frame())
    return;
  DCHECK(current_generation_item_);
  DCHECK(!current_generation_item_->generation_element_.IsNull());
  LogMessage(Logger::STRING_GENERATION_RENDERER_AUTOMATIC_GENERATION_AVAILABLE);
  // If the field is not |type=password|, the list of suggestions
  // should not be populated with passwordS to avoid filling them in a
  // clear-text field.
  // |IsPasswordFieldForAutofill()| is deliberately not used.
  bool is_generation_element_password_type =
      current_generation_item_->generation_element_.IsPasswordField();
  autofill::password_generation::PasswordGenerationUIData
      password_generation_ui_data(
          render_frame()->ElementBoundsInWindow(
              current_generation_item_->generation_element_),
          current_generation_item_->generation_element_.MaxLength(),
          current_generation_item_->generation_element_.NameForAutofill()
              .Utf16(),
          current_generation_item_->generation_element_
              .UniqueRendererFormControlId(),
          is_generation_element_password_type,
          GetTextDirectionForElement(
              current_generation_item_->generation_element_),
          current_generation_item_->form_);
  current_generation_item_->generation_popup_shown_ = true;
  GetPasswordGenerationDriver()->AutomaticGenerationAvailable(
      password_generation_ui_data);
}

void PasswordGenerationAgent::ShowEditingPopup() {
  if (!render_frame())
    return;

  gfx::RectF bounding_box = render_frame()->ElementBoundsInWindow(
      current_generation_item_->generation_element_);

  std::unique_ptr<PasswordForm> password_form = CreatePasswordFormToPresave();
  DCHECK(password_form);

  uint32_t generation_element_renderer_id =
      current_generation_item_->generation_element_
          .UniqueRendererFormControlId();

  GetPasswordGenerationDriver()->ShowPasswordEditingPopup(
      bounding_box, *password_form, generation_element_renderer_id);
  current_generation_item_->editing_popup_shown_ = true;
}

void PasswordGenerationAgent::GenerationRejectedByTyping() {
  GetPasswordGenerationDriver()->PasswordGenerationRejectedByTyping();
}

void PasswordGenerationAgent::PasswordNoLongerGenerated() {
  DCHECK(current_generation_item_);
  DCHECK(current_generation_item_->password_is_generated_);
  // Do not treat the password as generated, either here or in the browser.
  current_generation_item_->password_is_generated_ = false;
  current_generation_item_->password_edited_ = false;
  for (WebInputElement& password : current_generation_item_->password_elements_)
    password.SetAutofillState(WebAutofillState::kNotFilled);
  password_generation::LogPasswordGenerationEvent(
      password_generation::PASSWORD_DELETED);
  // Clear all other password fields.
  for (WebInputElement& element :
       current_generation_item_->password_elements_) {
    base::AutoReset<bool> auto_reset_update_confirmation_password(
        &current_generation_item_->updating_other_password_fileds_, true);
    if (current_generation_item_->generation_element_ != element)
      element.SetAutofillValue(blink::WebString());
  }
  std::unique_ptr<PasswordForm> presaved_form(CreatePasswordFormToPresave());
  if (presaved_form)
    GetPasswordGenerationDriver()->PasswordNoLongerGenerated(*presaved_form);
}

void PasswordGenerationAgent::MaybeCreateCurrentGenerationItem(
    WebInputElement generation_element,
    uint32_t confirmation_password_renderer_id) {
  // Do not create |current_generation_item_| if it already is created for
  // |generation_element| or the user accepted generated password. So if the
  // user accepted the generated password, generation is not offered on any
  // other field.
  if (current_generation_item_ &&
      (current_generation_item_->generation_element_ == generation_element ||
       current_generation_item_->password_is_generated_))
    return;

  std::unique_ptr<PasswordForm> password_form =
      generation_element.Form().IsNull()
          ? password_agent_->GetSimplifiedPasswordFormFromUnownedInputElements()
          : password_agent_->GetSimplifiedPasswordFormFromWebForm(
                generation_element.Form());

  if (!password_form)
    return;

  std::vector<blink::WebInputElement> passwords = {generation_element};

  WebFormControlElement confirmation_password =
      form_util::FindFormControlElementByUniqueRendererId(
          generation_element.GetDocument(), confirmation_password_renderer_id);

  if (!confirmation_password.IsNull()) {
    WebInputElement* input = ToWebInputElement(&confirmation_password);
    if (input)
      passwords.push_back(*input);
  }

  current_generation_item_.reset(new GenerationItemInfo(
      generation_element, std::move(*password_form), std::move(passwords)));

  generation_element.SetHasBeenPasswordField();

  generation_element.SetAttribute("aria-autocomplete", "list");
}

const mojo::AssociatedRemote<mojom::PasswordManagerDriver>&
PasswordGenerationAgent::GetPasswordManagerDriver() {
  DCHECK(password_agent_);
  return password_agent_->GetPasswordManagerDriver();
}

const mojo::AssociatedRemote<mojom::PasswordGenerationDriver>&
PasswordGenerationAgent::GetPasswordGenerationDriver() {
  if (!password_generation_client_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &password_generation_client_);
  }

  return password_generation_client_;
}

void PasswordGenerationAgent::LogMessage(Logger::StringID message_id) {
  if (!password_agent_->logging_state_active())
    return;
  RendererSavePasswordProgressLogger logger(GetPasswordManagerDriver().get());
  logger.LogMessage(message_id);
}

void PasswordGenerationAgent::LogBoolean(Logger::StringID message_id,
                                         bool truth_value) {
  if (!password_agent_->logging_state_active())
    return;
  RendererSavePasswordProgressLogger logger(GetPasswordManagerDriver().get());
  logger.LogBoolean(message_id, truth_value);
}

}  // namespace autofill
