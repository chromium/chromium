// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt_view.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/paint_vector_icon.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExclusiveAccessPermissionPromptView,
                                      kMainViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExclusiveAccessPermissionPromptView,
                                      kLabelViewId1);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExclusiveAccessPermissionPromptView,
                                      kLabelViewId2);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExclusiveAccessPermissionPromptView,
                                      kAlwaysAllowId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExclusiveAccessPermissionPromptView,
                                      kAllowThisTimeId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExclusiveAccessPermissionPromptView,
                                      kNeverAllowId);

namespace {

constexpr int kBodyTopMargin = 10;
constexpr int kButtonVerticalDistance = 8;
constexpr int kPermissionIconSize = 20;

void AddElementIdentifierToLabel(views::Label& label, size_t index) {
  ui::ElementIdentifier id;
  switch (index) {
    case 0:
      id = ExclusiveAccessPermissionPromptView::kLabelViewId1;
      break;
    case 1:
      id = ExclusiveAccessPermissionPromptView::kLabelViewId2;
      break;
    default:
      return;
  }
  label.SetProperty(views::kElementIdentifierKey, id);
}

}  // namespace

ExclusiveAccessPermissionPromptView::ExclusiveAccessPermissionPromptView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate)
    : PermissionPromptBaseView(browser, delegate),
      browser_(browser),
      delegate_(delegate) {
  SetProperty(views::kElementIdentifierKey, kMainViewId);
}

ExclusiveAccessPermissionPromptView::~ExclusiveAccessPermissionPromptView() =
    default;

std::u16string ExclusiveAccessPermissionPromptView::GetAccessibleWindowTitle()
    const {
  auto& requests = delegate_->Requests();
  std::u16string display_name = GetUrlIdentityObject().name;

  if (requests.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_ONE_PERM, display_name,
        requests[0]->GetMessageTextFragment());
  }

  int template_id =
      requests.size() == 2
          ? IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS
          : IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS_MORE;
  return l10n_util::GetStringFUTF16(template_id, display_name,
                                    requests[0]->GetMessageTextFragment(),
                                    requests[1]->GetMessageTextFragment());
}

std::u16string ExclusiveAccessPermissionPromptView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    GetUrlIdentityObject().name);
}

void ExclusiveAccessPermissionPromptView::RunButtonCallback(int button_id) {
  if (!delegate_) {
    return;
  }
  ButtonType button = GetButtonType(button_id);
  if (button == ButtonType::kAllowThisTime) {
    delegate_->AcceptThisTime();
  } else if (button == ButtonType::kAlwaysAllow) {
    delegate_->Accept();
  } else if (button == ButtonType::kNeverAllow) {
    delegate_->Deny();
  }
}

void ExclusiveAccessPermissionPromptView::Show() {
  CreateWidget();
  ShowWidget();
}

void ExclusiveAccessPermissionPromptView::CreateWidget() {
  DCHECK(browser_->window());
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelSecurity);
}

void ExclusiveAccessPermissionPromptView::AddedToWidget() {
  StartTrackingPictureInPictureOcclusion();

  auto title_container = std::make_unique<views::FlexLayoutView>();
  title_container->SetOrientation(views::LayoutOrientation::kHorizontal);

  auto label = std::make_unique<views::Label>(
      GetWindowTitle(), views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetCollapseWhenHidden(true);
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kScaleToMaximum,
                               /*adjust_height_for_width=*/true));
  AddElementIdentifierToLabel(*label, /*index*/ 0);

  title_container->AddChildView(std::move(label));

  GetBubbleFrameView()->SetTitleView(std::move(title_container));
}

void ExclusiveAccessPermissionPromptView::PrepareToClose() {
  DialogDelegate::SetCloseCallback(base::DoNothing());
}

void ExclusiveAccessPermissionPromptView::ShowWidget() {
  GetWidget()->Show();
}

void ExclusiveAccessPermissionPromptView::UpdateAnchor(views::Widget* widget) {
  SetAnchorView(widget->GetContentsView());
  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));
  SetArrow(views::BubbleBorder::Arrow::FLOAT);
}

bool ExclusiveAccessPermissionPromptView::ShouldShowCloseButton() const {
  return true;
}

