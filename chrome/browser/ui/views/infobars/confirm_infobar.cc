// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ConfirmInfoBar, kOkButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ConfirmInfoBar, kCancelButtonElementId);

ConfirmInfoBar::ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  SetProperty(views::kElementIdentifierKey, kInfoBarElementId);
  auto* delegate_ptr = GetDelegate();

  // TODO: (457800852) Create hierarchy of views for Infobar.
  // Set the layout on the content container for flex layout.
  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    auto* layout = content_container()->SetLayoutManager(
        std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  }

  // Create the label and set the eliding behaviour.
  label_ = AddContentChildView(CreateLabel(delegate_ptr->GetMessageText()));
  label_->SetElideBehavior(delegate_ptr->GetMessageElideBehavior());

  // Set properties on the created label_ for flex layout.
  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    int kHorizontalDistanceLabel =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
    if (GetDelegate()->ShouldShowLinkBeforeButton()) {
      kHorizontalDistanceLabel = 4;
    }
    // Set horizontal distance for label for flex layout.
    label_->SetProperty(views::kMarginsKey,
                        std::make_unique<gfx::Insets>(gfx::Insets::TLBR(
                            0, 0, 0, kHorizontalDistanceLabel)));
    label_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred)
            .WithWeight(1));
  }

  const views::FlexSpecification kRigidFlex =
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred);

  // Create both the ok and cancel buttons.
  const int buttons = delegate_ptr->GetButtons();
  const auto create_button = [&](ConfirmInfoBarDelegate::InfoBarButton type,
                                 void (ConfirmInfoBar::*click_function)()) {
    auto button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(click_function, base::Unretained(this)),
        GetDelegate()->GetButtonLabel(type));
    auto* button_ptr = button.get();
    // Set custom padding on the buttons.
    if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
      button_ptr->SetCustomPadding(
          gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_INFOBAR_BUTTON_VERTICAL_PADDING),
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_INFOBAR_BUTTON_HORIZONTAL_PADDING)));
    }

    button_ptr->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_TOAST_CONTROL_VERTICAL),
                        0));

    const bool is_default_button =
        type == buttons || type == ConfirmInfoBarDelegate::BUTTON_OK;
    button_ptr->SetStyle(is_default_button ? ui::ButtonStyle::kProminent
                                           : ui::ButtonStyle::kTonal);
    button_ptr->SetImageModel(views::Button::STATE_NORMAL,
                              delegate_ptr->GetButtonImage(type));
    button_ptr->SetEnabled(delegate_ptr->GetButtonEnabled(type));
    button_ptr->SetTooltipText(delegate_ptr->GetButtonTooltip(type));

    AddContentChildView(std::move(button));
    return button_ptr;
  };

  if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
    ok_button_ = create_button(ConfirmInfoBarDelegate::BUTTON_OK,
                               &ConfirmInfoBar::OkButtonPressed);
    ok_button_->SetProperty(views::kElementIdentifierKey, kOkButtonElementId);

    if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
      ok_button_->SetProperty(views::kFlexBehaviorKey, kRigidFlex);
      // Set the margin for FlexLayout for ok button.
      ok_button_->SetProperty(
          views::kMarginsKey,
          std::make_unique<gfx::Insets>(gfx::Insets::TLBR(0, 0, 0, 0)));
    }
  }

  if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_ = create_button(ConfirmInfoBarDelegate::BUTTON_CANCEL,
                                   &ConfirmInfoBar::CancelButtonPressed);
    cancel_button_->SetProperty(views::kElementIdentifierKey,
                                kCancelButtonElementId);

    if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
      cancel_button_->SetProperty(views::kFlexBehaviorKey, kRigidFlex);
      // Set the margin for FlexLayout for cancel button.
      cancel_button_->SetProperty(
          views::kMarginsKey,
          std::make_unique<gfx::Insets>(
              gfx::Insets::TLBR(0,
                                ChromeLayoutProvider::Get()->GetDistanceMetric(
                                    views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                                0, 0)));
    }
  }
  auto link_unique_ptr = CreateLink(delegate_ptr->GetLinkText(),
                                    delegate_ptr->GetLinkAccessibleText());

  if (!link_unique_ptr) {
    return;
  }

  link_ = link_unique_ptr.get();

  // Add the link to the infobar view and reorder it to be placed before the
  // close button.
  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    if (GetDelegate()->ShouldShowLinkBeforeButton()) {
      AddContentChildView(std::move(link_unique_ptr));
    } else {
      AddViewBeforeCloseButton(std::move(link_unique_ptr));
    }

    link_->SetProperty(views::kFlexBehaviorKey, kRigidFlex);
    // Add margins for spacing for flex layout.
    link_->SetProperty(views::kMarginsKey, std::make_unique<gfx::Insets>(
                                               gfx::Insets::TLBR(0, 0, 0, 0)));
  } else {
    AddContentChildView(std::move(link_unique_ptr));
  }
}
ConfirmInfoBar::~ConfirmInfoBar() = default;

