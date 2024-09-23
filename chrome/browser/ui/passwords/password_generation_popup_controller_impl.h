// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/zoom/zoom_observer.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {
class WebContents;
}  // namespace content

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

namespace autofill {
class FormData;
namespace password_generation {
enum class PasswordGenerationType;
struct PasswordGenerationUIData;
}  // namespace password_generation
}  // namespace autofill

class PasswordGenerationPopupObserver;
class PasswordGenerationPopupView;

// This class controls a PasswordGenerationPopupView. It is responsible for
// determining the location of the popup, handling keypress events while the
// popup is active, and notifying both the renderer and the password manager
// if the password is accepted.
//
// NOTE: This is used on Android only to display the editing popup.
//
// TODO(crbug.com/40619484): Clean up the popup code on Android to make its use
// clear and remove unused code.
class PasswordGenerationPopupControllerImpl
    : public PasswordGenerationPopupController,
      public content::WebContentsObserver
#if !BUILDFLAG(IS_ANDROID)
    ,
      public zoom::ZoomObserver
#endif  // !BUILDFLAG(IS_ANDROID)
{
 public:
  // Create a controller or return |previous| if it is suitable. Will hide
  // |previous| if it is not returned. |bounds| is the bounds of the element
  // that we are showing the dropdown for in screen space. |ui_data| contains
  // parameters for generation a passwords. If not NULL, |observer| will be
  // notified of changes of the popup state.
  static base::WeakPtr<PasswordGenerationPopupControllerImpl> GetOrCreate(
      base::WeakPtr<PasswordGenerationPopupControllerImpl> previous,
      const gfx::RectF& bounds,
      const autofill::password_generation::PasswordGenerationUIData& ui_data,
      const base::WeakPtr<password_manager::PasswordManagerDriver>& driver,
      PasswordGenerationPopupObserver* observer,
      content::WebContents* web_contents,
      content::RenderFrameHost* frame);

  PasswordGenerationPopupControllerImpl(
      const PasswordGenerationPopupControllerImpl&) = delete;
  PasswordGenerationPopupControllerImpl& operator=(
      const PasswordGenerationPopupControllerImpl&) = delete;

  ~PasswordGenerationPopupControllerImpl() override;

  // Generate the password string and store it in `current_generated_password_`.
  void GeneratePasswordValue(
      autofill::password_generation::PasswordGenerationType generation_type);

  // Create a PasswordGenerationPopupView if one doesn't already exist.
  void Show(GenerationUIState state);

  // Update the value of the generated password to be displayed in the UI (e.g.
  // upon editing the generated password).
  void UpdateGeneratedPassword(std::u16string new_password);

  // Hides the popup, since its position is no longer valid.
  void FrameWasScrolled();

  // Hides the popup, since the generation element for which it was shown
  // is no longer focused.
  void GenerationElementLostFocus();

  // The generated password counts as rejected either if the user ignores the
  // popup and types a password, or if the generated password is deleted.
  // In both cases the popups should be hidden. In the latter case, a new popup
  // might be shown offering another password if generation is offered
  // automatically on that field.
  void GeneratedPasswordRejected();

  base::WeakPtr<PasswordGenerationPopupControllerImpl> GetWeakPtr();

  // content::WebContentsObserver overrides
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

  // Returns true if the popup is visible, or false otherwise.
  bool IsVisible() const;

#if !BUILDFLAG(IS_ANDROID)
  // ZoomObserver:
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
#endif

#if defined(UNIT_TEST)
  PasswordGenerationPopupView* view() const { return view_; }
  void SetViewForTesting(PasswordGenerationPopupView* view) { view_ = view; }
  void SelectAcceptButtonForTesting() {
    SelectElement(PasswordGenerationPopupElement::kNudgePasswordAcceptButton);
  }
  void SelectCancelButtonForTesting() {
    SelectElement(PasswordGenerationPopupElement::kNudgePasswordCancelButton);
  }
#endif

 protected:
  PasswordGenerationPopupControllerImpl(
      const gfx::RectF& bounds,
      const autofill::password_generation::PasswordGenerationUIData& ui_data,
      const base::WeakPtr<password_manager::PasswordManagerDriver>& driver,
      PasswordGenerationPopupObserver* observer,
      content::WebContents* web_contents,
      content::RenderFrameHost* frame);

 private:
  class KeyPressRegistrator;

  // Defines different elements of the popup that can be selected.
  enum class PasswordGenerationPopupElement {
    kNone = 0,
    kUseStrongPassword = 1,
    kNudgePasswordAcceptButton = 2,
    kNudgePasswordCancelButton = 3,
  };

  // AutofillPopupViewDelegate implementation:
  void Hide(autofill::SuggestionHidingReason) override;
  void ViewDestroyed() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  autofill::PopupAnchorType anchor_type() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;

  // PasswordGenerationPopupController implementation:
  void PasswordAccepted() override;
  void SetSelected() override;
  void SelectionCleared() override;
#if !BUILDFLAG(IS_ANDROID)
  std::u16string GetPrimaryAccountEmail() override;
  bool ShouldShowNudgePassword() const override;
#endif  // !BUILDFLAG(IS_ANDROID)
  GenerationUIState state() const override;
  bool password_selected() const override;
  bool accept_button_selected() const override;
  bool cancel_button_selected() const override;
  const std::u16string& password() const override;
  std::u16string SuggestedText() const override;
  const std::u16string& HelpText() const override;

  void HideImpl();

  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event);

  // Whether the elements of popup are selectable (true in generation state).
  bool IsSelectable() const;
  // Sets currently selected popup element.
  void SelectElement(PasswordGenerationPopupElement element);
  // Accepts currently selected element. No-op if no element is selected.
  bool PossiblyAcceptSelectedElement();

  // Handle to the popup. May be NULL if popup isn't showing.
  raw_ptr<PasswordGenerationPopupView> view_;

  const autofill::FormData form_data_;

  base::WeakPtr<password_manager::PasswordManagerDriver> const driver_;

  // May be NULL.
  const raw_ptr<PasswordGenerationPopupObserver> observer_;

  // Signature of the form for which password generation is triggered.
  const autofill::FormSignature form_signature_;

  // Signature of the field for which password generation is triggered.
  const autofill::FieldSignature field_signature_;

  // Renderer ID of the generation element.
  const autofill::FieldRendererId generation_element_id_;

  // Maximum length of the password to be generated. 0 represents an unbound
  // maximum length.
  const uint32_t max_length_;

  // Contains common popup data.
  const autofill::PopupControllerCommon controller_common_;

  // Help text in the footer.
  std::u16string help_text_;

  // The current password that is considered generated. This is the password to
  // be displayed in the user generation dialog.
  std::u16string current_generated_password_;

  // Currently selected / highlighted element of the popup.
  PasswordGenerationPopupElement selected_element_ =
      PasswordGenerationPopupElement::kNone;

  // The state of the generation popup.
  GenerationUIState state_;

  std::unique_ptr<KeyPressRegistrator> key_press_handler_manager_;

#if !BUILDFLAG(IS_ANDROID)
  base::ScopedObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observation_{this};
#endif

  base::WeakPtrFactory<PasswordGenerationPopupControllerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_IMPL_H_
