// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/notifications/request_pin_view_chromeos.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/passphrase_textfield.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/security_token_pin/error_generator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Default width of the text field.
constexpr int kDefaultTextWidthChars = 36;

}  // namespace

RequestPinView::RequestPinView(
    const std::string& extension_name,
    chromeos::security_token_pin::CodeType code_type,
    int attempts_left,
    const PinEnteredCallback& pin_entered_callback,
    ViewDestructionCallback view_destruction_callback)
    : pin_entered_callback_(pin_entered_callback),
      view_destruction_callback_(std::move(view_destruction_callback)) {
  Init();
  SetExtensionName(extension_name);
  const bool accept_input = (attempts_left != 0);
  SetDialogParameters(code_type,
                      chromeos::security_token_pin::ErrorLabel::kNone,
                      attempts_left, accept_input);

  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

RequestPinView::~RequestPinView() {
  std::move(view_destruction_callback_).Run();
}

void RequestPinView::ContentsChanged(views::Textfield* sender,
                                     const std::u16string& new_contents) {
  DialogModelChanged();
}

bool RequestPinView::Accept() {
  if (!textfield_->GetEnabled())
    return true;
  DCHECK(!textfield_->GetText().empty());
  DCHECK(!locked_);

  error_label_->SetVisible(true);
  error_label_->SetText(
      l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_PROCESSING));
  error_label_->SetTooltipText(error_label_->GetText());
  error_label_->SetTextStyle(views::style::STYLE_SECONDARY);
  error_label_->SizeToPreferredSize();
  // The |textfield_| and OK button become disabled, but the user still can
  // close the dialog.
  SetAcceptInput(false);
  pin_entered_callback_.Run(base::UTF16ToUTF8(textfield_->GetText()));
  locked_ = true;
  DialogModelChanged();

  return false;
}

bool RequestPinView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  switch (button) {
    case ui::mojom::DialogButton::kCancel:
      return true;
    case ui::mojom::DialogButton::kOk:
      if (locked_)
        return false;
      // Not locked but the |textfield_| is not enabled. It's just a
      // notification to the user and [OK] button can be used to close the
      // dialog.
      if (!textfield_->GetEnabled())
        return true;
      return textfield_->GetText().size() > 0;
    case ui::mojom::DialogButton::kNone:
      return true;
  }

  NOTREACHED();
}

views::View* RequestPinView::GetInitiallyFocusedView() {
  return textfield_;
}

std::u16string RequestPinView::GetWindowTitle() const {
  return window_title_;
}

void RequestPinView::SetDialogParameters(
    chromeos::security_token_pin::CodeType code_type,
    chromeos::security_token_pin::ErrorLabel error_label,
    int attempts_left,
    bool accept_input) {
  locked_ = false;
  SetErrorMessage(error_label, attempts_left, accept_input);
  SetAcceptInput(accept_input);

  switch (code_type) {
    case chromeos::security_token_pin::CodeType::kPin:
      code_type_ = l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_PIN);
      break;
    case chromeos::security_token_pin::CodeType::kPuk:
      code_type_ = l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_PUK);
      break;
  }

  UpdateHeaderText();
}

void RequestPinView::SetExtensionName(const std::string& extension_name) {
  window_title_ = base::ASCIIToUTF16(extension_name);
  UpdateHeaderText();
}

bool RequestPinView::IsTextStyleOfErrorLabelCorrectForTesting() const {
  return STYLE_RED == error_label_->GetTextStyle();
}

void RequestPinView::UpdateHeaderText() {
  int label_text_id = IDS_REQUEST_PIN_DIALOG_HEADER;
  std::u16string label_text =
      l10n_util::GetStringFUTF16(label_text_id, window_title_, code_type_);
  header_label_->SetText(label_text);
  header_label_->SizeToPreferredSize();
}

void RequestPinView::Init() {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText))
      .SetCollapseMargins(true)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(provider->GetDistanceMetric(
                                      views::DISTANCE_RELATED_CONTROL_VERTICAL),
                                  0));

  // Information label.
  header_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_HEADER)));
  header_label_->SetEnabled(true);
  header_label_->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kStart);

  // Textfield to enter the PIN/PUK.
  textfield_ = AddChildView(std::make_unique<chromeos::PassphraseTextfield>());
  textfield_->set_controller(this);
  textfield_->SetEnabled(true);
  textfield_->GetViewAccessibility().SetName(*header_label_);
  textfield_->SetDefaultWidthInChars(kDefaultTextWidthChars);

  // Error label.
  error_label_ = AddChildView(std::make_unique<views::Label>());
  error_label_->SetVisible(false);
  error_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void RequestPinView::SetAcceptInput(bool accept_input) {
  textfield_->SetEnabled(accept_input);
  if (accept_input)
    textfield_->RequestFocus();
}

void RequestPinView::SetErrorMessage(
    chromeos::security_token_pin::ErrorLabel error_label,
    int attempts_left,
    bool accept_input) {
  if (error_label == chromeos::security_token_pin::ErrorLabel::kNone &&
      attempts_left < 0) {
    error_label_->SetVisible(false);
    textfield_->SetInvalid(false);
    return;
  }

  std::u16string error_message =
      chromeos::security_token_pin::GenerateErrorMessage(
          error_label, attempts_left, accept_input);

  error_label_->SetVisible(true);
  error_label_->SetText(error_message);
  error_label_->SetTooltipText(error_message);
  error_label_->SetTextStyle(STYLE_RED);
  error_label_->SizeToPreferredSize();
  textfield_->SetInvalid(true);
}

BEGIN_METADATA(RequestPinView)
END_METADATA
