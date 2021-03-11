// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/password_reuse_modal_warning_dialog.h"

#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/password_protection/metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

using views::BoxLayout;

namespace {

// Updates the image displayed on the illustration based on the current theme.
void SafeBrowsingUpdateImageView(NonAccessibleImageView* image_view,
                                 bool dark_mode_enabled) {
  image_view->SetImage(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          dark_mode_enabled ? IDR_PASSWORD_CHECK_DARK : IDR_PASSWORD_CHECK));
}

// Creates the illustration which is rendered on top of the dialog.
std::unique_ptr<NonAccessibleImageView> SafeBrowsingCreateIllustration(
    bool dark_mode_enabled) {
  auto image_view = std::make_unique<NonAccessibleImageView>();
  SafeBrowsingUpdateImageView(image_view.get(), dark_mode_enabled);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  return image_view;
}

// Sets up the content containing the title and description for the dialog
// rendered below the illustration.
std::unique_ptr<views::View> SetupContent(const std::u16string& title) {
  auto content = std::make_unique<views::View>();
  content->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  content->SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::CONTROL)));

  auto title_label = std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content->AddChildView(std::move(title_label));

  return content;
}

// Creates the description on the modal warning dialog.
views::Label* CreateMessageBodyLabel(std::u16string text) {
  views::Label* message_body_label = new views::Label(text);
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_body_label->SetHandlesTooltips(false);
  return message_body_label;
}

std::u16string GetOkButtonLabel(
    safe_browsing::ReusedPasswordAccountType password_type) {
  switch (password_type.account_type()) {
    case safe_browsing::ReusedPasswordAccountType::NON_GAIA_ENTERPRISE:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_BUTTON);
    case safe_browsing::ReusedPasswordAccountType::SAVED_PASSWORD:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON);
    default:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON);
  }
}

}  // namespace

namespace safe_browsing {

void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordAccountType password_type,
    OnWarningDone done_callback) {
  PasswordReuseModalWarningDialog* dialog = new PasswordReuseModalWarningDialog(
      web_contents, service, password_type, std::move(done_callback));
  constrained_window::CreateBrowserModalDialogViews(
      dialog, web_contents->GetTopLevelNativeWindow())
      ->Show();
}

PasswordReuseModalWarningDialog::PasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordAccountType password_type,
    OnWarningDone done_callback)
    : content::WebContentsObserver(web_contents),
      done_callback_(std::move(done_callback)),
      service_(service),
      url_(web_contents->GetLastCommittedURL()),
      password_type_(password_type) {
  bool show_check_passwords = false;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  show_check_passwords = password_type_.account_type() ==
                         ReusedPasswordAccountType::SAVED_PASSWORD;
#endif
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowIcon(true);
  if (password_type.account_type() !=
          ReusedPasswordAccountType::SAVED_PASSWORD ||
      show_check_passwords) {
    SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  } else {
    SetButtons(ui::DIALOG_BUTTON_OK);
  }
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButtonLabel(password_type_));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON));

  // The set_*_callback() methods below need a OnceCallback each and we only have one
  // (done_callback_), so create a proxy callback that references done_callback_ and use it for each
  // of the set_*_callback() callbacks. Note that since only one of the three callbacks can ever be
  // invoked, done_callback_ is still run at most once.
  auto make_done_callback = [this](safe_browsing::WarningAction value) {
    return base::BindOnce(
        [](OnWarningDone* callback, safe_browsing::WarningAction value) {
          std::move(*callback).Run(value);
        },
        base::Unretained(&done_callback_), value);
  };
  SetAcceptCallback((password_type_.account_type() !=
                         ReusedPasswordAccountType::SAVED_PASSWORD ||
                     show_check_passwords)
                        ? make_done_callback(WarningAction::CHANGE_PASSWORD)
                        : base::DoNothing());
  SetCancelCallback(make_done_callback(WarningAction::IGNORE_WARNING));
  SetCloseCallback(make_done_callback(WarningAction::CLOSE));

  // |service| maybe NULL in tests.
  if (service_)
    service_->AddObserver(this);

  std::vector<size_t> placeholder_offsets;

  if (password_type.account_type() ==
      ReusedPasswordAccountType::SAVED_PASSWORD) {
    const std::u16string message_body =
        service_->GetWarningDetailText(password_type, &placeholder_offsets);

    CreateSavedPasswordReuseModalWarningDialog(
        message_body, service_->GetPlaceholdersForSavedPasswordWarningText(),
        placeholder_offsets);
  } else {
    views::Label* message_body_label = CreateMessageBodyLabel(
        service_
            ? service_->GetWarningDetailText(password_type,
                                             &placeholder_offsets)
            : l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS));
    CreateGaiaPasswordReuseModalWarningDialog(message_body_label);
  }
  modal_construction_start_time_ = base::TimeTicks::Now();
}

