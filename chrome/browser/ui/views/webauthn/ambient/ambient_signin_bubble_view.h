// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_BUBBLE_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace password_manager {
class PasskeyCredential;
struct PasswordForm;
}  // namespace password_manager

namespace ui {
class Event;
}  // namespace ui

namespace views {
class View;
class Widget;
}  // namespace views

namespace ambient_signin {

class AmbientSigninController;

class AmbientSigninBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(AmbientSigninBubbleView, views::BubbleDialogDelegateView)
 public:
  enum class Mode {
    // Mode when there is single credential. A button area with "Sign in" and
    // "Cancel" buttons will be shown.
    kSingleCredential,
    // Mode when all credentials are shown. No button area will be displayed.
    kAllCredentials,
    // TODO(crbug.com/358119268): Consider a mode that displays a limited number
    // of credentials and includes a button to display all credentials by
    // extending the bubble.
  };

  explicit AmbientSigninBubbleView(views::View* anchor_view,
                                   AmbientSigninController* controller);
  ~AmbientSigninBubbleView() override;

  AmbientSigninBubbleView(const AmbientSigninBubbleView&) = delete;
  AmbientSigninBubbleView& operator=(const AmbientSigninBubbleView&) = delete;

  // Provide credentials for display and show the bubble. This must be called
  // before any of the methods below are called.
  void ShowCredentials(
      const std::vector<password_manager::PasskeyCredential>& credentials,
      const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
          forms);

  void Show();
  void Hide();
  void Close();

  void NotifyWidgetDestroyed();

 private:
  void OnPasskeySelected(const std::vector<uint8_t>& account_id,
                         const ui::Event& event);
  void OnPasswordSelected(const password_manager::PasswordForm* form,
                          const ui::Event& event);
  void SetModeByCredentialCount(size_t credential_count);
  void SetButtonArea();

  std::unique_ptr<views::View> CreatePasskeyRow(
      const password_manager::PasskeyCredential& passkey);
  std::unique_ptr<views::View> CreatePasswordRow(
      const password_manager::PasswordForm* form);

  Mode mode_ = Mode::kSingleCredential;
  raw_ptr<AmbientSigninController> controller_;
  base::WeakPtr<views::Widget> widget_;

  base::WeakPtrFactory<AmbientSigninBubbleView> weak_ptr_factory_{this};
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_BUBBLE_VIEW_H_
