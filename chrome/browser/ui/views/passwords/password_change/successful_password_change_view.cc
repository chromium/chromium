// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/successful_password_change_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/successful_password_change_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/vector_icons.h"

namespace {

// Returns margins for a dialog.
gfx::Insets ComputeRowMargins() {
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  gfx::Insets margins = layout_provider->GetInsetsMetric(views::INSETS_DIALOG);
  margins.set_top_bottom(0, 0);
  return margins;
}

std::unique_ptr<views::View> CreateUsernameLabel(
    const std::u16string& username) {
  std::unique_ptr<views::Label> username_label(new views::Label(
      username, views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  username_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  username_label->SetID(SuccessfulPasswordChangeView::kUsernameLabelId);
  return username_label;
}

std::unique_ptr<views::Label> CreatePasswordLabel(
    const std::u16string& password) {
  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      password, views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetTextStyle(views::style::STYLE_SECONDARY_MONOSPACED);
  label->SetObscured(true);
  label->SetElideBehavior(gfx::TRUNCATE);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetID(SuccessfulPasswordChangeView::kPasswordLabelId);
  return label;
}

std::unique_ptr<views::View> CreateVerticalStackView() {
  auto view = std::make_unique<views::BoxLayoutView>();
  view->SetOrientation(views::LayoutOrientation::kVertical);
  view->SetCollapseMarginsSpacing(true);
  return view;
}

// Row containing favicon, username, password and eye icon:
// *--------------------------------------------------------*
// |         | Username                          |          |
// | Favicon |-----------------------------------| Eye icon |
// |         | Password                          |          |
// *--------------------------------------------------------*
std::unique_ptr<views::View> CreateUsernamePasswordWithEyeIcon(
    SuccessfulPasswordChangeBubbleController* controller) {
  CHECK(controller);

  auto parent_view = std::make_unique<views::BoxLayoutView>();
  parent_view->SetInsideBorderInsets(ComputeRowMargins());

  // Set the same spacing as for RichHoverButton which is displayed bellow.
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL);
  parent_view->SetBetweenChildSpacing(icon_label_spacing);

  // Add favicon.
  views::ImageView* favicon_view =
      parent_view->AddChildView(std::make_unique<views::ImageView>());
  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);
  favicon_view->SetImageSize({icon_size, icon_size});
  favicon_view->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kGlobeIcon, ui::kColorIcon, gfx::kFaviconSize));
  controller->RequestFavicon(base::BindOnce(
      [](views::ImageView* favicon_view, const gfx::Image& favicon) {
        if (!favicon.IsEmpty()) {
          favicon_view->SetImage(ui::ImageModel::FromImage(favicon));
        }
      },
      favicon_view));

  // Add username/password labels.
  auto* username_password_view =
      parent_view->AddChildView(CreateVerticalStackView());
  username_password_view->SetProperty(views::kBoxLayoutFlexKey,
                                      views::BoxLayoutFlexSpecification());
  username_password_view->AddChildView(
      CreateUsernameLabel(controller->GetUsername()));
  views::Label* password_label = username_password_view->AddChildView(
      CreatePasswordLabel(controller->GetNewPassword()));

  // Add eye icon which allows to reveal a password.
  auto* eye_icon = parent_view->AddChildView(
      CreateVectorToggleImageButton(views::Button::PressedCallback()));
  eye_icon->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_SHOW_PASSWORD));
  eye_icon->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_HIDE_PASSWORD));
  eye_icon->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  views::SetImageFromVectorIconWithColorId(eye_icon, views::kEyeIcon,
                                           ui::kColorIcon, ui::kColorIcon);
  views::SetToggledImageFromVectorIconWithColorId(
      eye_icon, views::kEyeCrossedIcon, ui::kColorIcon, ui::kColorIcon);

  base::RepeatingCallback<void(bool)> auth_result_callback =
      base::BindRepeating(
          [](views::ToggleImageButton* toggle_button,
             views::Label* password_label, bool auth_result) {
            if (!auth_result) {
              return;
            }
            password_label->SetObscured(!password_label->GetObscured());
            toggle_button->SetToggled(!toggle_button->GetToggled());
          },
          eye_icon, password_label);

  eye_icon->SetCallback(base::BindRepeating(
      [](base::WeakPtr<SuccessfulPasswordChangeBubbleController> controller,
         views::Label* password_label,
         base::RepeatingCallback<void(bool)> auth_callback) {
        if (!password_label->GetObscured()) {
          // Run callback to hide the password. No auth needed to do it.
          auth_callback.Run(true);
          return;
        }
        if (controller) {
          controller->AuthenticateUser(auth_callback);
        }
      },
      controller->GetWeakPtr(), password_label,
      std::move(auth_result_callback)));
  eye_icon->SetID(SuccessfulPasswordChangeView::kEyeIconButtonId);

  return parent_view;
}

