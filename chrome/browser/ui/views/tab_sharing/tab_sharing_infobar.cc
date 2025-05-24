// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_infobar.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/constants.h"
#include "media/capture/capture_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr auto kCapturedSurfaceControlIndicatorButtonInsets =
    gfx::Insets::VH(4, 8);

url::Origin GetOriginFromId(content::GlobalRenderFrameHostId rfh_id) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(rfh_id);
  if (!rfh) {
    return {};
  }

  return rfh->GetLastCommittedOrigin();
}

}  // namespace

TabSharingInfoBar::TabSharingInfoBar(
    std::unique_ptr<TabSharingInfoBarDelegate> delegate,
    content::GlobalRenderFrameHostId shared_tab_id,
    content::GlobalRenderFrameHostId capturer_id,
    const std::u16string& shared_tab_name,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type)
    : InfoBarView(std::move(delegate)) {
  auto* delegate_ptr = GetDelegate();

  status_message_view_ = AddChildView(
      CreateStatusMessageView(shared_tab_id, capturer_id, shared_tab_name,
                              capturer_name, role, capture_type));

  const int buttons = delegate_ptr->GetButtons();
  const auto create_button =
      [&](TabSharingInfoBarDelegate::TabSharingInfoBarButton type,
          void (TabSharingInfoBar::*click_function)(),
          int button_context = views::style::CONTEXT_BUTTON_MD) {
        const bool use_text_color_for_icon =
            type != TabSharingInfoBarDelegate::kCapturedSurfaceControlIndicator;
        auto* button = AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(click_function, base::Unretained(this)),
            delegate_ptr->GetButtonLabel(type), button_context,
            use_text_color_for_icon));
        button->SetProperty(
            views::kMarginsKey,
            gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                                DISTANCE_TOAST_CONTROL_VERTICAL),
                            0));

        const bool is_default_button =
            type == buttons || type == TabSharingInfoBarDelegate::kStop;
        button->SetStyle(is_default_button ? ui::ButtonStyle::kProminent
                                           : ui::ButtonStyle::kTonal);
        button->SetImageModel(views::Button::STATE_NORMAL,
                              delegate_ptr->GetButtonImage(type));
        button->SetEnabled(delegate_ptr->IsButtonEnabled(type));
        button->SetTooltipText(delegate_ptr->GetButtonTooltip(type));
        return button;
      };

  if (buttons & TabSharingInfoBarDelegate::kStop) {
    stop_button_ = create_button(TabSharingInfoBarDelegate::kStop,
                                 &TabSharingInfoBar::StopButtonPressed);
  }

  if (buttons & TabSharingInfoBarDelegate::kShareThisTabInstead) {
    share_this_tab_instead_button_ =
        create_button(TabSharingInfoBarDelegate::kShareThisTabInstead,
                      &TabSharingInfoBar::ShareThisTabInsteadButtonPressed);
  }

  if (buttons & TabSharingInfoBarDelegate::kQuickNav &&
      !base::FeatureList::IsEnabled(features::kTabCaptureInfobarLinks)) {
    quick_nav_button_ =
        create_button(TabSharingInfoBarDelegate::kQuickNav,
                      &TabSharingInfoBar::QuickNavButtonPressed);
  }

  if (buttons & TabSharingInfoBarDelegate::kCapturedSurfaceControlIndicator) {
    csc_indicator_button_ = create_button(
        TabSharingInfoBarDelegate::kCapturedSurfaceControlIndicator,
        &TabSharingInfoBar::OnCapturedSurfaceControlActivityIndicatorPressed,
        CONTEXT_OMNIBOX_PRIMARY);
    csc_indicator_button_->SetStyle(ui::ButtonStyle::kDefault);
    csc_indicator_button_->SetCornerRadius(
        GetLayoutConstant(TOOLBAR_CORNER_RADIUS));
    csc_indicator_button_->SetCustomPadding(
        kCapturedSurfaceControlIndicatorButtonInsets);
    csc_indicator_button_->SetTextColor(
        views::Button::ButtonState::STATE_NORMAL, ui::kColorSysOnSurface);
  }

  // TODO(crbug.com/378107817): It seems like link_ isn't always needed, but
  // it's added regardless. See about only adding when necessary.
  link_ = AddChildView(CreateLink(delegate_ptr->GetLinkText()));
}

TabSharingInfoBar::~TabSharingInfoBar() = default;

