// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/anchored_message_view.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {

const gfx::Insets kChipContainerBorderInset = gfx::Insets::TLBR(8, 12, 8, 16);
const int kChipContainerLineHeight = 20;
const int kChipContainerChildSpacing = 8;
const int kChipContainerHeight = 36;
const int kAnchoredMessageHeight = 52;
const gfx::Insets kAnchoredMessageMarginsInset = gfx::Insets::TLBR(8, 16, 8, 9);
const gfx::Insets kAnchoredMessageIconMarginsInset =
    gfx::Insets::TLBR(0, 0, 0, 12);
const int kAnchoredMessageIconSize = 20;
const int kAnchoredMessageSpaceLeftOfChip = 16;
const gfx::Insets kAnchoreMessageActionIconMarginsInset =
    gfx::Insets::TLBR(0, 8, 0, 0);

// ChipContainerView holds the clickable chip of the anchored message, similar
// to the Suggestion Chip version of the Page Action View.
class ChipContainerView : public views::View {
  METADATA_HEADER(ChipContainerView, views::View)
 public:
  ChipContainerView(const std::u16string& label_text,
                    const std::optional<ui::ImageModel>& icon,
                    const raw_ref<AnchoredMessageBubbleView::Delegate> delegate)
      : delegate_(delegate) {
    icon_view_ = AddChildView(std::make_unique<views::ImageView>());
    icon_view_->SetVisible(false);
    icon_view_->SetProperty(
        views::kElementIdentifierKey,
        AnchoredMessageBubbleView::kAnchoredMessageChipIconId);
    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetVisible(false);
    label_->SetAutoColorReadabilityEnabled(false);
    label_->SetLineHeight(kChipContainerLineHeight);
    label_->SetProperty(views::kElementIdentifierKey,
                        AnchoredMessageBubbleView::kAnchoredMessageChipLabelId);

    SetLayoutManager(std::make_unique<views::BoxLayout>(
                         views::BoxLayout::Orientation::kHorizontal,
                         gfx::Insets(), kChipContainerChildSpacing))
        ->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kCenter);
    SetBorder(views::CreateEmptyBorder(kChipContainerBorderInset));

    Update(label_text, icon);
  }

  ~ChipContainerView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const ui::ColorProvider* color_provider = GetColorProvider();

    if (!color_provider) {
      return;
    }

    SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetColor(ui::kColorSysPrimary),
        kChipContainerHeight / 2));
    if (icon_) {
      // Assuming `icon_` is a vector icon, re-colorize it.
      if (icon_->IsVectorIcon()) {
        ui::ImageModel colored_icon = ui::ImageModel::FromVectorIcon(
            *icon_->GetVectorIcon().vector_icon(),
            color_provider->GetColor(ui::kColorSysOnPrimary),
            kAnchoredMessageIconSize);
        icon_view_->SetImage(colored_icon);
      } else {
        icon_view_->SetImage(icon_.value());
      }
      icon_view_->SetImageSize(
          gfx::Size(kAnchoredMessageIconSize, kAnchoredMessageIconSize));
    }
    label_->SetEnabledColor(color_provider->GetColor(ui::kColorSysOnPrimary));
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton()) {
      delegate_->AnchoredMessageChipClick();
    }
    // If the event has been handled, this will never be reached, so we can just
    // always return false.
    return false;
  }

 public:
  void Update(const std::u16string& label_text,
              const std::optional<ui::ImageModel>& icon) {
    icon_ = icon;
    icon_view_->SetVisible(icon_ != std::nullopt);
    label_->SetVisible(!label_text.empty());
    label_->SetText(label_text);
    OnThemeChanged();
  }

 private:
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  std::optional<ui::ImageModel> icon_;
  const raw_ref<AnchoredMessageBubbleView::Delegate> delegate_;
};

BEGIN_METADATA(ChipContainerView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageBubbleId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageIconId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageLabelId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageChipId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageChipIconId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageChipLabelId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageCloseIconId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageMenuIconId);

AnchoredMessageBubbleView::AnchoredMessageBubbleView(
    views::BubbleAnchor parent,
    const PageActionModelInterface& model,
    Delegate& delegate)
    : BubbleDialogDelegate(parent,
                           views::BubbleBorder::Arrow::TOP_RIGHT,
                           views::BubbleBorder::DIALOG_SHADOW,
                           true),
      menu_model_(model.GetAnchoredMessageMenuModel()),
      delegate_(delegate) {
  SetProperty(views::kElementIdentifierKey, kAnchoredMessageBubbleId);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetBackgroundColor(ui::kColorSysSurface);
  set_corner_radius(kAnchoredMessageHeight / 2);
  set_margins(kAnchoredMessageMarginsInset);

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetProperty(views::kMarginsKey, kAnchoredMessageIconMarginsInset);
  icon_view_->SetImageSize(
      gfx::Size(kAnchoredMessageIconSize, kAnchoredMessageIconSize));
  icon_view_->SetProperty(views::kElementIdentifierKey, kAnchoredMessageIconId);

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  label_->SetMultiLine(false);
  label_->SetProperty(views::kElementIdentifierKey, kAnchoredMessageLabelId);

  chip_container_ = AddChildView(std::make_unique<ChipContainerView>(
      std::u16string(), std::nullopt, delegate_));
  chip_container_->SetProperty(views::kElementIdentifierKey,
                               kAnchoredMessageChipId);

  close_button_ =
      AddChildView(std::make_unique<views::ImageButton>(base::BindRepeating(
          &AnchoredMessageBubbleView::Delegate::CloseAnchoredMessage,
          base::Unretained(delegate_))));
  close_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseChromeRefreshIcon,
                                     ui::kColorIcon, kAnchoredMessageIconSize));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_CLOSE));
  close_button_->SetProperty(views::kMarginsKey,
                             kAnchoreMessageActionIconMarginsInset);
  close_button_->SetProperty(views::kElementIdentifierKey,
                             kAnchoredMessageCloseIconId);

  menu_button_ = AddChildView(std::make_unique<views::MenuButton>(
      base::BindRepeating(&AnchoredMessageBubbleView::MenuButtonPressed,
                          base::Unretained(this))));
  ConfigureInkDropForToolbar(menu_button_);
  menu_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP));
  menu_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kBrowserToolsChromeRefreshIcon,
                                     ui::kColorIcon, kAnchoredMessageIconSize));
  menu_button_->SetProperty(views::kMarginsKey,
                            kAnchoreMessageActionIconMarginsInset);
  menu_button_->SetProperty(views::kElementIdentifierKey,
                            kAnchoredMessageMenuIconId);

  UpdateContent(model);
}

