// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/ambient_login_permission_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

constexpr int kIconSize = 20;

}  // namespace

AmbientLoginPermissionBubbleView::AmbientLoginPermissionBubbleView(
    content::WebContents* web_contents,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    PermissionPromptStyle prompt_style)
    : PermissionPromptBubbleBaseView(web_contents, delegate, prompt_style) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_title_margins(gfx::Insets::TLBR(10, 16, 8, 16));
  set_margins(gfx::Insets::VH(8, 12));

  SetTitle(l10n_util::GetStringUTF16(IDS_AMBIENT_LOGIN_TITLE));

  CHECK_EQ(delegate->Requests().size(), 1u);
  permissions::PermissionRequest* request = delegate->Requests().front().get();
  CHECK_EQ(request->request_type(), permissions::RequestType::kAmbientLogin);
  auto* ambient_request =
      static_cast<ambient_signin::AmbientLoginPermissionRequest*>(request);
  std::u16string username = ambient_request->username();
  const std::u16string& provider = ambient_request->provider_name();
  if (username.empty()) {
    username = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);

  // 1. Icon on the left.
  auto icon_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon, kIconSize));
  AddChildView(std::move(icon_view));

  // 2. Two text lines: Username (slightly bolded) and Provider.
  auto text_container = std::make_unique<views::View>();
  auto* text_layout =
      text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
  text_layout->set_cross_axis_alignment(views::LayoutAlignment::kStart);

  auto username_label = std::make_unique<views::Label>(
      username, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_3_MEDIUM);
  username_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  username_label->SetElideBehavior(gfx::ELIDE_EMAIL);
  text_container->AddChildView(std::move(username_label));

  if (!provider.empty()) {
    auto provider_label = std::make_unique<views::Label>(
        provider, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_4);
    provider_label->SetEnabledColor(ui::kColorLabelForegroundSecondary);
    provider_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    provider_label->SetElideBehavior(gfx::ELIDE_TAIL);
    text_container->AddChildView(std::move(provider_label));
  }

  layout->SetFlexForView(AddChildView(std::move(text_container)), 1);

  // 3. Sign In button.
  auto sign_in_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &AmbientLoginPermissionBubbleView::OnSignInButtonClicked,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
  sign_in_button->SetStyle(ui::ButtonStyle::kProminent);
  sign_in_button->SetProperty(views::kElementIdentifierKey,
                              kAllowButtonElementId);
  std::u16string button_description =
      provider.empty() ? username : base::StrCat({username, u" ", provider});
  sign_in_button->GetViewAccessibility().SetDescription(button_description);
  sign_in_button_ = AddChildView(std::move(sign_in_button));
}

AmbientLoginPermissionBubbleView::~AmbientLoginPermissionBubbleView() = default;

void AmbientLoginPermissionBubbleView::Show() {
  CHECK(GetNativeWindow());
  UpdateAnchorPosition();
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelSecurity);
  ShowWidget();
}

void AmbientLoginPermissionBubbleView::RunButtonCallback(int button_id) {
  CHECK_EQ(GetPermissionDialogButton(button_id),
           PermissionDialogButton::kAccept);
  PermissionPromptBubbleBaseView::RunButtonCallback(button_id);
}

void AmbientLoginPermissionBubbleView::AddedToWidget() {
  PermissionPromptBubbleBaseView::AddedToWidget();

  auto title_label = std::make_unique<views::Label>(
      GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_BODY_3_MEDIUM);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
  title_label->GetViewAccessibility().SetHierarchicalLevel(1);
  GetBubbleFrameView()->SetTitleView(std::move(title_label));
}

void AmbientLoginPermissionBubbleView::OnSignInButtonClicked(
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false)) {
    return;
  }
  if (ShouldIgnoreButtonPressedEventHandling(sign_in_button_, event)) {
    return;
  }
  RunButtonCallback(static_cast<int>(PermissionDialogButton::kAccept));
}

BEGIN_METADATA(AmbientLoginPermissionBubbleView)
END_METADATA
