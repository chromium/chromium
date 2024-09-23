// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/impl/views/password_auth_view.h"

#include <memory>
#include <optional>

#include "ash/auth/views/auth_textfield.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/ime_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_textfield_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/auth_panel/impl/auth_factor_store.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/impl/views/view_size_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace ash {

views::Textfield* PasswordAuthView::TestApi::GetPasswordTextfield() {
  auto* password_auth_view =
      static_cast<PasswordAuthView*>(password_auth_view_);
  return static_cast<AuthTextfield*>(password_auth_view->auth_textfield_);
}

views::View* PasswordAuthView::TestApi::GetSubmitPasswordButton() {
  auto* password_auth_view =
      static_cast<PasswordAuthView*>(password_auth_view_);
  return password_auth_view->submit_button_;
}

// The login password row contains the password textfield and different buttons
// and indicators (easy unlock, display password, caps lock enabled).
class PasswordAuthView::LoginPasswordRow : public views::View {
  METADATA_HEADER(LoginPasswordRow, views::View)

 public:
  LoginPasswordRow() {
      SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysSystemBaseElevated,
          kLoginPasswordRowRoundedRectRadius));
      SetBorder(std::make_unique<views::HighlightBorder>(
          kLoginPasswordRowRoundedRectRadius,
          views::HighlightBorder::Type::kHighlightBorderNoShadow));
  }

  ~LoginPasswordRow() override = default;
  LoginPasswordRow(const LoginPasswordRow&) = delete;
  LoginPasswordRow& operator=(const LoginPasswordRow&) = delete;

};

BEGIN_METADATA(PasswordAuthView, LoginPasswordRow)
END_METADATA

void PasswordAuthView::ConfigureRootLayout() {
  // Contains the password layout on the left and the submit button on the
  // right.
  auto* root_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(0, kLeftPaddingPasswordView, 0, 0),
      kSpacingBetweenPasswordRowAndSubmitButtonDp));
  root_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
}

void PasswordAuthView::CreateAndConfigurePasswordRow() {
  auto* password_row_container =
      AddChildView(std::make_unique<NonAccessibleView>());
  // The password row should have the same visible height than the submit
  // button. Since the login password view has the same height than the submit
  // button – border included – we need to remove its border.
  auto* password_row_container_layout =
      password_row_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical,
              gfx::Insets::VH(kBorderForFocusRingDp, 0)));
  password_row_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  password_row_ = password_row_container->AddChildView(
      std::make_unique<LoginPasswordRow>());
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kInternalHorizontalPaddingPasswordRowDp),
      kHorizontalSpacingBetweenIconsAndTextfieldDp);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  password_row_layout_ = password_row_->SetLayoutManager(std::move(layout));

  views::FocusRing::Install(password_row_);

  // Make the password row fill the view.
  password_row_container_layout->SetFlexForView(password_row_, 1);
}

void PasswordAuthView::CreateAndConfigureCapslockIcon() {
  capslock_icon_ =
      password_row_->AddChildView(std::make_unique<views::ImageView>());
  capslock_icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_CAPS_LOCK_ACCESSIBLE_NAME));
  capslock_icon_->SetVisible(false);

  capslock_icon_highlighted_ = ui::ImageModel::FromVectorIcon(
      kLockScreenCapsLockIcon, cros_tokens::kCrosSysOnSurface);
  capslock_icon_blurred_ = ui::ImageModel::FromVectorIcon(
      kLockScreenCapsLockIcon, cros_tokens::kCrosSysDisabled);
}

void PasswordAuthView::CreateAndConfigureTextfieldContainer() {
  auto* textfield_container =
      password_row_->AddChildView(std::make_unique<NonAccessibleView>());
  textfield_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kPasswordTextfieldMarginDp)));

  // Password textfield. We control the textfield size by sizing the parent
  // view, as the textfield will expand to fill it.
  auth_textfield_ = textfield_container->AddChildView(
      std::make_unique<AuthTextfield>(AuthTextfield::AuthType::kPassword));

  auth_textfield_->AddObserver(this);

  auth_textfield_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PASSWORD_PLACEHOLDER));
  auth_textfield_->SetFocusBehavior(FocusBehavior::ALWAYS);

  password_row_layout_->SetFlexForView(textfield_container, 1);
}

void PasswordAuthView::CreateAndConfigureSubmitButton() {
  submit_button_ = AddChildView(std::make_unique<ArrowButtonView>(
      base::BindRepeating(&PasswordAuthView::OnSubmitButtonPressed,
                          base::Unretained(this)),
      kSubmitButtonContentSizeDp));
  submit_button_->SetBackgroundColorId(cros_tokens::kCrosSysSystemOnBase);
  submit_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetEnabled(false);
}

