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
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

constexpr int BODY_TOP_MARGIN = 10;
constexpr int DISTANCE_BUTTON_VERTICAL = 8;

}  // namespace

const std::vector<permissions::PermissionRequest*>&
EmbeddedPermissionPromptBaseView::Delegate::Requests() const {
  if (auto permission_prompt_delegate = GetPermissionPromptDelegate()) {
    return permission_prompt_delegate->Requests();
  }
  NOTREACHED();
  static const std::vector<permissions::PermissionRequest*> empty_requests;
  return empty_requests;
}

EmbeddedPermissionPromptBaseView::EmbeddedPermissionPromptBaseView(
    Browser* browser,
    base::WeakPtr<Delegate> delegate)
    : PermissionPromptBaseView(browser,
                               delegate->GetPermissionPromptDelegate()),
      browser_(browser),
      delegate_(delegate) {}

EmbeddedPermissionPromptBaseView::~EmbeddedPermissionPromptBaseView() = default;

void EmbeddedPermissionPromptBaseView::Show() {
  CreateWidget();
  ShowWidget();
}

void EmbeddedPermissionPromptBaseView::CreateWidget() {
  DCHECK(browser_->window());

  UpdateAnchorPosition();

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  if (base::FeatureList::IsEnabled(views::features::kWidgetLayering)) {
    widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelSecurity);
  }
}

void EmbeddedPermissionPromptBaseView::ClosingPermission() {
  if (delegate()) {
    delegate()->Dismiss();
  }
}

void EmbeddedPermissionPromptBaseView::PrepareToClose() {
  DialogDelegate::SetCloseCallback(base::DoNothing());
}

void EmbeddedPermissionPromptBaseView::ShowWidget() {
  GetWidget()->Show();

  SizeToContents();
}

void EmbeddedPermissionPromptBaseView::UpdateAnchorPosition() {
  SetAnchorView(
      BrowserView::GetBrowserViewForBrowser(browser_)->GetContentsView());
  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));
  SetArrow(views::BubbleBorder::Arrow::FLOAT);
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

  SetButtons(ui::DIALOG_BUTTON_NONE);

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
  const int kPermissionIconSize = features::IsChromeRefresh2023() ? 20 : 18;

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
            *line.icon, ui::kColorIcon, kPermissionIconSize)));
    icon->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  }

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(line.message));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);

  if (features::IsChromeRefresh2023()) {
    label->SetTextStyle(views::style::STYLE_BODY_3);
    label->SetEnabledColorId(kColorPermissionPromptRequestText);

    line_container->SetProperty(views::kMarginsKey,
                                gfx::Insets().set_top(BODY_TOP_MARGIN));
  }
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

  buttons_container.AddChildView(std::move(button_view));
}
