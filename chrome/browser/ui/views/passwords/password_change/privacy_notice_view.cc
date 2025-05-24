// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/privacy_notice_view.h"

#include <memory>

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/privacy_notice_bubble_view_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"

using TableLayout = views::TableLayout;
using LayoutAlignment = views::LayoutAlignment;
using ClosedReason = views::Widget::ClosedReason;

namespace {
// The corner radius of the text area in the bubble.
const float kCornerRadius = 12;
constexpr int kIconSize = 16;

std::unique_ptr<views::View> CreateLabel(const std::u16string& text,
                                         int style) {
  auto label = std::make_unique<views::Label>();
  label->SetText(text);
  label->SetTextStyle(style);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

void CreateRow(views::View& table_root_view,
               const gfx::VectorIcon& icon_id,
               int string_id) {
  table_root_view.AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(icon_id, ui::kColorIcon, kIconSize)));
  table_root_view.AddChildView(CreateLabel(l10n_util::GetStringUTF16(string_id),
                                           views::style::STYLE_SECONDARY));
}

std::unique_ptr<views::View> CreateThingsToConsiderList() {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  const int related_control_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int label_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto table_root_view = std::make_unique<views::TableLayoutView>();
  table_root_view
      ->AddColumn(LayoutAlignment::kCenter, LayoutAlignment::kCenter,
                  TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed,
                  kIconSize, kIconSize)
      .AddPaddingColumn(TableLayout::kFixedSize, label_padding)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, related_control_padding)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, related_control_padding)
      .AddRows(1, TableLayout::kFixedSize);

  CreateRow(
      *table_root_view, views::kPsychiatryIcon,
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_ITEM_EXPERIMENTAL);
  CreateRow(
      *table_root_view, views::kAccountBoxIcon,
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_ITEM_HUMAN_REVIEW);
  CreateRow(
      *table_root_view, vector_icons::kLockIcon,
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_ITEM_ENCRYPTED);
  return table_root_view;
}

}  // namespace

PrivacyNoticeView::PrivacyNoticeView(content::WebContents* web_contents,
                                     views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* root_view = AddChildView(std::make_unique<views::BoxLayoutView>());

  root_view->SetOrientation(views::LayoutOrientation::kVertical);
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  root_view->SetInsideBorderInsets(gfx::Insets::VH(
      layout_provider->GetDistanceMetric(views::DISTANCE_CONTROL_LIST_VERTICAL),
      // TODO(pkasting): Why is this using a vertical distance metric for a
      // horizontal inset?
      layout_provider->GetDistanceMetric(
          views::DISTANCE_CONTROL_LIST_VERTICAL)));
  root_view->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  const int spacing = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  root_view->SetBetweenChildSpacing(spacing);

  root_view->SetBackground(
      views::CreateRoundedRectBackground(ui::kColorSysSurface4,
                                         /*top_radius=*/kCornerRadius,
                                         /*bottom_radius=*/kCornerRadius));
  root_view->AddChildView(CreateLabel(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_SUBTITLE),
      views::style::STYLE_BODY_4_MEDIUM));
  root_view->AddChildView(CreateThingsToConsiderList());

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CONTINUE));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CANCEL));
  SetAcceptCallback(
      base::BindOnce(&PrivacyNoticeBubbleViewController::AcceptNotice,
                     base::Unretained(&controller_)));
  SetCancelCallback(base::BindOnce(&PrivacyNoticeBubbleViewController::Cancel,
                                   base::Unretained(&controller_)));
  SetCloseCallback(base::BindRepeating(
      [](PrivacyNoticeView* view) {
        ClosedReason reason = view->GetWidget()->closed_reason();
        // Cancel the flow if the dialog is explicitly closed.
        if (reason == ClosedReason::kCloseButtonClicked ||
            reason == ClosedReason::kEscKeyPressed) {
          view->controller_.Cancel();
        }
      },
      this));
}

PrivacyNoticeView::~PrivacyNoticeView() = default;

PasswordBubbleControllerBase* PrivacyNoticeView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PrivacyNoticeView::GetController() const {
  return &controller_;
}

BEGIN_METADATA(PrivacyNoticeView)
END_METADATA