void ExclusiveAccessPermissionPromptView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kButtonVerticalDistance));

  set_close_on_deactivate(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  DialogDelegate::SetCloseCallback(
      base::BindOnce(&ExclusiveAccessPermissionPromptView::ClosingPermission,
                     base::Unretained(this)));

  int index = 0;
  for (permissions::PermissionRequest* request : delegate_->Requests()) {
    AddRequestLine(&permissions::GetIconId(request->request_type()),
                   request->GetMessageTextFragment(), index++);
  }
  InitButtons();
}

void ExclusiveAccessPermissionPromptView::InitButtons() {
  // Hide the OK/Cancel buttons that are shown by default on dialogs.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  auto buttons_container = std::make_unique<views::View>();
  buttons_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kButtonVerticalDistance));

  if (permissions::feature_params::kShowAllowAlwaysAsFirstButton.Get()) {
    AddAlwaysAllowButton(*buttons_container);
    AddAllowThisTimeButton(*buttons_container);
  } else {
    AddAllowThisTimeButton(*buttons_container);
    AddAlwaysAllowButton(*buttons_container);
  }
  AddButton(*buttons_container,
            l10n_util::GetStringUTF16(IDS_PERMISSION_NEVER_ALLOW),
            ButtonType::kNeverAllow, ui::ButtonStyle::kTonal, kNeverAllowId);

  views::LayoutProvider* const layout_provider = views::LayoutProvider::Get();
  buttons_container->SetPreferredSize(gfx::Size(
      layout_provider->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
          layout_provider->GetInsetsMetric(views::INSETS_DIALOG_BUTTON_ROW)
              .width(),
      buttons_container->GetPreferredSize().height()));

  SetExtraView(std::move(buttons_container));
}

void ExclusiveAccessPermissionPromptView::AddRequestLine(
    raw_ptr<const gfx::VectorIcon> icon,
    const std::u16string& message,
    std::size_t index) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto* line_container = AddChildViewAt(std::make_unique<views::View>(), index);
  line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, provider->GetDistanceMetric(
                             DISTANCE_SUBSECTION_HORIZONTAL_INDENT)),
      provider->GetDistanceMetric(
          DISTANCE_PERMISSION_PROMPT_HORIZONTAL_ICON_LABEL_PADDING)));

  if (icon) {
    auto* icon_view = line_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            *icon, ui::kColorIcon, kPermissionIconSize)));
    icon_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  }

  auto* label =
      line_container->AddChildView(std::make_unique<views::Label>(message));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddElementIdentifierToLabel(*label, index);
  label->SetTextStyle(views::style::STYLE_BODY_3);
  label->SetEnabledColorId(kColorPermissionPromptRequestText);

  line_container->SetProperty(views::kMarginsKey,
                              gfx::Insets().set_top(kBodyTopMargin));
}

void ExclusiveAccessPermissionPromptView::AddButton(
    views::View& buttons_container,
    const std::u16string& label,
    ButtonType type,
    ui::ButtonStyle style,
    ui::ElementIdentifier identifier) {
  auto button_view = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ExclusiveAccessPermissionPromptView::
                              FilterUnintenedEventsAndRunCallbacks,
                          base::Unretained(this), GetViewId(type)),
      label);
  button_view->SetID(GetViewId(type));
  button_view->SetStyle(style);
  button_view->SetProperty(views::kElementIdentifierKey, identifier);
  buttons_container.AddChildView(std::move(button_view));
}

void ExclusiveAccessPermissionPromptView::AddAlwaysAllowButton(
    views::View& buttons_container) {
  AddButton(buttons_container,
            l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_EVERY_VISIT),
            ButtonType::kAlwaysAllow, ui::ButtonStyle::kTonal, kAlwaysAllowId);
}

void ExclusiveAccessPermissionPromptView::AddAllowThisTimeButton(
    views::View& buttons_container) {
  AddButton(buttons_container,
            l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME),
            ButtonType::kAllowThisTime, ui::ButtonStyle::kTonal,
            kAllowThisTimeId);
}

void ExclusiveAccessPermissionPromptView::ClosingPermission() {
  if (delegate_) {
    delegate_->Dismiss();
  }
}

BEGIN_METADATA(ExclusiveAccessPermissionPromptView)
END_METADATA
