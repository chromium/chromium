// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/ambient_login_permission_bubble_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

constexpr int kIconSize = 20;
constexpr int kViewportBottomMargin = 48;

const gfx::VectorIcon& GetCredentialIcon(ambient_signin::CredentialType type) {
  switch (type) {
    case ambient_signin::CredentialType::kPasskey:
      return features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                               : vector_icons::kPasskeyOldIcon;
    case ambient_signin::CredentialType::kPassword:
      return features::IsRoundedIconsEnabled()
                 ? vector_icons::kPasswordManagerIcon
                 : vector_icons::kPasswordManagerOldIcon;
  }
}

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

  std::u16string username;
  std::u16string provider;
  const gfx::VectorIcon* credential_icon = &GooglePasswordManagerVectorIcon();
  CHECK_EQ(delegate->Requests().size(), 1u);
  ambient_signin::AmbientLoginPermissionRequest* ambient_request =
      GetAmbientRequest();
  CHECK(ambient_request);
  if (!ambient_request->credentials().empty()) {
    const ambient_signin::PasskeyOrPasswordCredential& cred =
        ambient_request->credentials().front();
    username = cred.username;
    provider = cred.provider_name;
    credential_icon = &GetCredentialIcon(cred.type);
  } else if (!ambient_request->federated_credentials().empty()) {
    const ambient_signin::FederatedCredential& fed_cred =
        ambient_request->federated_credentials().front();
    username =
        !fed_cred.account_name.empty() ? fed_cred.account_name : fed_cred.email;
    provider = fed_cred.idp_name;
  }

  if (username.empty()) {
    username = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);

  auto icon_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          *credential_icon, ui::kColorIcon, kIconSize));
  AddChildView(std::move(icon_view));

  auto text_container = std::make_unique<views::View>();
  text_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, views::LayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  auto* text_layout =
      text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
  text_layout->set_cross_axis_alignment(views::LayoutAlignment::kStart);

  auto username_label = std::make_unique<views::Label>(
      username, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_3_MEDIUM);
  username_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  username_label->SetElideBehavior(gfx::ELIDE_TAIL);
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

  auto sign_in_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &AmbientLoginPermissionBubbleView::OnSignInButtonClicked,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
  sign_in_button->SetStyle(ui::ButtonStyle::kProminent);
  sign_in_button->SetProperty(views::kElementIdentifierKey,
                              kAllowButtonElementId);
  sign_in_button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0,
                        views::LayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  std::u16string button_description =
      provider.empty() ? username : base::StrCat({username, u" ", provider});
  sign_in_button->GetViewAccessibility().SetDescription(button_description);
  sign_in_button_ = AddChildView(std::move(sign_in_button));

  std::unique_ptr<views::ImageButton> expand_button =
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              &AmbientLoginPermissionBubbleView::OnExpandButtonClicked,
              base::Unretained(this)),
          kUnfoldMoreIcon, kIconSize);
  expand_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ANCHORED_MESSAGE_EXPAND_BUTTON_TOOLTIP));
  views::InstallCircleHighlightPathGenerator(expand_button.get());
  AddChildView(std::move(expand_button));
}

AmbientLoginPermissionBubbleView::~AmbientLoginPermissionBubbleView() = default;

void AmbientLoginPermissionBubbleView::Show() {
  CHECK(GetNativeWindow());
  UpdateAnchorPosition();
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelSecurity);
  ShowWidget();
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

ambient_signin::AmbientLoginPermissionRequest*
AmbientLoginPermissionBubbleView::GetAmbientRequest() {
  if (!delegate() || delegate()->Requests().empty() ||
      delegate()->Requests().front()->request_type() !=
          permissions::RequestType::kAmbientLogin) {
    return nullptr;
  }
  return static_cast<ambient_signin::AmbientLoginPermissionRequest*>(
      delegate()->Requests().front().get());
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
  if (ambient_signin::AmbientLoginPermissionRequest* ambient_request =
          GetAmbientRequest()) {
    if (!ambient_request->credentials().empty()) {
      ambient_request->SelectCredential(0);
    } else if (!ambient_request->federated_credentials().empty()) {
      ambient_request->SelectFederatedCredential(0);
    }
  }
  RunButtonCallback(static_cast<int>(PermissionDialogButton::kAccept));
}