void PasswordAuthView::CreateAndConfigureDisplayPasswordButton() {
  display_password_button_ =
      password_row_->AddChildView(std::make_unique<views::ToggleImageButton>(
          base::BindRepeating(&PasswordAuthView::OnDisplayPasswordButtonPressed,
                              base::Unretained(this))));

  display_password_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_DISPLAY_PASSWORD_BUTTON_ACCESSIBLE_NAME_SHOW));
  display_password_button_->SetToggledTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_DISPLAY_PASSWORD_BUTTON_ACCESSIBLE_NAME_HIDE));
  display_password_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  display_password_button_->SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(display_password_button_)
      ->SetColorId(ui::kColorAshFocusRing);

  const ui::ImageModel invisible_icon = ui::ImageModel::FromVectorIcon(
      kLockScreenPasswordInvisibleIcon, cros_tokens::kCrosSysOnSurface,
      kIconSizeDp);
  const ui::ImageModel visible_icon = ui::ImageModel::FromVectorIcon(
      kLockScreenPasswordVisibleIcon, cros_tokens::kCrosSysOnSurface,
      kIconSizeDp);
  const ui::ImageModel visible_icon_disabled = ui::ImageModel::FromVectorIcon(
      kLockScreenPasswordVisibleIcon, cros_tokens::kCrosSysDisabled,
      kIconSizeDp);
  display_password_button_->SetImageModel(views::Button::STATE_NORMAL,
                                          visible_icon);
  display_password_button_->SetImageModel(views::Button::STATE_DISABLED,
                                          visible_icon_disabled);
  display_password_button_->SetToggledImageModel(views::Button::STATE_NORMAL,
                                                 invisible_icon);

  display_password_button_->SetEnabled(false);
}

PasswordAuthView::PasswordAuthView(AuthPanelEventDispatcher* dispatcher,
                                   AuthFactorStore* store)
    : dispatcher_(dispatcher) {
  auth_factor_store_subscription_ = store->Subscribe(base::BindRepeating(
      &PasswordAuthView::OnStateChanged, weak_ptr_factory_.GetWeakPtr()));
  input_methods_observer_.Observe(ImeController::Get());

  ConfigureRootLayout();
  CreateAndConfigurePasswordRow();
  CreateAndConfigureCapslockIcon();
  CreateAndConfigureTextfieldContainer();
  CreateAndConfigureDisplayPasswordButton();
  CreateAndConfigureSubmitButton();
}

PasswordAuthView::~PasswordAuthView() {
  auth_textfield_->RemoveObserver(this);
}

AshAuthFactor PasswordAuthView::GetFactor() {
  return AshAuthFactor::kGaiaPassword;
}

void PasswordAuthView::RequestFocus() {
  auth_textfield_->RequestFocus();
}

void PasswordAuthView::OnSubmit() {
  OnSubmitButtonPressed();
}

void PasswordAuthView::OnEscape() {
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::
          kEscapePressedOnPasswordTextfield,
      std::nullopt});
}

void PasswordAuthView::OnCapsLockChanged(bool enabled) {
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::kCapslockKeyPressed,
      std::nullopt});
}

void PasswordAuthView::OnSubmitButtonPressed() {
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::kPasswordSubmit,
      std::nullopt});
}

void PasswordAuthView::OnDisplayPasswordButtonPressed() {
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::kDisplayPasswordButtonPressed,
      std::nullopt});
}

void PasswordAuthView::OnContentsChanged(const std::u16string& new_contents) {
  // TODO(b/288692954): switch to variant-based implementation of event objects.
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::
          kPasswordTextfieldContentsChanged,
      base::UTF16ToUTF8(auth_textfield_->GetText())});
}

gfx::Size PasswordAuthView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  views::SizeBounds content_available_size(available_size);
  content_available_size.set_width(kPasswordTotalWidthDp);
  gfx::Size size = views::View::CalculatePreferredSize(available_size);
  size.set_width(kPasswordTotalWidthDp);
  return size;
}

void PasswordAuthView::OnStateChanged(const AuthFactorStore::State& state) {
  // TODO(b/271248452): logic for timer reset
  CHECK(state.password_view_state_.has_value());
  const auto& password_view_state = state.password_view_state_.value();

  if (state.password_view_state_->is_password_textfield_focused_) {
    RequestFocus();
  }

  UpdateTextfield(password_view_state.auth_textfield_state_);

  const auto& password = password_view_state.auth_textfield_state_.password_;

  bool is_display_password_button_enabled =
      password_view_state.is_factor_enabled_ && !password.empty();
  bool is_display_password_button_toggled =
      password_view_state.auth_textfield_state_.is_password_visible_;

  display_password_button_->SetEnabled(is_display_password_button_enabled);

  display_password_button_->SetToggled(is_display_password_button_toggled);

  bool is_submit_button_enabled =
      password_view_state.is_factor_enabled_ && !password.empty();

  submit_button_->SetEnabled(is_submit_button_enabled);

  capslock_icon_->SetVisible(password_view_state.is_capslock_on_);

  SetCapsLockIconHighlighted(
      password_view_state.is_password_textfield_focused_);
}

void PasswordAuthView::SetCapsLockIconHighlighted(bool highlight) {
  capslock_icon_->SetImage(highlight ? capslock_icon_highlighted_
                                     : capslock_icon_blurred_);
}

void PasswordAuthView::OnTextfieldBlur() {
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::kPasswordTextfieldBlurred,
      std::nullopt});
}

void PasswordAuthView::OnTextfieldFocus() {
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::kPasswordTextfieldFocused,
      std::nullopt});
}

void PasswordAuthView::UpdateTextfield(
    const AuthFactorStore::State::AuthTextfieldState& auth_textfield_state) {
  auth_textfield_->SetReadOnly(auth_textfield_state.is_read_only);
  auth_textfield_->SetTextVisible(auth_textfield_state.is_password_visible_);

  if (auto new_text = base::UTF8ToUTF16(auth_textfield_state.password_);
      new_text != auth_textfield_->GetText()) {
    auth_textfield_->SetText(new_text);
  }
}

BEGIN_METADATA(PasswordAuthView)
END_METADATA

}  // namespace ash
