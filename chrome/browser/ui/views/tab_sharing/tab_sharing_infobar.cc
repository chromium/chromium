// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_infobar.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

TabSharingInfoBar::TabSharingInfoBar(
    std::unique_ptr<TabSharingInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  auto* delegate_ptr = GetDelegate();
  label_ = AddChildView(CreateLabel(delegate_ptr->GetMessageText()));
  label_->SetElideBehavior(gfx::ELIDE_TAIL);

  const auto buttons = delegate_ptr->GetButtons();
  const auto create_button = [&](TabSharingInfoBarDelegate::InfoBarButton type,
                                 void (TabSharingInfoBar::*click_function)()) {
    auto* button = AddChildView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(click_function, base::Unretained(this)),
        delegate_ptr->GetButtonLabel(type)));
    button->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_TOAST_CONTROL_VERTICAL),
                        0));

    const bool is_default_button =
        type == buttons || type == TabSharingInfoBarDelegate::BUTTON_OK;
    button->SetStyle(is_default_button ? ui::ButtonStyle::kProminent
                                       : ui::ButtonStyle::kTonal);
    button->SetImageModel(views::Button::STATE_NORMAL,
                          delegate_ptr->GetButtonImage(type));
    button->SetEnabled(delegate_ptr->GetButtonEnabled(type));
    button->SetTooltipText(delegate_ptr->GetButtonTooltip(type));
    return button;
  };

  if (buttons & TabSharingInfoBarDelegate::BUTTON_OK) {
    ok_button_ = create_button(TabSharingInfoBarDelegate::BUTTON_OK,
                               &TabSharingInfoBar::OkButtonPressed);
  }

  if (buttons & TabSharingInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_ = create_button(TabSharingInfoBarDelegate::BUTTON_CANCEL,
                                   &TabSharingInfoBar::CancelButtonPressed);
  }

  if (buttons & TabSharingInfoBarDelegate::BUTTON_EXTRA) {
    extra_button_ = create_button(TabSharingInfoBarDelegate::BUTTON_EXTRA,
                                  &TabSharingInfoBar::ExtraButtonPressed);
  }

  // TODO(josephjoopark): It seems like link_ isn't always needed, but it's
  // added regardless. See about only adding when necessary.
  link_ = AddChildView(CreateLink(delegate_ptr->GetLinkText()));
}

TabSharingInfoBar::~TabSharingInfoBar() = default;

void TabSharingInfoBar::Layout(PassKey) {
  LayoutSuperclass<InfoBarView>(this);

  if (ok_button_) {
    ok_button_->SizeToPreferredSize();
  }

  if (cancel_button_) {
    cancel_button_->SizeToPreferredSize();
  }

  if (extra_button_) {
    extra_button_->SizeToPreferredSize();
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
  if (extra_button_) {
    order_of_buttons.push_back(extra_button_);
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

void TabSharingInfoBar::OkButtonPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  if (GetDelegate()->Accept()) {
    RemoveSelf();
  }
}

void TabSharingInfoBar::CancelButtonPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  if (GetDelegate()->Cancel()) {
    RemoveSelf();
  }
}

void TabSharingInfoBar::ExtraButtonPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  if (GetDelegate()->ExtraButtonPressed()) {
    RemoveSelf();
  }
}

TabSharingInfoBarDelegate* TabSharingInfoBar::GetDelegate() {
  return static_cast<TabSharingInfoBarDelegate*>(delegate());
}

int TabSharingInfoBar::GetContentMinimumWidth() const {
  return label_->GetMinimumSize().width() + link_->GetMinimumSize().width() +
         NonLabelWidth();
}

int TabSharingInfoBar::NonLabelWidth() const {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  const int label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int button_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  const int button_count =
      (ok_button_ ? 1 : 0) + (cancel_button_ ? 1 : 0) + (extra_button_ ? 1 : 0);

  int width =
      (label_->GetText().empty() || button_count == 0) ? 0 : label_spacing;

  width += std::max(0, button_spacing * (button_count - 1));

  width += ok_button_ ? ok_button_->width() : 0;
  width += cancel_button_ ? cancel_button_->width() : 0;
  width += extra_button_ ? extra_button_->width() : 0;

  return width + ((width && !link_->GetText().empty()) ? label_spacing : 0);
}

std::unique_ptr<infobars::InfoBar> CreateTabSharingInfoBar(
    std::unique_ptr<TabSharingInfoBarDelegate> delegate) {
  return std::make_unique<TabSharingInfoBar>(std::move(delegate));
}
