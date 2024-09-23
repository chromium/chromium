// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/permissions/features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EmbeddedPermissionPromptBaseView,
                                      kMainViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EmbeddedPermissionPromptBaseView,
                                      kLabelViewId1);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EmbeddedPermissionPromptBaseView,
                                      kLabelViewId2);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EmbeddedPermissionPromptBaseView,
                                      kTitleViewId);

using permissions::feature_params::PermissionElementPromptPosition;

namespace {

constexpr int BODY_TOP_MARGIN = 10;
constexpr int DISTANCE_BUTTON_VERTICAL = 8;

void AddElementIdentifierToLabel(views::Label& label, size_t index) {
  ui::ElementIdentifier id;
  switch (index) {
    case 0:
      id = EmbeddedPermissionPromptBaseView::kLabelViewId1;
      break;
    case 1:
      id = EmbeddedPermissionPromptBaseView::kLabelViewId2;
      break;
    default:
      return;
  }

  label.SetProperty(views::kElementIdentifierKey, id);
}

std::unique_ptr<views::View> AddSpacer() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto spacer = std::make_unique<views::View>();
  spacer->SetPreferredSize(
      gfx::Size(provider->GetDistanceMetric(
                    DISTANCE_PERMISSION_PROMPT_HORIZONTAL_ICON_LABEL_PADDING),
                /*height=*/1));
  return spacer;
}

int GetPermissionIconSize() {
  return 20;
}

}  // namespace

EmbeddedPermissionPromptBaseView::EmbeddedPermissionPromptBaseView(
    Browser* browser,
    base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate)
    : PermissionPromptBaseView(browser,
                               delegate->GetPermissionPromptDelegate()),
      delegate_(delegate) {
  SetProperty(views::kElementIdentifierKey, kMainViewId);

  CHECK_GT(delegate_->Requests().size(), 0u);
  element_rect_ = delegate_->Requests()[0]->GetAnchorElementPosition().value_or(
      gfx::Rect());
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      delegate_->Requests()[0]->get_requesting_frame_id());
  if (rfh && rfh->GetView()) {
    element_rect_ = gfx::Rect(
        rfh->GetView()->TransformPointToRootCoordSpace(element_rect_.origin()),
        element_rect_.size());
  }
}

EmbeddedPermissionPromptBaseView::~EmbeddedPermissionPromptBaseView() = default;

void EmbeddedPermissionPromptBaseView::Show() {
  CreateWidget();
  ShowWidget();
}

const gfx::VectorIcon& EmbeddedPermissionPromptBaseView::GetIcon() const {
  return gfx::kNoneIcon;
}

bool EmbeddedPermissionPromptBaseView::ShowLoadingIcon() const {
  return false;
}

void EmbeddedPermissionPromptBaseView::CreateWidget() {
  DCHECK(browser()->window());
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelSecurity);
}

std::unique_ptr<views::FlexLayoutView>
EmbeddedPermissionPromptBaseView::CreateLoadingIcon() {
  auto throbber_container = std::make_unique<views::FlexLayoutView>();
  throbber_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  throbber_container->SetOrientation(views::LayoutOrientation::kVertical);
  auto throbber = std::make_unique<views::Throbber>();
  throbber->SetPreferredSize(
      gfx::Size(GetPermissionIconSize(), GetPermissionIconSize()));

  throbber->Start();
  throbber_container->AddChildView(std::move(throbber));

  // Also add a filler view to fill in vertical space below the throbber.
  auto filler = std::make_unique<views::View>();
  throbber_container->AddChildView(std::move(filler));
  return throbber_container;
}

void EmbeddedPermissionPromptBaseView::AddedToWidget() {
  StartTrackingPictureInPictureOcclusion();

  auto title_container = std::make_unique<views::FlexLayoutView>();
  title_container->SetOrientation(views::LayoutOrientation::kHorizontal);

  const gfx::VectorIcon& vector_icon = GetIcon();

  if (!vector_icon.is_empty()) {
    auto icon =
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            vector_icon, ui::kColorIcon, GetPermissionIconSize()));
    icon->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
    title_container->AddChildView(std::move(icon));

    // Add space between the icon and the text.
    title_container->AddChildView(AddSpacer());
  }

  auto label = std::make_unique<views::Label>(
      GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetCollapseWhenHidden(true);
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kScaleToMaximum,
                               /*adjust_height_for_width=*/true));
  label->SetProperty(views::kElementIdentifierKey,
                     EmbeddedPermissionPromptBaseView::kTitleViewId);

  if (ShowLoadingIcon()) {
    title_container->AddChildView(CreateLoadingIcon());

    // Add space between the icon and the text.
    title_container->AddChildView(AddSpacer());
  }

  title_container->AddChildView(std::move(label));

  GetBubbleFrameView()->SetTitleView(std::move(title_container));
}

void EmbeddedPermissionPromptBaseView::ClosingPermission() {
  if (delegate()) {
    delegate()->Dismiss();
  }
}

void EmbeddedPermissionPromptBaseView::PrepareToClose() {
  DialogDelegate::SetCloseCallback(base::DoNothing());
}

PermissionElementPromptPosition
EmbeddedPermissionPromptBaseView::GetPromptPosition() const {
  CHECK(base::FeatureList::IsEnabled(blink::features::kPermissionElement));
  if (!base::FeatureList::IsEnabled(
          permissions::features::kPermissionElementPromptPositioning)) {
    return PermissionElementPromptPosition::kWindowMiddle;
  }

  if (permissions::feature_params::kPermissionElementPromptPositioningParam
              .Get() == PermissionElementPromptPosition::kNearElement &&
      element_rect_.IsEmpty()) {
    return PermissionElementPromptPosition::kWindowMiddle;
  }

  return permissions::feature_params::kPermissionElementPromptPositioningParam
      .Get();
}