void TabSharingInfoBar::Layout(PassKey) {
  LayoutSuperclass<InfoBarView>(this);

  if (stop_button_) {
    stop_button_->SizeToPreferredSize();
  }

  if (share_this_tab_instead_button_) {
    share_this_tab_instead_button_->SizeToPreferredSize();
  }

  if (quick_nav_button_) {
    quick_nav_button_->SizeToPreferredSize();
  }

  if (csc_indicator_button_) {
    csc_indicator_button_->SizeToPreferredSize();
  }

  int x = GetStartX();
  Views views;
  views.push_back(status_message_view_.get());
  views.push_back(link_.get());
  AssignWidths(&views, std::max(0, GetEndX() - x - NonLabelWidth()));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  status_message_view_->SetPosition(
      gfx::Point(x, OffsetY(status_message_view_)));
  x = status_message_view_->bounds().right() +
      layout_provider->GetDistanceMetric(
          DISTANCE_INFOBAR_HORIZONTAL_ICON_LABEL_PADDING);

  // Add buttons into a vector to be displayed in an ordered row.
  // Depending on the PlatformStyle, reverse the vector so the stop button will
  // be on the correct leading style.
  std::vector<views::MdTextButton*> order_of_buttons;
  if (stop_button_) {
    order_of_buttons.push_back(stop_button_);
  }
  if (share_this_tab_instead_button_) {
    order_of_buttons.push_back(share_this_tab_instead_button_);
  }
  if (quick_nav_button_) {
    order_of_buttons.push_back(quick_nav_button_);
  }
  if (csc_indicator_button_) {
    order_of_buttons.push_back(csc_indicator_button_);
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

  link_->SetPosition(gfx::Point(GetEndX() - link_->width(), OffsetY(link_)));
}

void TabSharingInfoBar::StopButtonPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  RecordUma(TabSharingInfoBarInteraction::kStopButtonClicked);
  GetDelegate()->Stop();
}

void TabSharingInfoBar::ShareThisTabInsteadButtonPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  RecordUma(TabSharingInfoBarInteraction::kShareThisTabInsteadButtonClicked);
  GetDelegate()->ShareThisTabInstead();
}

void TabSharingInfoBar::QuickNavButtonPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  GetDelegate()->QuickNav();
}

void TabSharingInfoBar::OnCapturedSurfaceControlActivityIndicatorPressed() {
  if (!owner()) {
    return;  // We're closing; don't call anything, it might access the owner.
  }
  GetDelegate()->OnCapturedSurfaceControlActivityIndicatorPressed();
}

std::unique_ptr<views::View> TabSharingInfoBar::CreateStatusMessageView(
    content::GlobalRenderFrameHostId shared_tab_id,
    content::GlobalRenderFrameHostId capturer_id,
    const std::u16string& shared_tab_name,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) const {
  TabSharingStatusMessageView::EndpointInfo shared_tab_info(
      shared_tab_name,
      TabSharingStatusMessageView::EndpointInfo::TargetType::kCapturedTab,
      shared_tab_id);
  TabSharingStatusMessageView::EndpointInfo capturer_info(
      capturer_name,
      TabSharingStatusMessageView::EndpointInfo::TargetType::kCapturingTab,
      capturer_id);
  if (base::FeatureList::IsEnabled(features::kTabCaptureInfobarLinks) &&
      GetOriginFromId(capturer_id).scheme() != extensions::kExtensionScheme) {
    return TabSharingStatusMessageView::Create(capturer_id, shared_tab_info,
                                               capturer_info, capturer_name,
                                               role, capture_type);
  } else {
    return CreateStatusMessageLabel(shared_tab_info, capturer_info,
                                    capturer_name, role, capture_type);
  }
}

std::unique_ptr<views::Label> TabSharingInfoBar::CreateStatusMessageLabel(
    const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
    const TabSharingStatusMessageView::EndpointInfo& capturer_info,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) const {
  std::unique_ptr<views::Label> label =
      CreateLabel(TabSharingStatusMessageView::GetMessageText(
          shared_tab_info, capturer_info, capturer_name, role, capture_type));
  label->SetElideBehavior(gfx::ELIDE_TAIL);
  return label;
}

TabSharingInfoBarDelegate* TabSharingInfoBar::GetDelegate() {
  return static_cast<TabSharingInfoBarDelegate*>(delegate());
}

int TabSharingInfoBar::GetContentMinimumWidth() const {
  return status_message_view_->GetMinimumSize().width() +
         link_->GetMinimumSize().width() + NonLabelWidth();
}

int TabSharingInfoBar::NonLabelWidth() const {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  const int label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int button_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  const int button_count =
      (stop_button_ ? 1 : 0) + (share_this_tab_instead_button_ ? 1 : 0) +
      (quick_nav_button_ ? 1 : 0) + (csc_indicator_button_ ? 1 : 0);

  int width = (button_count == 0) ? 0 : label_spacing;

  width += std::max(0, button_spacing * (button_count - 1));

  width += stop_button_ ? stop_button_->width() : 0;
  width += share_this_tab_instead_button_
               ? share_this_tab_instead_button_->width()
               : 0;
  width += quick_nav_button_ ? quick_nav_button_->width() : 0;
  width += csc_indicator_button_ ? csc_indicator_button_->width() : 0;

  return width + ((width && !link_->GetText().empty()) ? label_spacing : 0);
}

std::unique_ptr<infobars::InfoBar> CreateTabSharingInfoBar(
    std::unique_ptr<TabSharingInfoBarDelegate> delegate,
    content::GlobalRenderFrameHostId shared_tab_id,
    content::GlobalRenderFrameHostId capturer_id,
    const std::u16string& shared_tab_name,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) {
  return std::make_unique<TabSharingInfoBar>(std::move(delegate), shared_tab_id,
                                             capturer_id, shared_tab_name,
                                             capturer_name, role, capture_type);
}
