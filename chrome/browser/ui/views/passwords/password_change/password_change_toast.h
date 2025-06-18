// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_TOAST_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_TOAST_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class MdTextButton;
class Throbber;
}  // namespace views

// Toast view displaying the progress of password change. Displayed content can
// be updated using UpdateConfiguration() without closing the toast.
class PasswordChangeToast : public views::View {
  METADATA_HEADER(PasswordChangeToast, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPasswordChangeViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPasswordChangeActionButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPasswordChangeCloseButton);

  // Helper structure which allows to customize toast according to the
  // requirements.
  struct ToastOptions {
    ToastOptions(const std::u16string& text,
                 const gfx::VectorIcon& icon,
                 const std::optional<std::u16string>& action_button_text,
                 base::OnceClosure action_button_closure = base::DoNothing(),
                 bool has_close_button = false);

    ToastOptions(const std::u16string& text,
                 const std::optional<std::u16string>& action_button_text,
                 base::OnceClosure action_button_closure = base::DoNothing(),
                 bool has_close_button = false);

    ToastOptions(ToastOptions&& other) noexcept;
    ToastOptions& operator=(ToastOptions&& other) noexcept;

    ~ToastOptions();

    std::u16string text;
    // If not present, throbber will be shown.
    std::optional<raw_ref<const gfx::VectorIcon>> icon;
    std::optional<std::u16string> action_button_text;
    base::OnceClosure action_button_closure;
    bool has_close_button = false;
  };

  explicit PasswordChangeToast(ToastOptions toast_configuration);
  ~PasswordChangeToast() override;

  // Recreates displayed content according to the newly provided
  // `configuration`.
  void UpdateLayout(ToastOptions configuration);

  gfx::Insets CalculateMargins();

  views::Throbber* throbber() { return throbber_; }
  views::ImageView* icon_view() { return icon_view_; }
  views::Label* label() { return label_; }
  views::MdTextButton* action_button() { return action_button_; }
  views::ImageButton* close_button() { return close_button_; }

 private:
  void UpdateConfiguration(ToastOptions configuration);

  // views::View
  void OnThemeChanged() override;

  void OnActionButtonClicked();
  void OnCloseButtonClicked();

  std::optional<raw_ref<const gfx::VectorIcon>> icon_;
  base::OnceClosure action_button_closure_;

  raw_ptr<views::Throbber> throbber_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::MdTextButton> action_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_TOAST_H_
