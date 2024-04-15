// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ConfirmInfoBar, kOkButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ConfirmInfoBar, kCancelButtonElementId);

ConfirmInfoBar::ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  SetProperty(views::kElementIdentifierKey, kInfoBarElementId);
  auto* delegate_ptr = GetDelegate();
  label_ = AddChildView(CreateLabel(delegate_ptr->GetMessageText()));
  label_->SetElideBehavior(delegate_ptr->GetMessageElideBehavior());

  const int buttons = delegate_ptr->GetButtons();
  const auto create_button = [&](ConfirmInfoBarDelegate::InfoBarButton type,
                                 void (ConfirmInfoBar::*click_function)()) {
    auto* button = AddChildView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(click_function, base::Unretained(this)),
        GetDelegate()->GetButtonLabel(type)));
    button->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_TOAST_CONTROL_VERTICAL),
                        0));

    const bool is_default_button =
        type == buttons || type == ConfirmInfoBarDelegate::BUTTON_OK;
    button->SetStyle(is_default_button ? ui::ButtonStyle::kProminent
                                       : ui::ButtonStyle::kTonal);
    button->SetImageModel(views::Button::STATE_NORMAL,
                          delegate_ptr->GetButtonImage(type));
    button->SetEnabled(delegate_ptr->GetButtonEnabled(type));
    button->SetTooltipText(delegate_ptr->GetButtonTooltip(type));
    return button;
  };

  if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
    ok_button_ = create_button(ConfirmInfoBarDelegate::BUTTON_OK,
                               &ConfirmInfoBar::OkButtonPressed);
    ok_button_->SetProperty(views::kElementIdentifierKey, kOkButtonElementId);
  }

  if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_ = create_button(ConfirmInfoBarDelegate::BUTTON_CANCEL,
                                   &ConfirmInfoBar::CancelButtonPressed);
    cancel_button_->SetProperty(views::kElementIdentifierKey,
                                kCancelButtonElementId);
  }

  // TODO(josephjoopark): It seems like link_ isn't always needed, but it's
  // added regardless. See about only adding when necessary.
  link_ = AddChildView(CreateLink(delegate_ptr->GetLinkText()));
}

ConfirmInfoBar::~ConfirmInfoBar() = default;

void ConfirmInfoBar::Layout(PassKey) {
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

  if (!views::PlatformStyle::kIsOkButtonLeading) {
    base::ranges::reverse(order_of_buttons);
  }

  for (views::MdTextButton* button : order_of_buttons) {
    button->SetPosition(gfx::Point(x, OffsetY(button)));
    x = button->bounds().right() +
        layout_provider->GetDistanceMetric(
            views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  }

  link_->SetPosition(gfx::Point(GetEndX() - link_->width(), OffsetY(link_)));
}

void ConfirmInfoBar::OkButtonPressed() {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  if (GetDelegate()->Accept())
    RemoveSelf();
}

void ConfirmInfoBar::CancelButtonPressed() {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  if (GetDelegate()->Cancel())
    RemoveSelf();
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

int ConfirmInfoBar::GetContentMinimumWidth() const {
  return label_->GetMinimumSize().width() + link_->GetMinimumSize().width() +
         NonLabelWidth();
}

int ConfirmInfoBar::NonLabelWidth() const {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  const int label_spacing = layout_provider->GetDistanceMetric(
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
