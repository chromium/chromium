// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/anchored_message_view.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/page_action/multi_icon_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {

const int kChipContainerHeight = 36;
const int kChipIconSize = 20;
const int kAnchoredMessageHeight = 52;
const gfx::Insets kAnchoredMessageMarginsInset = gfx::Insets::TLBR(8, 16, 8, 9);
const gfx::Insets kAnchoredMessageIconMarginsInset =
    gfx::Insets::TLBR(0, 0, 0, 12);
const int kAnchoredMessageIconSize = 18;
const int kAnchoredMessageSpaceLeftOfChip = 16;
const gfx::Insets kAnchoreMessageActionIconMarginsInset =
    gfx::Insets::TLBR(0, 8, 0, 0);

// ChipContainerView holds the clickable chip of the anchored message, similar
// to the Suggestion Chip version of the Page Action View.
class ChipContainerView : public views::MdTextButton {
  METADATA_HEADER(ChipContainerView, views::MdTextButton)
 public:
  ChipContainerView(const std::u16string& label_text,
                    const std::optional<ui::ImageModel>& icon,
                    const raw_ref<AnchoredMessageBubbleView::Delegate> delegate,
                    const std::u16string& accessible_name)
      : views::MdTextButton(
            base::BindRepeating(
                &AnchoredMessageBubbleView::Delegate::AnchoredMessageChipClick,
                base::Unretained(&delegate.get())),
            label_text),
        delegate_(delegate) {
    // We set max height, since otherwise MdTextButton forces 10px vertical
    // padding, and we need 8px.
    SetMaxSize(gfx::Size(0, kChipContainerHeight));
    SetRequestFocusOnPress(false);
    SetStyle(ui::ButtonStyle::kProminent);
    SetLabelStyle(views::style::STYLE_BODY_3_MEDIUM);

    Update(label_text, icon, accessible_name);
  }

  ~ChipContainerView() override = default;