PasswordReuseModalWarningDialog::~PasswordReuseModalWarningDialog() {
  if (service_)
    service_->RemoveObserver(this);
  LogModalWarningDialogLifetime(modal_construction_start_time_);
}

void PasswordReuseModalWarningDialog::
    CreateSavedPasswordReuseModalWarningDialog(
        const std::u16string message_body,
        std::vector<std::u16string> placeholders,
        std::vector<size_t> placeholder_offsets) {
  SetLayoutManager(std::make_unique<BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      0 /* between_child_spacing */));
  std::unique_ptr<NonAccessibleImageView> illustration =
      SafeBrowsingCreateIllustration(GetNativeTheme()->ShouldUseDarkColors());
  std::unique_ptr<views::View> content = SetupContent(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY));

  // Bold the domains in the message body label.
  views::StyledLabel* const styled_message_body_label =
      content->AddChildView(std::make_unique<views::StyledLabel>());
  styled_message_body_label->SetText(message_body);
  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = STYLE_EMPHASIZED;
  for (size_t idx = 0; idx < placeholder_offsets.size(); idx++) {
    styled_message_body_label->AddStyleRange(
        gfx::Range(placeholder_offsets[idx],
                   placeholder_offsets[idx] + placeholders.at(idx).length()),
        bold_style);
  }
  styled_message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(illustration));
  AddChildView(std::move(content));
}

void PasswordReuseModalWarningDialog::CreateGaiaPasswordReuseModalWarningDialog(
    views::Label* message_body_label) {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  set_margins(
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Makes message label align with title label.
  const int horizontal_adjustment =
      provider->GetDistanceMetric(DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE) +
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  if (base::i18n::IsRTL()) {
    message_body_label->SetBorder(
        views::CreateEmptyBorder(0, 0, 0, horizontal_adjustment));
  } else {
    message_body_label->SetBorder(
        views::CreateEmptyBorder(0, horizontal_adjustment, 0, 0));
  }
  AddChildView(message_body_label);
}

gfx::Size PasswordReuseModalWarningDialog::CalculatePreferredSize() const {
  constexpr int kDialogWidth = 400;
  return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
}

std::u16string PasswordReuseModalWarningDialog::GetWindowTitle() const {
  // It's ok to return an empty string for the title as this method
  // is from views::DialogDelegateView class.
  return password_type_.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD
             ? std::u16string()
             : l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
}

bool PasswordReuseModalWarningDialog::ShouldShowCloseButton() const {
  return false;
}

gfx::ImageSkia PasswordReuseModalWarningDialog::GetWindowIcon() {
  return password_type_.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD
             ? gfx::ImageSkia()
             : gfx::CreateVectorIcon(
                   kSecurityIcon,
                   ChromeLayoutProvider::Get()->GetDistanceMetric(
                       DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE),
                   gfx::kChromeIconGrey);
}

void PasswordReuseModalWarningDialog::OnGaiaPasswordChanged() {
  GetWidget()->Close();
}

void PasswordReuseModalWarningDialog::OnMarkingSiteAsLegitimate(
    const GURL& url) {
  if (url_.GetWithEmptyPath() == url.GetWithEmptyPath())
    GetWidget()->Close();
}

void PasswordReuseModalWarningDialog::InvokeActionForTesting(
    WarningAction action) {
  switch (action) {
    case WarningAction::CHANGE_PASSWORD:
      Accept();
      break;
    case WarningAction::IGNORE_WARNING:
      Cancel();
      break;
    case WarningAction::CLOSE:
      Close();
      break;
    default:
      NOTREACHED();
      break;
  }
}

WarningUIType PasswordReuseModalWarningDialog::GetObserverType() {
  return WarningUIType::MODAL_DIALOG;
}

void PasswordReuseModalWarningDialog::WebContentsDestroyed() {
  GetWidget()->Close();
}

BEGIN_METADATA(PasswordReuseModalWarningDialog, views::DialogDelegateView)
END_METADATA

}  // namespace safe_browsing