void EmbeddedPermissionPromptBaseView::ShowWidget() {
  GetWidget()->Show();
}

void EmbeddedPermissionPromptBaseView::UpdateAnchor(views::Widget* widget) {
  if (GetPromptPosition() == PermissionElementPromptPosition::kLegacyPrompt) {
    AnchorToPageInfoOrChip();
    return;
  }
  SetAnchorView(widget->GetContentsView());
  set_parent_window(
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));

  SetArrow(views::BubbleBorder::Arrow::BOTTOM_LEFT);
}

bool EmbeddedPermissionPromptBaseView::ShouldShowCloseButton() const {
  return true;
}

void EmbeddedPermissionPromptBaseView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      DISTANCE_BUTTON_VERTICAL));

  set_close_on_deactivate(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  DialogDelegate::SetCloseCallback(
      base::BindOnce(&EmbeddedPermissionPromptBaseView::ClosingPermission,
                     base::Unretained(this)));

  auto requests_configuration = GetRequestLinesConfiguration();
  int index = 0;
  for (auto& request : requests_configuration) {
    AddRequestLine(request, index++);
  }

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  auto buttons_container = std::make_unique<views::View>();
  buttons_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      DISTANCE_BUTTON_VERTICAL));

  auto buttons_configuration = GetButtonsConfiguration();

  for (auto& button : buttons_configuration) {
    AddButton(*buttons_container, button);
  }

  views::LayoutProvider* const layout_provider = views::LayoutProvider::Get();
  buttons_container->SetPreferredSize(gfx::Size(
      layout_provider->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
          layout_provider->GetInsetsMetric(views::INSETS_DIALOG_BUTTON_ROW)
              .width(),
      buttons_container->GetPreferredSize().height()));

  SetExtraView(std::move(buttons_container));
}

void EmbeddedPermissionPromptBaseView::AddRequestLine(
    const RequestLineConfiguration& line,
    std::size_t index) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto* line_container = AddChildViewAt(std::make_unique<views::View>(), index);
  line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, provider->GetDistanceMetric(
                             DISTANCE_SUBSECTION_HORIZONTAL_INDENT)),
      provider->GetDistanceMetric(
          DISTANCE_PERMISSION_PROMPT_HORIZONTAL_ICON_LABEL_PADDING)));

  if (line.icon) {
    auto* icon = line_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            *line.icon, ui::kColorIcon, GetPermissionIconSize())));
    icon->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  }

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(line.message));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddElementIdentifierToLabel(*label, index);

  label->SetTextStyle(views::style::STYLE_BODY_3);
  label->SetEnabledColorId(kColorPermissionPromptRequestText);

  line_container->SetProperty(views::kMarginsKey,
                              gfx::Insets().set_top(BODY_TOP_MARGIN));
}

void EmbeddedPermissionPromptBaseView::AddButton(
    views::View& buttons_container,
    const ButtonConfiguration& button) {
  auto button_view = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&EmbeddedPermissionPromptBaseView::
                              FilterUnintenedEventsAndRunCallbacks,
                          base::Unretained(this), GetViewId(button.type)),
      button.label);
  button_view->SetID(GetViewId(button.type));

  button_view->SetStyle(button.style);

  button_view->SetProperty(views::kElementIdentifierKey, button.identifier);

  buttons_container.AddChildView(std::move(button_view));
}

gfx::Rect EmbeddedPermissionPromptBaseView::GetBubbleBounds() {
  if (GetPromptPosition() == PermissionElementPromptPosition::kLegacyPrompt) {
    return views::BubbleDialogDelegateView::GetBubbleBounds();
  }

  gfx::Rect default_bounds = views::BubbleDialogDelegateView::GetBubbleBounds();

  content::WebContents* web_contents =
      delegate_->GetPermissionPromptDelegate()->GetAssociatedWebContents();

  gfx::Rect container_bounds = web_contents->GetContainerBounds();
  gfx::Rect prompt_bounds;

  if (GetPromptPosition() == PermissionElementPromptPosition::kNearElement) {
    // First, attempt to position the prompt below the PEPC, if it would not
    // overflow the container bounds.
    prompt_bounds =
        gfx::Rect(default_bounds.x() + element_rect_.bottom_center().x() -
                      default_bounds.width() / 2,
                  default_bounds.y() + element_rect_.bottom_center().y() +
                      default_bounds.height(),
                  default_bounds.width(), default_bounds.height());

    if (container_bounds.Contains(prompt_bounds)) {
      return prompt_bounds;
    }

    // Second, attempt to position the prompt above the PEPC, if it would not
    // overflow the container bounds.
    prompt_bounds =
        gfx::Rect(default_bounds.x() + element_rect_.top_center().x() -
                      default_bounds.width() / 2,
                  default_bounds.y() + element_rect_.top_center().y(),
                  default_bounds.width(), default_bounds.height());

    if (container_bounds.Contains(prompt_bounds)) {
      return prompt_bounds;
    }
    // Otherwise, default to kWindowMiddle placement logic.
  }

  // At this point we're either in the kWindowMiddle case or the kNearElement
  // case after failing to place the prompt near the element.
  prompt_bounds = gfx::Rect(
      container_bounds.CenterPoint().x() - default_bounds.width() / 2,
      container_bounds.CenterPoint().y() - default_bounds.height() / 2,
      default_bounds.width(), default_bounds.height());

  // Do not allow the prompt to be positioned above the container bounds as it
  // can overlap and potentially obfuscate browser UI.
  if (prompt_bounds.y() < container_bounds.y()) {
    prompt_bounds.set_y(container_bounds.y());
  }

  return prompt_bounds;
}

BEGIN_METADATA(EmbeddedPermissionPromptBaseView)
END_METADATA
