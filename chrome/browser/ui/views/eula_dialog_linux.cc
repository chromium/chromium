// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eula_dialog_linux.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kBorderInsets = 10;

// This is sized so that the entire EULA text is visible without scrolling
// with typical fonts and sizes.
constexpr gfx::Size kPreferredContentSize(600, 450);

}  // namespace

EulaDialog::EulaDialog(base::OnceCallback<void(bool)> callback)
    : callback_(std::move(callback)) {
  SetTitle(l10n_util::GetStringUTF16(IDS_TERMS_DIALOG_TITLE));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_TERMS_DIALOG_ACCEPT));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_TERMS_DIALOG_CANCEL));

  auto content_view = std::make_unique<views::View>();
  content_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view->SetBorder(views::CreateEmptyBorder(gfx::Insets(kBorderInsets)));

  auto* textarea =
      content_view->AddChildView(std::make_unique<views::Textarea>());
  textarea->SetReadOnly(true);
  textarea->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TERMS_DIALOG_ACCESSIBILITY_LABEL));

  std::string terms_utf8 =
      ui::ResourceBundle::GetSharedInstance().LoadLocalizedResourceString(
          IDS_TERMS_TXT);
  textarea->SetText(base::UTF8ToUTF16(terms_utf8));
  // Scroll to the top in case the text doesn't fit with the user's default
  // font or size.
  textarea->Scroll({0});

  content_view->SetPreferredSize(kPreferredContentSize);
  SetContentsView(std::move(content_view));
}

EulaDialog::~EulaDialog() = default;

// static
views::Widget* EulaDialog::Show(base::OnceCallback<void(bool)> callback) {
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      new EulaDialog(std::move(callback)), nullptr, nullptr);
  widget->Show();
  return widget;
}

bool EulaDialog::Accept() {
  if (callback_) {
    std::move(callback_).Run(true);
  }
  return true;
}

bool EulaDialog::Cancel() {
  if (callback_) {
    std::move(callback_).Run(false);
  }
  return true;
}

void EulaDialog::WindowClosing() {
  if (callback_) {
    std::move(callback_).Run(false);
  }
}
