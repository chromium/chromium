// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_VIEWS_PASSWORD_AUTH_VIEW_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_VIEWS_PASSWORD_AUTH_VIEW_H_

#include <string>

#include "ash/ime/ime_controller_impl.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/auth_panel/auth_factor_store.h"
#include "chromeos/ash/components/auth_panel/factor_auth_view.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace views {

class BoxLayout;
class ImageView;
class Textfield;
class ToggleImageButton;

}  // namespace views

namespace ash {

class ArrowButtonView;
class LoginTextfield;
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
                         public views::TextfieldController,
                         public ImeControllerImpl::Observer {
 public:
  PasswordAuthView(AuthPanelEventDispatcher* dispatcher,
                   AuthFactorStore* store);
  ~PasswordAuthView() override;

  // FactorAuthView:
  AshAuthFactor GetFactor() override;
  void OnStateChanged(const AuthFactorStore::State& state) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void RequestFocus() override;

  // ImeControllerImpl::Observer
  void OnCapsLockChanged(bool enabled) override;

 private:
  class LoginPasswordRow;
  class DisplayPasswordButton;

  void ConfigureRootLayout();
  void CreateAndConfigurePasswordRow();
  void CreateAndConfigureCapslockIcon();
  void CreateAndConfigureLoginTextfield();
  void CreateAndConfigureDisplayPasswordButton();
  void CreateAndConfigureSubmitButton();

  void OnSubmitButtonPressed();
  void OnDisplayPasswordButtonPressed();
  void SetCapsLockIconHighlighted(bool highlight);

  raw_ptr<AuthPanelEventDispatcher> dispatcher_ = nullptr;

  raw_ptr<LoginPasswordRow> password_row_ = nullptr;

  raw_ptr<views::BoxLayout> password_row_layout_ = nullptr;

  raw_ptr<views::ImageView> capslock_icon_ = nullptr;

  raw_ptr<LoginTextfield> textfield_ = nullptr;

  raw_ptr<views::ToggleImageButton> display_password_button_ = nullptr;

  raw_ptr<ArrowButtonView> submit_button_ = nullptr;

  base::CallbackListSubscription auth_factor_store_subscription_;

  base::ScopedObservation<ImeControllerImpl, ImeControllerImpl::Observer>
      input_methods_observer_{this};

  ui::ImageModel capslock_icon_highlighted_;

  ui::ImageModel capslock_icon_blurred_;

  base::WeakPtrFactory<PasswordAuthView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_PASSWORD_AUTH_VIEW_H_