 public:
  void Update(const std::u16string& label_text,
              const std::optional<ui::ImageModel>& icon,
              const std::u16string& accessible_name) {
    SetText(label_text);
    if (icon && !icon->IsEmpty()) {
      if (icon->IsVectorIcon()) {
        SetImageModel(views::Button::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          *icon->GetVectorIcon().vector_icon(),
                          ui::kColorSysOnPrimary, kChipIconSize));
      } else {
        SetImageModel(views::Button::STATE_NORMAL, icon.value());
      }
    } else {
      SetImageModel(views::Button::STATE_NORMAL, std::nullopt);
    }
    SetAccessibleName(accessible_name,
                      accessible_name.empty()
                          ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                          : ax::mojom::NameFrom::kAttribute);
  }

 private:
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
                                      kAnchoredMessageCloseIconId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageMenuIconId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageExpandButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AnchoredMessageBubbleView,
                                      kAnchoredMessageExpandedContentId);

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
  set_close_on_deactivate(false);
  SetProperty(views::kElementIdentifierKey, kAnchoredMessageBubbleId);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetBackgroundColor(ui::kColorSysSurface);
  set_corner_radius(kAnchoredMessageHeight / 2);
  set_margins(kAnchoredMessageMarginsInset);

  auto* animating_layout =
      SetLayoutManager(std::make_unique<views::AnimatingLayoutManager>());
  animating_layout
      ->SetBoundsAnimationMode(
          views::AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis)
      .SetOrientation(views::LayoutOrientation::kVertical);

  auto* layout = animating_layout->SetTargetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  top_row_ = AddChildView(std::make_unique<views::View>());
  auto* top_row_layout =
      top_row_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  top_row_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  top_row_layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  top_row_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  icon_view_ = top_row_->AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetProperty(views::kMarginsKey, kAnchoredMessageIconMarginsInset);
  icon_view_->SetImageSize(
      gfx::Size(kAnchoredMessageIconSize, kAnchoredMessageIconSize));
  icon_view_->SetProperty(views::kElementIdentifierKey, kAnchoredMessageIconId);

  label_ = top_row_->AddChildView(std::make_unique<views::Label>());
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  label_->SetMultiLine(false);
  label_->SetTextStyle(views::style::STYLE_BODY_2_MEDIUM);
  label_->SetProperty(views::kElementIdentifierKey, kAnchoredMessageLabelId);

  expand_button_ = top_row_->AddChildView(std::make_unique<MultiIconButton>(
      base::BindRepeating(&AnchoredMessageBubbleView::OnExpandButtonPressed,
                          base::Unretained(this))));
  expand_button_->SetProperty(views::kElementIdentifierKey,
                              kAnchoredMessageExpandButtonId);
  expand_button_->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, 8, 0, 0));
  expand_button_->SetVisible(false);

  chip_container_ = top_row_->AddChildView(std::make_unique<ChipContainerView>(
      std::u16string(), std::nullopt, delegate_, model.GetAccessibleName()));
  chip_container_->SetProperty(views::kElementIdentifierKey,
                               kAnchoredMessageChipId);

  close_button_ = top_row_->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &AnchoredMessageBubbleView::Delegate::CloseAnchoredMessage,
          base::Unretained(delegate_))));
  close_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  close_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled()
              ? vector_icons::kCloseSmallIcon
              : vector_icons::kCloseChromeRefreshOldIcon,
          ui::kColorIcon, kAnchoredMessageIconSize));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_CLOSE));
  close_button_->SetProperty(views::kMarginsKey,
                             kAnchoreMessageActionIconMarginsInset);
  close_button_->SetProperty(views::kElementIdentifierKey,
                             kAnchoredMessageCloseIconId);

  menu_button_ = top_row_->AddChildView(std::make_unique<views::MenuButton>(
      base::BindRepeating(&AnchoredMessageBubbleView::MenuButtonPressed,
                          base::Unretained(this))));
  menu_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  ConfigureInkDrop(menu_button_);
  menu_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ANCHORED_MESSAGE_MENU_TOOLTIP));
  menu_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kMoreVertIcon
                                         : kBrowserToolsChromeRefreshOldIcon,
                                     ui::kColorIcon, kAnchoredMessageIconSize));
  menu_button_->SetProperty(views::kMarginsKey,
                            kAnchoreMessageActionIconMarginsInset);
  menu_button_->SetProperty(views::kElementIdentifierKey,
                            kAnchoredMessageMenuIconId);

  bottom_container_ = AddChildView(std::make_unique<views::View>());
  bottom_container_->SetProperty(views::kElementIdentifierKey,
                                 kAnchoredMessageExpandedContentId);
  bottom_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 8));
  bottom_container_->SetVisible(false);

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

  chip_container_->Update(model.GetText(), chip_icon,
                          model.GetAccessibleName());
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
  bool add_padding_before_chip = icon_view_->GetVisible() ||
                                 label_->GetVisible() ||
                                 expand_button_->GetVisible();
  chip_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, add_padding_before_chip ? kAnchoredMessageSpaceLeftOfChip : 0, 0,
          0));

  const auto& expandable_content = model.GetAnchoredMessageExpandableContent();
  if (expandable_content) {
    std::vector<std::reference_wrapper<const ui::ImageModel>> icons;
    for (const auto& item : expandable_content->items) {
      if (item.icon) {
        icons.emplace_back(*item.icon);
      }
    }
    expand_button_->Update(icons);
    expand_button_->SetVisible(true);

    bottom_container_->RemoveAllChildViews();

    auto* separator =
        bottom_container_->AddChildView(std::make_unique<views::Separator>());
    separator->SetProperty(views::kMarginsKey, gfx::Insets::VH(8, 0));

    if (expandable_content->heading) {
      auto* title_label = bottom_container_->AddChildView(
          std::make_unique<views::Label>(*expandable_content->heading));
      title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      title_label->SetTextStyle(views::style::STYLE_BODY_4_MEDIUM);
      title_label->SetEnabledColor(ui::kColorSysOnSurface);
      title_label->SetElideBehavior(gfx::ELIDE_TAIL);
      // Set width to 0 so the text will fill available space, but not stretch
      // the bubble.
      title_label->SetPreferredSize(
          gfx::Size(0, title_label->GetPreferredSize().height()));
    }

    for (const auto& item : expandable_content->items) {
      auto* item_row =
          bottom_container_->AddChildView(std::make_unique<views::View>());
      auto* item_layout =
          item_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
      item_layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);

      if (item.icon) {
        auto* item_icon =
            item_row->AddChildView(std::make_unique<views::ImageView>());
        item_icon->SetImage(item.icon.value());
        item_icon->SetImageSize(
            gfx::Size(kAnchoredMessageIconSize, kAnchoredMessageIconSize));
      }

      auto* item_label =
          item_row->AddChildView(std::make_unique<views::Label>(item.text));
      item_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      item_label->SetTextStyle(views::style::STYLE_BODY_4);
      item_label->SetEnabledColor(ui::kColorSysOnSurface);
      item_label->SetMultiLine(false);
      item_label->SetElideBehavior(gfx::ELIDE_TAIL);
      // Set width to 0 so the text will fill available space, but not stretch
      // the bubble.
      item_label->SetPreferredSize(
          gfx::Size(0, item_label->GetPreferredSize().height()));
      item_layout->SetFlexForView(item_label, 1);
    }
  } else {
    expand_button_->SetVisible(false);
    bottom_container_->RemoveAllChildViews();
    bottom_container_->SetVisible(false);
  }

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
            features::IsRoundedIconsEnabled()
                ? vector_icons::kCloseSmallIcon
                : vector_icons::kCloseChromeRefreshOldIcon,
            color_provider->GetColor(ui::kColorSysOnSurfaceVariant),
            kAnchoredMessageIconSize));
  }
  if (menu_button_) {
    menu_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            ::features::IsRoundedIconsEnabled()
                ? kMoreVertIcon
                : kBrowserToolsChromeRefreshOldIcon,
            color_provider->GetColor(ui::kColorSysOnSurfaceVariant),
            kAnchoredMessageIconSize));
  }
}

bool AnchoredMessageBubbleView::CanActivate() const {
  return true;  // Needed for the widget buttons to work on Windows
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

void AnchoredMessageBubbleView::OnExpandButtonPressed() {
  expanded_ = !expanded_;
  bottom_container_->SetVisible(expanded_);
  SizeToContents();
  if (expanded_) {
    delegate_->AnchoredMessageExpanded();
  } else {
    delegate_->AnchoredMessageCollapsed();
  }
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
    delegate_->AnchoredMessageExpanded();
  } else {
    pressed_lock_.reset();
  }
}

void AnchoredMessageBubbleView::OnMenuClosed() {
  pressed_lock_.reset();
  delegate_->AnchoredMessageCollapsed();
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
