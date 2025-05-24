// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_upgrade_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

class PasskeyUpgradeBubbleController : public PasswordBubbleControllerBase {
 public:
  PasskeyUpgradeBubbleController(
      content::WebContents* web_contents,
      password_manager::metrics_util::UIDisplayDisposition display_disposition,
      std::string passkey_rp_id)
      : PasswordBubbleControllerBase(
            PasswordsModelDelegateFromWebContents(web_contents),
            display_disposition),
        passkey_rp_id_(std::move(passkey_rp_id)) {}

  ~PasskeyUpgradeBubbleController() override { OnBubbleClosing(); }

  std::u16string GetPrimaryAccountEmail() {
    Profile* profile = GetProfile();
    if (!profile) {
      return std::u16string();
    }
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    if (!identity_manager) {
      return std::u16string();
    }
    return base::UTF8ToUTF16(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email);
  }

  void OnManagePasswordClicked() {
    if (delegate_) {
      delegate_->NavigateToPasswordDetailsPageInPasswordManager(
          passkey_rp_id_,
          password_manager::ManagePasswordsReferrer::kPasskeyUpgradeBubble);
    }
  }

  void OnLearnMoreClicked() const {
    content::WebContents* web_contents = GetWebContents();
    if (!web_contents) {
      return;
    }
    constexpr char kHelpCenterUrlBase[] =
        "https://support.google.com/chrome/?p=passkeys";
    GURL learn_more_url(kHelpCenterUrlBase);
    google_util::AppendGoogleLocaleParam(learn_more_url,
                                         base::i18n::GetConfiguredLocale());
    content::OpenURLParams params(learn_more_url, content::Referrer(),
                                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  ui::PAGE_TRANSITION_LINK,
                                  /*is_renderer_initiated=*/false);
    web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
  }

  // PasswordBubbleControllerBase:
  std::u16string GetTitle() const override {
    return l10n_util::GetStringUTF16(IDS_PASSKEY_UPGRADE_BUBBLE_TITLE);
  }

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override {
    // TODO: crbug.com/377758786 - Log metrics
  }

  std::string passkey_rp_id_;
};

PasskeyUpgradeBubbleView::PasskeyUpgradeBubbleView(
    content::WebContents* web_contents,
    views::View* anchor,
    DisplayReason display_reason,
    std::string passkey_rp_id)
    : PasswordBubbleViewBase(web_contents,
                             anchor,
                             /*easily_dismissable=*/true),
      controller_(std::make_unique<PasskeyUpgradeBubbleController>(
          web_contents,
          display_reason == DisplayReason::AUTOMATIC
              ? password_manager::metrics_util::AUTOMATIC_PASSKEY_UPGRADE_BUBBLE
              : password_manager::metrics_util::MANUAL_PASSKEY_UPGRADE_BUBBLE,
          passkey_rp_id)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  set_margins(gfx::Insets());  // `container_` has its own margins.

  SetTitle(controller_->GetTitle());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowIcon(true);

  container_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets().set_bottom(
                           ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_CONTENT_LIST_VERTICAL_SINGLE)))
          .Build());

  std::u16string learn_more_link_text =
      l10n_util::GetStringUTF16(IDS_PASSKEY_UPGRADE_BUBBLE_LEARN_MORE);
  std::vector<size_t> offsets;
  std::u16string full_text = l10n_util::GetStringFUTF16(
      IDS_PASSKEY_UPGRADE_BUBBLE_DESCRIPTION,
      {base::UTF8ToUTF16(passkey_rp_id), controller_->GetPrimaryAccountEmail(),
       learn_more_link_text},
      &offsets);
  CHECK_EQ(offsets.size(), 3u);

  gfx::Range learn_more_link_range(offsets[2],
                                   offsets[2] + learn_more_link_text.size());
  auto learn_more_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](PasskeyUpgradeBubbleView* view) {
            view->controller_->OnLearnMoreClicked();
          },
          base::Unretained(this)));

  auto* label = container_->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetProperty(views::kMarginsKey,
                       ChromeLayoutProvider::Get()->GetInsetsMetric(
                           views::INSETS_DIALOG))
          .SetText(std::move(full_text))
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetDefaultTextStyle(views::style::STYLE_PRIMARY)
          .AddStyleRange(learn_more_link_range, std::move(learn_more_info))
          .Build());

  container_->AddChildView(std::make_unique<views::Separator>())
      ->SetBorder(views::CreateEmptyBorder(
          gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_CONTENT_LIST_VERTICAL_SINGLE),
                          0)));

  manage_passkeys_button_ =
      container_->AddChildView(std::make_unique<RichHoverButton>(
          base::BindRepeating(
              [](PasskeyUpgradeBubbleView* view) {
                view->controller_->OnManagePasswordClicked();
              },
              base::Unretained(this)),
          /*main_image_icon=*/
          ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                         ui::kColorIcon),
          /*title_text=*/
          l10n_util::GetStringUTF16(IDS_PASSKEY_UPGRADE_BUBBLE_MANAGE_BUTTON),
          /*subtitle_text=*/std::u16string(),
          /*action_image_icon=*/
          ui::ImageModel::FromVectorIcon(
              vector_icons::kLaunchIcon, ui::kColorIconSecondary,
              GetLayoutConstant(PAGE_INFO_ICON_SIZE))));

  // The base class sets a fixed dialog width, but that might not fit the
  // manage passkeys hover button. Instead, size the bubble dynamically and
  // size the body content to the width of the button.
  set_fixed_width(0);
  label->SizeToFit(manage_passkeys_button_->GetPreferredSize().width());
}

PasskeyUpgradeBubbleView::~PasskeyUpgradeBubbleView() = default;

RichHoverButton*
PasskeyUpgradeBubbleView::manage_passkeys_button_for_testing() {
  return manage_passkeys_button_;
}

PasswordBubbleControllerBase* PasskeyUpgradeBubbleView::GetController() {
  return controller_.get();
}

const PasswordBubbleControllerBase* PasskeyUpgradeBubbleView::GetController()
    const {
  return controller_.get();
}

ui::ImageModel PasskeyUpgradeBubbleView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

BEGIN_METADATA(PasskeyUpgradeBubbleView)
END_METADATA
