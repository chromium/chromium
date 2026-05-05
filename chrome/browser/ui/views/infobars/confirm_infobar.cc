// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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
  auto* layout = content_container()->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Create the label and set the eliding behaviour.
  label_ = AddContentChildView(CreateLabel(delegate_ptr->GetMessageText()));
  label_->SetElideBehavior(delegate_ptr->GetMessageElideBehavior());

  // Set properties on the created label_ for flex layout.
  int kHorizontalDistanceLabel = ChromeLayoutProvider::Get()->GetDistanceMetric(
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
    button_ptr->SetCustomPadding(
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_INFOBAR_BUTTON_VERTICAL_PADDING),
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_INFOBAR_BUTTON_HORIZONTAL_PADDING)));

    button_ptr->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_TOAST_CONTROL_VERTICAL),
                        0));

    const bool is_default_button =
        type == buttons || type == ConfirmInfoBarDelegate::BUTTON_OK;
    const auto fallback_style = is_default_button ? ui::ButtonStyle::kProminent
                                                  : ui::ButtonStyle::kTonal;
    button_ptr->SetStyle(
        delegate_ptr->GetButtonStyle(type).value_or(fallback_style));

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

    ok_button_->SetProperty(views::kFlexBehaviorKey, kRigidFlex);
    // Set the margin for FlexLayout for ok button.
    ok_button_->SetProperty(
        views::kMarginsKey,
        std::make_unique<gfx::Insets>(gfx::Insets::TLBR(0, 0, 0, 0)));
  }

  if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_ = create_button(ConfirmInfoBarDelegate::BUTTON_CANCEL,
                                   &ConfirmInfoBar::CancelButtonPressed);
    cancel_button_->SetProperty(views::kElementIdentifierKey,
                                kCancelButtonElementId);

    cancel_button_->SetProperty(views::kFlexBehaviorKey, kRigidFlex);
    // Set the margin for FlexLayout for cancel button.
    cancel_button_->SetProperty(
        views::kMarginsKey, std::make_unique<gfx::Insets>(gfx::Insets::TLBR(
                                0,
                                ChromeLayoutProvider::Get()->GetDistanceMetric(
                                    views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                                0, 0)));
  }
  auto link_unique_ptr = CreateLink(delegate_ptr->GetLinkText(),
                                    delegate_ptr->GetLinkAccessibleText());

  if (!link_unique_ptr) {
    return;
  }

  link_ = link_unique_ptr.get();

  // Add the link to the infobar view and reorder it to be placed before the
  // close button.
  if (GetDelegate()->ShouldShowLinkBeforeButton()) {
    AddContentChildView(std::move(link_unique_ptr));
  } else {
    AddViewBeforeCloseButton(std::move(link_unique_ptr));
  }

  link_->SetProperty(views::kFlexBehaviorKey, kRigidFlex);
  // Add margins for spacing for flex layout.
  link_->SetProperty(views::kMarginsKey, std::make_unique<gfx::Insets>(
                                             gfx::Insets::TLBR(0, 0, 0, 0)));
}
ConfirmInfoBar::~ConfirmInfoBar() = default;

void ConfirmInfoBar::Layout(PassKey) {
  LayoutSuperclass<InfoBarView>(this);
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
  // With using flex layout, no manual calculations are needed.
  return 0;
}

int ConfirmInfoBar::GetContentPreferredWidth() const {
  // With using flex layout, no manual calculations are needed.
  return 0;
}

BEGIN_METADATA(ConfirmInfoBar)
END_METADATA
