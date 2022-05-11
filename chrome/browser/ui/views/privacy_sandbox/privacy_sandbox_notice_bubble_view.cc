// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_notice_bubble_view.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

using DialogAction = PrivacySandboxService::DialogAction;

namespace {

constexpr int kDialogWidth = 448;

void NotifyServiceAboutDialogAction(Profile* profile, DialogAction action) {
  if (auto* service = PrivacySandboxServiceFactory::GetForProfile(profile)) {
    service->DialogActionOccurred(action);
  }
}

AppMenuButton* GetAppMenuButton(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  // The notice bubble is anchored to the browser. If the browser is destroyed,
  // the bubble will be too. Because of that browser should always be present
  // while bubble is active.
  DCHECK(browser_view);
  return browser_view->toolbar_button_provider()->GetAppMenuButton();
}

class PrivacySandboxNoticeBubbleDelegate : public views::BubbleDialogDelegate {
 public:
  explicit PrivacySandboxNoticeBubbleDelegate(
      bubble_anchor_util::AnchorConfiguration configuration,
      Browser* browser)
      : BubbleDialogDelegate(configuration.anchor_view,
                             configuration.bubble_arrow),
        browser_(browser) {
    if (auto* privacy_sandbox_serivce =
            PrivacySandboxServiceFactory::GetForProfile(browser->profile())) {
      privacy_sandbox_serivce->DialogOpenedForBrowser(browser);
    }
    SetCloseCallback(base::BindOnce(
        &PrivacySandboxNoticeBubbleDelegate::OnClose, base::Unretained(this)));
  }

  void OnClose() {
    if (auto* privacy_sandbox_serivce =
            PrivacySandboxServiceFactory::GetForProfile(browser_->profile())) {
      privacy_sandbox_serivce->DialogClosedForBrowser(browser_);
    }
  }

  void WindowClosing() override {
    switch (GetWidget()->closed_reason()) {
      case views::Widget::ClosedReason::kEscKeyPressed:
        NotifyServiceAboutDialogAction(browser_->profile(),
                                       DialogAction::kNoticeDismiss);
        break;
      case views::Widget::ClosedReason::kUnspecified:
        NotifyServiceAboutDialogAction(
            browser_->profile(), DialogAction::kNoticeClosedNoInteraction);
        break;
      default:
        break;
    }
  }

 private:
  Browser* browser_;
};

}  // namespace

// static
void ShowPrivacySandboxNoticeBubble(Browser* browser) {
  auto bubble_delegate = std::make_unique<PrivacySandboxNoticeBubbleDelegate>(
      bubble_anchor_util::GetAppMenuAnchorConfiguration(browser), browser);
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->set_close_on_deactivate(false);
  bubble_delegate->set_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  bubble_delegate->set_fixed_width(
      ChromeLayoutProvider::Get()->GetSnappedDialogWidth(kDialogWidth));

  bubble_delegate->SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_PRIVACY_SANDBOX_DIALOG_NOTICE_ACKNOWLEDGE_BUTTON));
  bubble_delegate->SetAcceptCallback(base::BindOnce(
      [](Browser* browser) {
        NotifyServiceAboutDialogAction(browser->profile(),
                                       DialogAction::kNoticeAcknowledge);
      },
      base::Unretained(browser)));

  bubble_delegate->SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          IDS_PRIVACY_SANDBOX_DIALOG_NOTICE_OPEN_SETTINGS_BUTTON));
  bubble_delegate->SetCancelCallback(base::BindOnce(
      [](Browser* browser) {
        chrome::ShowPrivacySandboxSettings(browser);
        NotifyServiceAboutDialogAction(browser->profile(),
                                       DialogAction::kNoticeOpenSettings);
      },
      base::Unretained(browser)));

  auto& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_PRIVACY_SANDBOX_CONFIRMATION_BANNER),
      *bundle.GetImageSkiaNamed(IDR_PRIVACY_SANDBOX_CONFIRMATION_BANNER_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(bubble_delegate.get())));

  bubble_delegate->SetContentsView(
      std::make_unique<PrivacySandboxNoticeBubbleView>(browser,
                                                       std::move(image)));
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate))->Show();
}

PrivacySandboxNoticeBubbleView::PrivacySandboxNoticeBubbleView(
    Browser* browser,
    std::unique_ptr<views::View> image)
    : browser_(browser) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kHorizontal))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);

  // Add image view, that was created with the reference to bubble delegate to
  // support changing color for the theme.
  AddChildView(std::move(image));

  // Set up the container for the right side. It contains a title and a
  // description.
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_CONTENT_LIST_VERTICAL_SINGLE)));
  container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_LABEL_HORIZONTAL),
                        0, 0));

  // Create the title view.
  auto* title_label = container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_BUBBLE_NOTICE_TITLE),
      views::style::CONTEXT_DIALOG_TITLE));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetFontList(
      views::style::GetFont(views::style::TextContext::CONTEXT_DIALOG_TITLE,
                            views::style::TextStyle::STYLE_PRIMARY)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));

  // Create the description view.
  auto* description_label =
      container->AddChildView(std::make_unique<views::StyledLabel>());
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // The description contains a link that opens settings page. Find the position
  // of the link by inserting text into the placeholder.
  auto intesets_settings_link = l10n_util::GetStringUTF16(
      IDS_PRIVACY_SANDBOX_BUBBLE_NOTICE_DESCRIPTION_ESTIMATES_INTERESTS_LINK);
  size_t offset;
  description_label->SetText(
      l10n_util::GetStringFUTF16(IDS_PRIVACY_SANDBOX_BUBBLE_NOTICE_DESCRIPTION,
                                 intesets_settings_link, &offset));
  gfx::Range range(offset, offset + intesets_settings_link.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PrivacySandboxNoticeBubbleView::OpenAboutAdPersonalizationSettings,
          base::Unretained(this)));
  description_label->AddStyleRange(range, link_style);

  // Observe app menu button to close the bubble when app menu is shown.
  if (auto* app_menu_button = GetAppMenuButton(browser_)) {
    app_menu_button->AddObserver(this);
  }

  NotifyServiceAboutDialogAction(browser_->profile(),
                                 DialogAction::kNoticeShown);
}

PrivacySandboxNoticeBubbleView::~PrivacySandboxNoticeBubbleView() {
  if (auto* app_menu_button = GetAppMenuButton(browser_)) {
    app_menu_button->RemoveObserver(this);
  }
}

void PrivacySandboxNoticeBubbleView::AppMenuShown() {
  NotifyServiceAboutDialogAction(browser_->profile(),
                                 DialogAction::kNoticeDismiss);
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

void PrivacySandboxNoticeBubbleView::OpenAboutAdPersonalizationSettings() {
  chrome::ShowPrivacySandboxLearnMore(browser_);
  NotifyServiceAboutDialogAction(browser_->profile(),
                                 DialogAction::kNoticeLearnMore);
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
}

BEGIN_METADATA(PrivacySandboxNoticeBubbleView, views::View)
END_METADATA
