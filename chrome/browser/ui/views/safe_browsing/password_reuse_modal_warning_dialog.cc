// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/password_reuse_modal_warning_dialog.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

using views::BoxLayout;

namespace {

// Fixed height of the illustration shown on the top of the dialog.
constexpr int kSafeBrowsingIllustrationHeight = 148;

// Fixed background color of the illustration shown on the top of the dialog in
// normal mode.
constexpr SkColor kSafeBrowsingPictureBackgroundColor =
    SkColorSetARGB(0x0A, 0, 0, 0);

// Fixed background color of the illustration shown on the top of the dialog in
// dark mode.
constexpr SkColor kSafeBrowsingPictureBackgroundColorDarkMode =
    SkColorSetARGB(0x1A, 0x00, 0x00, 0x00);

// Updates the image displayed on the illustration based on the current theme.
void SafeBrowsingUpdateImageView(NonAccessibleImageView* image_view,
                                 bool dark_mode_enabled) {
  image_view->SetImage(gfx::CreateVectorIcon(
      dark_mode_enabled ? kPasswordCheckWarningDarkIcon
                        : kPasswordCheckWarningIcon,
      dark_mode_enabled ? kSafeBrowsingPictureBackgroundColorDarkMode
                        : kSafeBrowsingPictureBackgroundColor));
}

// Creates the illustration which is rendered on top of the dialog.
std::unique_ptr<NonAccessibleImageView> SafeBrowsingCreateIllustration(
    bool dark_mode_enabled) {
  const gfx::Size illustration_size(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
      kSafeBrowsingIllustrationHeight);
  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetPreferredSize(illustration_size);
  SafeBrowsingUpdateImageView(image_view.get(), dark_mode_enabled);
  image_view->SetSize(illustration_size);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  return image_view;
}

// Sets up the content containing the title and description for the dialog
// rendered below the illustration.
std::unique_ptr<views::View> SetupContent(const base::string16& title) {
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
views::Label* CreateMessageBodyLabel(base::string16 text) {
  views::Label* message_body_label = new views::Label(text);
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_body_label->SetHandlesTooltips(false);
  return message_body_label;
}

base::string16 GetOkButtonLabel(
    safe_browsing::ReusedPasswordAccountType password_type) {
  switch (password_type.account_type()) {
    case safe_browsing::ReusedPasswordAccountType::NON_GAIA_ENTERPRISE:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_BUTTON);
    case safe_browsing::ReusedPasswordAccountType::SAVED_PASSWORD:
      return l10n_util::GetStringUTF16(IDS_CLOSE);
    default:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON);
  }
}

}  // namespace

namespace safe_browsing {

constexpr int kIconSize = 20;

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
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   GetOkButtonLabel(password_type_));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON));

  // |service| maybe NULL in tests.
  if (service_)
    service_->AddObserver(this);

  views::Label* message_body_label = CreateMessageBodyLabel(
      service_
          ? service_->GetWarningDetailText(password_type)
          : l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS));

  if (password_type.account_type() ==
      ReusedPasswordAccountType::SAVED_PASSWORD) {
    CreateSavedPasswordReuseModalWarningDialog(message_body_label);
  } else {
    CreateGaiaPasswordReuseModalWarningDialog(message_body_label);
  }
}

PasswordReuseModalWarningDialog::~PasswordReuseModalWarningDialog() {
  if (service_)
    service_->RemoveObserver(this);
}

void PasswordReuseModalWarningDialog::
    CreateSavedPasswordReuseModalWarningDialog(
        views::Label* message_body_label) {
  SetLayoutManager(std::make_unique<BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      0 /* between_child_spacing */));
  std::unique_ptr<NonAccessibleImageView> illustration =
      SafeBrowsingCreateIllustration(GetNativeTheme()->ShouldUseDarkColors());
  std::unique_ptr<views::View> content = SetupContent(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY));
  content->AddChildView(std::move(message_body_label));
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
  int horizontal_adjustment =
      kIconSize +
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

ui::ModalType PasswordReuseModalWarningDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 PasswordReuseModalWarningDialog::GetWindowTitle() const {
  // It's ok to return an empty string for the title as this method
  // is from views::DialogDelegateView class.
  return password_type_.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD
             ? base::string16()
             : l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
}

bool PasswordReuseModalWarningDialog::ShouldShowCloseButton() const {
  return false;
}

gfx::ImageSkia PasswordReuseModalWarningDialog::GetWindowIcon() {
  return password_type_.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD
             ? gfx::ImageSkia()
             : gfx::CreateVectorIcon(kSecurityIcon, kIconSize,
                                     gfx::kChromeIconGrey);
}

bool PasswordReuseModalWarningDialog::ShouldShowWindowIcon() const {
  return true;
}

int PasswordReuseModalWarningDialog::GetDialogButtons() const {
  return password_type_.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD
             ? ui::DIALOG_BUTTON_OK
             : ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

bool PasswordReuseModalWarningDialog::Cancel() {
  std::move(done_callback_).Run(WarningAction::IGNORE_WARNING);
  return true;
}

bool PasswordReuseModalWarningDialog::Accept() {
  if (password_type_.account_type() !=
      ReusedPasswordAccountType::SAVED_PASSWORD)
    std::move(done_callback_).Run(WarningAction::CHANGE_PASSWORD);

  return true;
}

bool PasswordReuseModalWarningDialog::Close() {
  if (done_callback_)
    std::move(done_callback_).Run(WarningAction::CLOSE);
  return true;
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

}  // namespace safe_browsing