void ConfirmInfoBar::Layout(PassKey) {
  if (GetLayoutManager()) {
    // If using flex layout return as we are using layout manager.
    LayoutSuperclass<InfoBarView>(this);
    return;
  }
  LayoutSuperclass<InfoBarView>(this);

  if (ok_button_) {
    ok_button_->SizeToPreferredSize();
  }

  if (cancel_button_) {
    cancel_button_->SizeToPreferredSize();
  }

  int x = GetStartX();
  Views views;
  views.push_back(label_.get());
  views.push_back(link_.get());
  AssignWidths(&views, std::max(0, GetEndX() - x - NonLabelWidth()));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  label_->SetPosition(gfx::Point(x, OffsetY(label_)));
  if (!label_->GetText().empty()) {
    x = label_->bounds().right() +
        layout_provider->GetDistanceMetric(
            DISTANCE_INFOBAR_HORIZONTAL_ICON_LABEL_PADDING);
  }

  if (GetDelegate()->ShouldShowLinkBeforeButton()) {
    const int link_spacing = layout_provider->GetDistanceMetric(
        DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL);
    link_->SetPosition(
        gfx::Point(label_->bounds().right() + link_spacing, OffsetY(link_)));

    if (!link_->GetText().empty()) {
      x = link_->bounds().right() +
          layout_provider->GetDistanceMetric(
              views::DISTANCE_RELATED_LABEL_HORIZONTAL);
    }
  } else {
    link_->SetPosition(gfx::Point(GetEndX() - link_->width(), OffsetY(link_)));
  }

  // Add buttons into a vector to be displayed in an ordered row.
  // Depending on the PlatformStyle, reverse the vector so the ok button will be
  // on the correct leading style.
  std::vector<views::MdTextButton*> order_of_buttons;
  if (ok_button_) {
    order_of_buttons.push_back(ok_button_);
  }
  if (cancel_button_) {
    order_of_buttons.push_back(cancel_button_);
  }

  if constexpr (!views::PlatformStyle::kIsOkButtonLeading) {
    std::ranges::reverse(order_of_buttons);
  }

  for (views::MdTextButton* button : order_of_buttons) {
    button->SetPosition(gfx::Point(x, OffsetY(button)));
    x = button->bounds().right() +
        layout_provider->GetDistanceMetric(
            views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  }
}

void ConfirmInfoBar::OkButtonPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  if (GetDelegate()->Accept()) {
    RemoveSelf();
  }
}

void ConfirmInfoBar::CancelButtonPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  if (GetDelegate()->Cancel()) {
    RemoveSelf();
  }
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

const ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() const {
  return delegate()->AsConfirmInfoBarDelegate();
}

int ConfirmInfoBar::GetContentMinimumWidth() const {
  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    // With using flex layout, no manual calculations are needed.
    return 0;
  }
  return label_->GetMinimumSize().width() + link_->GetMinimumSize().width() +
         NonLabelWidth();
}

int ConfirmInfoBar::GetContentPreferredWidth() const {
  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    // With using flex layout, no manual calculations are needed.
    return 0;
  }
  return label_->GetPreferredSize().width() +
         link_->GetPreferredSize().width() + NonLabelWidth();
}

int ConfirmInfoBar::NonLabelWidth() const {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const bool should_show_link_before_button =
      GetDelegate()->ShouldShowLinkBeforeButton();
  // The link should be shown before the button if the custom layout is
  // enabled. The spacing between the label and the link is different than the
  // spacing between the link and the button.
  const int label_spacing =
      should_show_link_before_button
          ? layout_provider->GetDistanceMetric(
                DISTANCE_INFOBAR_HORIZONTAL_ICON_LABEL_PADDING)
          : layout_provider->GetDistanceMetric(
                views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int button_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  int spacing_from_previous = label_->GetText().empty() ? 0 : label_spacing;
  int width = 0;
  width += ok_button_ ? (std::exchange(spacing_from_previous, button_spacing) +
                         ok_button_->width())
                      : 0;
  width +=
      cancel_button_ ? (spacing_from_previous + cancel_button_->width()) : 0;
  width += (width && !link_->GetText().empty()) ? label_spacing : 0;
  return width;
}

BEGIN_METADATA(ConfirmInfoBar)
END_METADATA
