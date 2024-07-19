// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "url/gurl.h"

namespace autofill {

class PasswordAutofillAgent;

// This class is responsible for controlling communication for password
// generation between the browser (which shows the popup and generates
// passwords) and WebKit (shows the generation icon in the password field).
class PasswordGenerationAgent : public content::RenderFrameObserver,
                                public mojom::PasswordGenerationAgent {
 public:
  // Maximum number of characters typed by user while the generation is still
  // offered. When the (kMaximumCharsForGenerationOffer + 1)-th character is
  // typed, the generation becomes unavailable.
  static const size_t kMaximumCharsForGenerationOffer = 5;

  // User can edit the generated password. If the length falls below this value,
  // the password is no longer considered generated.
  static const size_t kMinimumLengthForEditedPassword = 4;

  PasswordGenerationAgent(content::RenderFrame* render_frame,
                          PasswordAutofillAgent* password_agent,
                          blink::AssociatedInterfaceRegistry* registry);

  PasswordGenerationAgent(const PasswordGenerationAgent&) = delete;
  PasswordGenerationAgent& operator=(const PasswordGenerationAgent&) = delete;

  ~PasswordGenerationAgent() override;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::PasswordGenerationAgent>
          pending_receiver);

  // mojom::PasswordGenerationAgent:
  void GeneratedPasswordAccepted(const std::u16string& password) override;
  void FoundFormEligibleForGeneration(
      const PasswordFormGenerationData& form) override;
  // Sets |generation_element_| to the focused password field and responds back
  // if the generation was triggered successfully.
  void TriggeredGeneratePassword(
      TriggeredGeneratePasswordCallback callback) override;
  void FocusNextFieldAfterPasswords() override;

  // Returns true if the field being changed is one where a generated password
  // is being offered. Updates the state of the popup if necessary.
  bool TextDidChangeInTextField(const blink::WebInputElement& element);

  // Returns true if the newly focused node caused the generation UI to show.
  bool ShowPasswordGenerationSuggestions(const blink::WebInputElement& element);

  // Event forwarded by AutofillAgent from WebAutofillClient, informing that
  // the text field editing has ended, which means that the field is not
  // focused anymore. This is required for Android, where moving focus
  // to a non-focusable element doesn't result in |FocusedNodeHasChanged|
  // being called.
  void DidEndTextFieldEditing(const blink::WebInputElement& element);

  // Event forwarded by AutofillAgent from WebAutofillClient, informing that the
  // text field was cleared. For password fields this means that they are no
  // longer generated and should be masked.
  void TextFieldCleared(const blink::WebInputElement& element);

  // Called right before PasswordAutofillAgent filled |password_element|.
  void OnFieldAutofilled(const blink::WebInputElement& password_element);

  // Returns true iff the currently handled 'blur' event is fake and should be
  // ignored.
  bool ShouldIgnoreBlur() const;

#if defined(UNIT_TEST)
  // This method requests the mojom::PasswordManagerClient which binds
  // requests the binding if it wasn't bound yet.
  void RequestPasswordManagerClientForTesting() {
    GetPasswordGenerationDriver();
  }
#endif

  bool IsPrerendering() const;

  // Previews the generation suggestion for the current generation element.
  void PreviewGenerationSuggestion(const std::u16string& password);

  // Clears the previewed field if it was previously previewed.
  void ClearPreviewedForm();

 private:
  class DeferringPasswordGenerationDriver;

  // Contains information about generation status for an element for the
  // lifetime of the possible interaction.
  struct GenerationItemInfo;

  // RenderFrameObserver:
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidChangeScrollOffset() override;
  void OnDestruct() override;

  mojom::PasswordManagerDriver& GetPasswordManagerDriver();

  // Callers should not store the returned value longer than a function scope.
  mojom::PasswordGenerationDriver& GetPasswordGenerationDriver();

  // Helper function which takes care of the form processing and collecting the
  // information which is required to show the generation popup. Returns true if
  // all required information is collected.
  bool SetUpTriggeredGeneration();

  // This is called whenever automatic generation could be offered, and returns
  // true if generation was offered.
  // If manual generation was already requested, automatic generation is not
  // offered.
  bool MaybeOfferAutomaticGeneration();

  // Signals the browser that it should offer automatic password generation
  // as a result of the user focusing a password field eligible for generation.
  void AutomaticGenerationAvailable();

  // Show UI for editing a generated password at |generation_element_|.
  void ShowEditingPopup();

  // Signals the browser that generation was rejected. This happens when the
  // user types more characters than the maximum offer size into the password
  // field. Upon receiving this message, the browser can choose to hide the
  // generation UI or not, depending on the platform.
  void GenerationRejectedByTyping();

  // Stops treating a password as generated.
  void PasswordNoLongerGenerated();

  // Creates |current_generation_item_| for |element| if |element| is a
  // generation enabled element. If |current_generation_item_| is already
  // created for |element| it is not recreated.
  void MaybeCreateCurrentGenerationItem(
      blink::WebInputElement element,
      FieldRendererId confirmation_password_renderer_id);

  void LogMessage(SavePasswordProgressLogger::StringID message_id);
  void LogBoolean(SavePasswordProgressLogger::StringID message_id,
                  bool truth_value);

  // Creates a FormData to presave a generated password. It copies behavior
  // of CreateFromDataFromWebForm/FromUnownedInputElements. If a form
  // creating is failed, returns an empty unique_ptr.
  std::optional<FormData> CreateFormDataToPresave();

  // Contains the current element where generation is offered at the moment. It
  // can be either automatic or manual password generation.
  std::unique_ptr<GenerationItemInfo> current_generation_item_;

  // Contains correspondence between generation enabled element and data for
  // generation.
  std::map<FieldRendererId, PasswordFormGenerationData>
      generation_enabled_fields_;

  // True iff the generation element should be marked with special HTML
  // attribute (only for experimental purposes).
  const bool mark_generation_element_;

  // Unowned pointer. Used to notify PassowrdAutofillAgent when values
  // in password fields are updated.
  const raw_ptr<PasswordAutofillAgent> password_agent_;

  mojo::AssociatedRemote<mojom::PasswordGenerationDriver>
      password_generation_client_;

  // Used for deferring messages while prerendering.
  std::unique_ptr<DeferringPasswordGenerationDriver>
      deferring_password_generation_driver_;

  mojo::AssociatedReceiver<mojom::PasswordGenerationAgent> receiver_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