void AnchoredMessageBubbleView::UpdateContent(
    const PageActionModelInterface& model) {
  const bool chip_has_icon = !model.GetAnchoredMessageIcon();
  // Ensure that the anchored message always has a chip showing - that is it has
  // an icon and/or non-empty text.
  CHECK(chip_has_icon || !model.GetText().empty());

  icon_ = model.GetAnchoredMessageIcon();
  if (icon_) {
    icon_view_->SetImage(icon_.value());
    icon_view_->SetVisible(true);
  } else {
    icon_view_->SetVisible(false);
  }

  label_text_ = model.GetAnchoredMessageText();
  label_->SetText(label_text_);
  label_->SetVisible(!label_text_.empty());

  std::optional<ui::ImageModel> chip_icon =
      chip_has_icon ? std::optional<ui::ImageModel>(model.GetImage())
                    : std::nullopt;
  bool show_chip = chip_icon || !model.GetText().empty();

  chip_container_->Update(model.GetText(), chip_icon);
  chip_container_->SetVisible(show_chip);

  AnchoredMessageActionIconType action_icon_type =
      model.GetAnchoredMessageActionIconType();
  show_close_button_ =
      action_icon_type == AnchoredMessageActionIconType::kClose;
  close_button_->SetVisible(show_close_button_);

  ui::SimpleMenuModel* const menu_model = model.GetAnchoredMessageMenuModel();
  if (menu_model_ != menu_model) {
    if (menu_runner_ && menu_runner_->IsRunning()) {
      menu_runner_->Cancel();
    }
    menu_runner_ = nullptr;
    menu_model_ = menu_model;
  }
  bool show_menu_button =
      action_icon_type == AnchoredMessageActionIconType::kMenu && menu_model_;
  menu_button_->SetVisible(show_menu_button);

  // Update margins dynamically to avoid excessive spacing when some components
  // are hidden.
  bool add_padding_before_chip =
      icon_view_->GetVisible() || label_->GetVisible();
  chip_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, add_padding_before_chip ? kAnchoredMessageSpaceLeftOfChip : 0, 0,
          0));

  OnThemeChanged();
}

void AnchoredMessageBubbleView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const ui::ColorProvider* color_provider = GetColorProvider();

  if (!color_provider) {
    return;
  }

  label_->SetEnabledColor(color_provider->GetColor(ui::kColorSysOnSurface));

  if (close_button_) {
    close_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            vector_icons::kCloseChromeRefreshIcon,
            color_provider->GetColor(ui::kColorSysOnSurfaceVariant),
            kAnchoredMessageIconSize));
  }
  if (menu_button_) {
    menu_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            ::kBrowserToolsChromeRefreshIcon,
            color_provider->GetColor(ui::kColorSysOnSurfaceVariant),
            kAnchoredMessageIconSize));
  }
}

bool AnchoredMessageBubbleView::CanActivate() const {
  return false;  // This widget should not take focus
}

views::View* AnchoredMessageBubbleView::GetContentsView() {
  return this;
}

views::Widget* AnchoredMessageBubbleView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* AnchoredMessageBubbleView::GetWidget() const {
  return View::GetWidget();
}

AnchoredMessageBubbleView::~AnchoredMessageBubbleView() {
  SetAnchorView(nullptr);
}

void AnchoredMessageBubbleView::MenuButtonPressed() {
  if (!menu_model_) {
    return;
  }

  pressed_lock_ = menu_button_->button_controller()->TakeLock();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_, views::MenuRunner::NO_FLAGS,
      base::BindRepeating(&AnchoredMessageBubbleView::OnMenuClosed,
                          base::Unretained(this)));
  menu_runner_->RunMenuAt(
      GetWidget(), nullptr, menu_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
  if (menu_runner_->IsRunning()) {
    delegate_->PauseAnchoredMessageTimeout();
  } else {
    pressed_lock_.reset();
  }
}

void AnchoredMessageBubbleView::OnMenuClosed() {
  pressed_lock_.reset();
  delegate_->ResumeAnchoredMessageTimeout();
}

void AnchoredMessageBubbleView::OnWidgetDestroying(views::Widget* widget) {
  if (menu_runner_ && menu_runner_->IsRunning()) {
    menu_runner_->Cancel();
  }
  menu_runner_ = nullptr;
  BubbleDialogDelegate::OnWidgetDestroying(widget);
}

BEGIN_METADATA(AnchoredMessageBubbleView)
END_METADATA

}  // namespace page_actions