std::unique_ptr<views::View> CreateManagePasswordsView(
    base::RepeatingClosure open_password_manager_closure) {
  auto manage_passwords_button = std::make_unique<RichHoverButton>(
      std::move(open_password_manager_closure),
      /*main_image_icon=*/
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                     ui::kColorIcon),
      /*title_text=*/
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON),
      /*subtitle_text=*/std::u16string(),
      /*action_image_icon=*/
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     ui::kColorIconSecondary,
                                     GetLayoutConstant(PAGE_INFO_ICON_SIZE)));
  manage_passwords_button->SetID(
      SuccessfulPasswordChangeView::kManagePasswordsButtonId);
  manage_passwords_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON));
  return manage_passwords_button;
}

}  // namespace

SuccessfulPasswordChangeView::SuccessfulPasswordChangeView(
    content::WebContents* web_contents,
    views::BubbleAnchor anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(std::make_unique<SuccessfulPasswordChangeBubbleController>(
          PasswordsModelDelegateFromWebContents(web_contents))) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::View* root_view = AddChildView(std::make_unique<views::View>());
  int spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);

  views::BoxLayout* box_layout =
      root_view->SetLayoutManager(std::make_unique<views::BoxLayout>());
  box_layout->SetOrientation(views::LayoutOrientation::kVertical);
  box_layout->set_cross_axis_alignment(views::LayoutAlignment::kStretch);
  box_layout->SetCollapseMarginsSpacing(true);
  box_layout->set_between_child_spacing(spacing);
  box_layout->set_inside_border_insets(
      gfx::Insets::TLBR(spacing, 0, spacing, 0));
  // Set the margins to 0 such that the `root_view` fills the whole page bubble
  // width.
  set_margins(gfx::Insets());

  views::View* username_password_row = root_view->AddChildView(
      CreateUsernamePasswordWithEyeIcon(controller_.get()));
  root_view->AddChildView(std::make_unique<views::Separator>());
  root_view->AddChildView(CreateManagePasswordsView(base::BindRepeating(
      &SuccessfulPasswordChangeBubbleController::OpenPasswordManager,
      controller_->GetWeakPtr())));

  SetShowIcon(true);
  SetInitiallyFocusedView(username_password_row->GetViewByID(kEyeIconButtonId));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCloseCallback(base::BindRepeating(
      [](SuccessfulPasswordChangeView* view) {
        // When dialog is closed explicitly finish password change flow to
        // transition into a default password manager state.
        if (view->GetWidget()->closed_reason() ==
            views::Widget::ClosedReason::kCloseButtonClicked) {
          view->controller_->FinishPasswordChange();
        }
      },
      this));
}

SuccessfulPasswordChangeView::~SuccessfulPasswordChangeView() = default;

PasswordBubbleControllerBase* SuccessfulPasswordChangeView::GetController() {
  return controller_.get();
}

const PasswordBubbleControllerBase*
SuccessfulPasswordChangeView::GetController() const {
  return controller_.get();
}

ui::ImageModel SuccessfulPasswordChangeView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void SuccessfulPasswordChangeView::AddedToWidget() {
  SetBubbleHeaderLottie(IDR_PASSWORD_CHANGE_SUCCESS_LOTTIE);
}

BEGIN_METADATA(SuccessfulPasswordChangeView)
END_METADATA
