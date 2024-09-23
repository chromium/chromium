// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_PASSWORD_AUTH_VIEW_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_PASSWORD_AUTH_VIEW_H_

#include <string>

#include "ash/auth/views/auth_textfield.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/ime_controller.h"
#include "ash/style/system_textfield_controller.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/auth_panel/impl/auth_factor_store.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"
#include "chromeos/ash/components/auth_panel/impl/factor_auth_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"

namespace views {

class BoxLayout;
class ImageView;
class ToggleImageButton;

}  // namespace views

namespace ash {

class ArrowButtonView;
class AuthPanelEventDispatcher;

// This class encapsulates a textfield, toggle display password eye icon, and
// a submit arrow button. It handles focus behavior, layout behavior, and state
// changes such as capslock state change, display password state change when the
// eye icon is pressed. Those state changes are listened to from
// `AuthFactorStore`.
//
// When the password is submitted, an event is dispatched with the password as a
// payload. The password submission logic does not live in this class. Here, we
// only handle UI behavior.
class PasswordAuthView : public FactorAuthView,
                         public ImeController::Observer,
                         public AuthTextfield::Observer {
  METADATA_HEADER(PasswordAuthView, FactorAuthView)

 public:
  class TestApi {
   public:
    explicit TestApi(PasswordAuthView* password_auth_view)
        : password_auth_view_(password_auth_view) {}

    views::Textfield* GetPasswordTextfield();

    views::View* GetSubmitPasswordButton();

   private:
    raw_ptr<PasswordAuthView> password_auth_view_;
  };

  PasswordAuthView(AuthPanelEventDispatcher* dispatcher,
                   AuthFactorStore* store);
  ~PasswordAuthView() override;

  // FactorAuthView:
  AshAuthFactor GetFactor() override;
  void OnStateChanged(const AuthFactorStore::State& state) override;

  // views::View:
  void RequestFocus() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override {}

  // AuthTextfield::Observer:
  void OnSubmit() override;
  void OnEscape() override;

 private:
  class LoginPasswordRow;
  class DisplayPasswordButton;

  void ConfigureRootLayout();
  void CreateAndConfigurePasswordRow();
  void CreateAndConfigureCapslockIcon();
  void CreateAndConfigureTextfieldContainer();
  void CreateAndConfigureDisplayPasswordButton();
  void CreateAndConfigureSubmitButton();

  void OnSubmitButtonPressed();
  void OnDisplayPasswordButtonPressed();
  void SetCapsLockIconHighlighted(bool highlight);

  void UpdateTextfield(
      const AuthFactorStore::State::AuthTextfieldState& auth_textfield_state);

  // AuthTextfield::Delegate:
  void OnTextfieldBlur() override;
  void OnTextfieldFocus() override;
  void OnContentsChanged(const std::u16string& new_contents) override;

  raw_ptr<AuthPanelEventDispatcher, DanglingUntriaged> dispatcher_ = nullptr;

  raw_ptr<LoginPasswordRow> password_row_ = nullptr;

  raw_ptr<views::BoxLayout> password_row_layout_ = nullptr;

  raw_ptr<views::ImageView> capslock_icon_ = nullptr;

  raw_ptr<views::ToggleImageButton> display_password_button_ = nullptr;

  raw_ptr<ArrowButtonView> submit_button_ = nullptr;

  raw_ptr<AuthTextfield> auth_textfield_ = nullptr;

  base::CallbackListSubscription auth_factor_store_subscription_;

  base::ScopedObservation<ImeController, ImeController::Observer>
      input_methods_observer_{this};

  ui::ImageModel capslock_icon_highlighted_;

  ui::ImageModel capslock_icon_blurred_;

  base::WeakPtrFactory<PasswordAuthView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_PASSWORD_AUTH_VIEW_H_
