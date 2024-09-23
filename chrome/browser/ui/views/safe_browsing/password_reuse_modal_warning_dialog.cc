// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/password_reuse_modal_warning_dialog.h"

#include "base/functional/callback_helpers.h"
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
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

using views::BoxLayout;

namespace {

class SafeBrowsingImageView : public NonAccessibleImageView {
  METADATA_HEADER(SafeBrowsingImageView, NonAccessibleImageView)

 public:
  SafeBrowsingImageView() {
    SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  }
  ~SafeBrowsingImageView() override = default;

  // NonAccessibleImageView:
  void OnThemeChanged() override {
    NonAccessibleImageView::OnThemeChanged();
    SetImage(ui::ImageModel::FromResourceId(
        GetNativeTheme()->ShouldUseDarkColors() ? IDR_PASSWORD_CHECK_DARK
                                                : IDR_PASSWORD_CHECK));
  }
};

BEGIN_METADATA(SafeBrowsingImageView)
END_METADATA

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
          views::DialogContentType::kControl,
          views::DialogContentType::kControl)));

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
  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowIcon(true);
  if (password_type.account_type() !=
          ReusedPasswordAccountType::SAVED_PASSWORD ||
      show_check_passwords) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
               static_cast<int>(ui::mojom::DialogButton::kCancel));
  } else {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  }
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 GetOkButtonLabel(password_type_));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
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

  if (password_type.account_type() ==
      ReusedPasswordAccountType::SAVED_PASSWORD) {
    const std::u16string message_body =
        service_->GetWarningDetailText(password_type);

    CreateSavedPasswordReuseModalWarningDialog(message_body);
  } else {
    views::Label* message_body_label = CreateMessageBodyLabel(
        service_
            ? service_->GetWarningDetailText(password_type)
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
        const std::u16string message_body) {
  SetLayoutManager(std::make_unique<BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      0 /* between_child_spacing */));
  std::unique_ptr<views::View> content = SetupContent(l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_SAVED_PASSWORD_SUMMARY));

  views::StyledLabel* const styled_message_body_label =
      content->AddChildView(std::make_unique<views::StyledLabel>());
  styled_message_body_label->SetText(message_body);
  styled_message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::make_unique<SafeBrowsingImageView>());
  AddChildView(std::move(content));
}

void PasswordReuseModalWarningDialog::CreateGaiaPasswordReuseModalWarningDialog(
    views::Label* message_body_label) {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Makes message label align with title label.
  const int horizontal_adjustment =
      provider->GetDistanceMetric(DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE) +
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  if (base::i18n::IsRTL()) {
    message_body_label->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, 0, 0, horizontal_adjustment)));
  } else {
    message_body_label->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, horizontal_adjustment, 0, 0)));
  }
  AddChildView(message_body_label);
}

gfx::Size PasswordReuseModalWarningDialog::GetMinimumSize() const {
  // The default GetMinimumSize of `View` will call the layout manager to
  // calculate under unconstrained conditions when there is a layout manager.
  // This will cause the minimum value to be calculated incorrectly.
  return GetPreferredSize(views::SizeBounds(0, 0));
}

gfx::Size PasswordReuseModalWarningDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  constexpr int kDialogWidth = 400;
  return gfx::Size(kDialogWidth, GetLayoutManager()->GetPreferredHeightForWidth(
                                     this, kDialogWidth));
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

ui::ImageModel PasswordReuseModalWarningDialog::GetWindowIcon() {
  return password_type_.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD
             ? ui::ImageModel()
             : ui::ImageModel::FromVectorIcon(
                   kSecurityIcon, ui::kColorIcon,
                   ChromeLayoutProvider::Get()->GetDistanceMetric(
                       DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE));
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
  }
}

WarningUIType PasswordReuseModalWarningDialog::GetObserverType() {
  return WarningUIType::MODAL_DIALOG;
}

void PasswordReuseModalWarningDialog::WebContentsDestroyed() {
  GetWidget()->Close();
}

BEGIN_METADATA(PasswordReuseModalWarningDialog)
END_METADATA

}  // namespace safe_browsing