void AmbientLoginPermissionBubbleView::OnExpandButtonClicked(
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false) ||
      ShouldIgnoreButtonPressedEventHandling(nullptr, event)) {
    return;
  }

  // Transitioning from the collapsed single-credential summary to the expanded
  // credential list replaces all child views, including `sign_in_button_` and
  // the expand button that triggered this callback. Clearing `sign_in_button_`
  // first avoids a dangling `raw_ptr`. Destruction of a clicked button during
  // its listener callback is explicitly supported (eg., see comment in
  // ui::views::ButtonController::OnMousePressed()).
  sign_in_button_ = nullptr;
  RemoveAllChildViews();
  set_margins(gfx::Insets::VH(8, 0));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto list_view = std::make_unique<views::View>();
  views::BoxLayout* list_layout =
      list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  list_layout->set_cross_axis_alignment(views::LayoutAlignment::kStretch);

  views::View* first_row = nullptr;
  if (ambient_signin::AmbientLoginPermissionRequest* ambient_request =
          GetAmbientRequest()) {
    for (size_t i = 0; i < ambient_request->credentials().size(); ++i) {
      HoverButton* row = list_view->AddChildView(
          CreateCredentialRow(ambient_request->credentials()[i], i));
      if (!first_row) {
        first_row = row;
      }
    }
    for (size_t i = 0; i < ambient_request->federated_credentials().size();
         ++i) {
      HoverButton* row = list_view->AddChildView(CreateFederatedCredentialRow(
          ambient_request->federated_credentials()[i], i));
      if (!first_row) {
        first_row = row;
      }
    }
  }

  views::ScrollView* scroll_view =
      AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetContents(std::move(list_view));

  int max_scroll_height = scroll_view->GetPreferredSize().height();
  if (web_contents()) {
    const gfx::Rect viewport_bounds = web_contents()->GetContainerBounds();
    if (!viewport_bounds.IsEmpty()) {
      const gfx::Rect unclipped_bubble_bounds =
          GetBubbleFrameView()->GetUpdatedWindowBounds(
              GetAnchorRect(), arrow(),
              GetWidget()->client_view()->GetPreferredSize({}),
              /*adjust_to_fit_available_bounds=*/false);
      const int max_bubble_bottom =
          viewport_bounds.bottom() - kViewportBottomMargin;
      if (unclipped_bubble_bounds.bottom() > max_bubble_bottom) {
        const int overflow =
            unclipped_bubble_bounds.bottom() - max_bubble_bottom;
        max_scroll_height = std::max(0, max_scroll_height - overflow);
      }
    }
  }
  scroll_view->ClipHeightTo(0, max_scroll_height);

  SizeToContents();
  if (first_row) {
    first_row->RequestFocus();
  }
}

void AmbientLoginPermissionBubbleView::OnCredentialClicked(
    size_t index,
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false) ||
      ShouldIgnoreButtonPressedEventHandling(nullptr, event)) {
    return;
  }
  if (ambient_signin::AmbientLoginPermissionRequest* ambient_request =
          GetAmbientRequest()) {
    ambient_request->SelectCredential(index);
  }
  RunButtonCallback(static_cast<int>(PermissionDialogButton::kAccept));
}

void AmbientLoginPermissionBubbleView::OnFederatedCredentialClicked(
    size_t index,
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false) ||
      ShouldIgnoreButtonPressedEventHandling(nullptr, event)) {
    return;
  }
  if (ambient_signin::AmbientLoginPermissionRequest* ambient_request =
          GetAmbientRequest()) {
    ambient_request->SelectFederatedCredential(index);
  }
  RunButtonCallback(static_cast<int>(PermissionDialogButton::kAccept));
}

std::unique_ptr<HoverButton>
AmbientLoginPermissionBubbleView::CreateCredentialRow(
    const ambient_signin::PasskeyOrPasswordCredential& credential,
    size_t index) {
  std::u16string username =
      credential.username.empty()
          ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
          : credential.username;

  HoverButton::Params params;
  params.icon_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GetCredentialIcon(credential.type), ui::kColorIcon, kIconSize));
  if (credential.type == ambient_signin::CredentialType::kPasskey &&
      !credential.display_name.empty() && credential.display_name != username) {
    params.title = credential.display_name;
    params.subtitle = username;
    params.footer = credential.provider_name;
  } else {
    params.title = username;
    params.subtitle = credential.provider_name;
  }

  auto row = std::make_unique<HoverButton>(
      base::BindRepeating(
          &AmbientLoginPermissionBubbleView::OnCredentialClicked,
          base::Unretained(this), index),
      std::move(params));
  row->title()->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  row->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                            views::style::STYLE_BODY_4);
  row->SetFooterTextStyle(views::style::CONTEXT_LABEL,
                          views::style::STYLE_BODY_4);
  return row;
}

std::unique_ptr<HoverButton>
AmbientLoginPermissionBubbleView::CreateFederatedCredentialRow(
    const ambient_signin::FederatedCredential& credential,
    size_t index) {
  HoverButton::Params params;
  params.icon_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon, kIconSize));
  if (!credential.account_name.empty() && !credential.email.empty()) {
    params.title = credential.account_name;
    params.subtitle = credential.email;
    params.footer = credential.idp_name;
  } else {
    std::u16string identifier = !credential.account_name.empty()
                                    ? credential.account_name
                                    : credential.email;
    if (identifier.empty()) {
      identifier = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
    }
    params.title = identifier;
    params.subtitle = credential.idp_name;
  }

  auto row = std::make_unique<HoverButton>(
      base::BindRepeating(
          &AmbientLoginPermissionBubbleView::OnFederatedCredentialClicked,
          base::Unretained(this), index),
      std::move(params));
  row->title()->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  row->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                            views::style::STYLE_BODY_4);
  row->SetFooterTextStyle(views::style::CONTEXT_LABEL,
                          views::style::STYLE_BODY_4);
  return row;
}

BEGIN_METADATA(AmbientLoginPermissionBubbleView)
END_METADATA
