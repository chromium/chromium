// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_IMPL_H_

#include <stddef.h>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "components/autofill/content/browser/key_press_handler_manager.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/signatures_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

#if !defined(OS_ANDROID)
#include "components/zoom/zoom_observer.h"
#endif  // !defined(OS_ANDROID)

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}  // namespace content

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

namespace autofill {
struct PasswordForm;
struct Suggestion;
namespace password_generation {
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
// TODO(crbug.com/944502): Clean up the popup code on Android to make its use
// clear and remove unused code.
class PasswordGenerationPopupControllerImpl
    : public PasswordGenerationPopupController,
      public content::WebContentsObserver
#if !defined(OS_ANDROID)
    ,
      public zoom::ZoomObserver
#endif  // !defined(OS_ANDROID)
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
  ~PasswordGenerationPopupControllerImpl() override;

  // Create a PasswordGenerationPopupView if one doesn't already exist.
  void Show(GenerationUIState state);

  // Update the password to be displayed in the UI.
  void UpdatePassword(base::string16 new_password);

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

  // content::WebContentsObserver overrides
  void DidAttachInterstitialPage() override;
  void WebContentsDestroyed() override;

#if !defined(OS_ANDROID)
  // ZoomObserver implementation.
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
#endif  // !defined(OS_ANDROID)

 protected:
  PasswordGenerationPopupControllerImpl(
      const gfx::RectF& bounds,
      const autofill::password_generation::PasswordGenerationUIData& ui_data,
      const base::WeakPtr<password_manager::PasswordManagerDriver>& driver,
      PasswordGenerationPopupObserver* observer,
      content::WebContents* web_contents,
      content::RenderFrameHost* frame);

  // Handle to the popup. May be NULL if popup isn't showing.
  PasswordGenerationPopupView* view_;

 private:
  class KeyPressRegistrator;
  // PasswordGenerationPopupController implementation:
  void Hide() override;
  void ViewDestroyed() override;
  void SetSelectionAtPoint(const gfx::Point& point) override;
  bool AcceptSelectedLine() override;
  void SelectionCleared() override;
  bool HasSelection() const override;
  void PasswordAccepted() override;
  gfx::NativeView container_view() const override;
  gfx::Rect popup_bounds() const override;
  const gfx::RectF& element_bounds() const override;
  bool IsRTL() const override;
  const std::vector<autofill::Suggestion> GetSuggestions() override;
#if !defined(OS_ANDROID)
  int GetElidedValueWidthForRow(int row) override;
  int GetElidedLabelWidthForRow(int row) override;
#endif

  GenerationUIState state() const override;
  bool password_selected() const override;
  const base::string16& password() const override;
  base::string16 SuggestedText() override;
  const base::string16& HelpText() override;

  base::WeakPtr<PasswordGenerationPopupControllerImpl> GetWeakPtr();

  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

  // Set if the password is currently selected.
  void PasswordSelected(bool selected);

  // Accept password if it's selected.
  bool PossiblyAcceptPassword();

  const autofill::PasswordForm form_;

  base::WeakPtr<password_manager::PasswordManagerDriver> const driver_;

  // May be NULL.
  PasswordGenerationPopupObserver* const observer_;

  // Signature of the form for which password generation is triggered.
  const autofill::FormSignature form_signature_;

  // Signature of the field for which password generation is triggered.
  const autofill::FieldSignature field_signature_;

  // Renderer ID of the generation element.
  const uint32_t generation_element_id_;

  // Maximum length of the password to be generated. 0 represents an unbound
  // maximum length.
  const uint32_t max_length_;

  // Contains common popup data.
  const autofill::PopupControllerCommon controller_common_;

  // Help text in the footer.
  base::string16 help_text_;

  // The password value to be displayed in the UI.
  base::string16 current_password_;
  // Whether the row with the password is currently selected/highlighted.
  bool password_selected_;

  // The state of the generation popup.
  GenerationUIState state_;

  autofill::PopupViewCommon view_common_;

  std::unique_ptr<KeyPressRegistrator> key_press_handler_manager_;

  base::WeakPtrFactory<PasswordGenerationPopupControllerImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationPopupControllerImpl);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_IMPL_H_
