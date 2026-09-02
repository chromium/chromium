// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/suspicious_site_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/suspicious_site_warnings/suspicious_site_controller_desktop.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Parses description string containing <link>...</link> and attaches link
// callback.
std::unique_ptr<views::StyledLabel> CreateLearnMoreLabel(
    const std::u16string& full_text_with_link,
    base::RepeatingClosure on_link_clicked) {
  constexpr std::u16string_view kOpenTag = u"<link>";
  constexpr std::u16string_view kCloseTag = u"</link>";

  size_t open_pos = full_text_with_link.find(kOpenTag);
  size_t close_pos = full_text_with_link.find(kCloseTag);

  auto label = std::make_unique<views::StyledLabel>();
  if (open_pos == std::u16string::npos || close_pos == std::u16string::npos ||
      close_pos <= open_pos) {
    label->SetText(full_text_with_link);
    return label;
  }

  std::u16string clean_text =
      full_text_with_link.substr(0, open_pos) +
      full_text_with_link.substr(open_pos + kOpenTag.length(),
                                 close_pos - (open_pos + kOpenTag.length())) +
      full_text_with_link.substr(close_pos + kCloseTag.length());
  label->SetText(clean_text);

  const gfx::Range link_range(
      static_cast<uint32_t>(open_pos),
      static_cast<uint32_t>(close_pos - kOpenTag.length()));
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          std::move(on_link_clicked));
  label->AddStyleRange(link_range, link_style);

  return label;
}

}  // namespace

SuspiciousSiteBubbleView::SuspiciousSiteBubbleView(
    views::BubbleAnchor anchor,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    content::WebContents* web_contents)
    : PageInfoBubbleViewBase(anchor,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_SUSPICIOUS_SITE,
                             web_contents) {
  // Keep the bubble open until the user interacts or navigates away.
  set_close_on_deactivate(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  views::BubbleDialogDelegateView::CreateBubble(this);

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int button_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  // Configure title view.
  auto title_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_TITLE),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_HEADLINE_5);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  GetBubbleFrameView()->SetTitleView(std::move(title_label));

  // Configure body layout.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_spacing));

  // Description text with Learn more link.
  description_label_ = AddChildView(CreateLearnMoreLabel(
      l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_MESSAGE),
      base::BindRepeating(&SuspiciousSiteBubbleView::OpenHelpCenter,
                          base::Unretained(this))));
  constexpr int kTargetDescriptionWidth = 335;
  description_label_->SizeToFit(kTargetDescriptionWidth);

  // Bottom action buttons row.
  auto* buttons_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  buttons_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  buttons_row->SetBetweenChildSpacing(button_spacing);
  buttons_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);

  mark_as_safe_button_ =
      buttons_row->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&SuspiciousSiteBubbleView::OnMarkAsSafeClicked,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_PROCEED_BUTTON)));
  mark_as_safe_button_->SetStyle(ui::ButtonStyle::kDefault);

  back_to_safety_button_ =
      buttons_row->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&SuspiciousSiteBubbleView::OnBackToSafetyClicked,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_BACK_TO_SAFETY)));
  back_to_safety_button_->SetStyle(ui::ButtonStyle::kProminent);
}

SuspiciousSiteBubbleView::~SuspiciousSiteBubbleView() {
  if (web_contents()) {
    if (auto* controller =
            safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
                web_contents())) {
      controller->OnBubbleDestroyed();
    }
  }
}

void SuspiciousSiteBubbleView::OnBackToSafetyClicked() {
  if (web_contents()) {
    if (auto* controller =
            safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
                web_contents())) {
      controller->OnBackToSafetyClicked();
      if (GetWidget()) {
        GetWidget()->Close();
      }
      return;
    }
    auto& controller = web_contents()->GetController();
    const GURL& current_url = web_contents()->GetLastCommittedURL();

    // Navigate to the most recent entry belonging to a different domain/host.
    for (int i = controller.GetLastCommittedEntryIndex() - 1; i >= 0; --i) {
      content::NavigationEntry* entry = controller.GetEntryAtIndex(i);
      if (entry && !entry->GetURL().is_empty() &&
          !net::registry_controlled_domains::SameDomainOrHost(
              current_url, entry->GetURL(),
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
        controller.GoToIndex(i);
        if (GetWidget()) {
          GetWidget()->Close();
        }
        return;
      }
    }

    controller.LoadURLWithParams(content::NavigationController::LoadURLParams(
        GURL(chrome::kChromeUINewTabURL)));
  }
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

void SuspiciousSiteBubbleView::OnMarkAsSafeClicked() {
  if (web_contents()) {
    if (auto* controller =
            safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
                web_contents())) {
      controller->OnMarkAsSafeClicked();
      if (GetWidget()) {
        GetWidget()->Close();
      }
      return;
    }
    const GURL& current_url = web_contents()->GetLastCommittedURL();
    if (current_url.is_valid() && !current_url.host().empty()) {
      if (auto* profile = Profile::FromBrowserContext(
              web_contents()->GetBrowserContext())) {
        if (auto* hcsm =
                HostContentSettingsMapFactory::GetForProfile(profile)) {
          safe_browsing::SuspiciousSiteWarningAllowlist(hcsm).AllowSiteForHost(
              std::string(current_url.host()));
        }
      }
    }
  }
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

void SuspiciousSiteBubbleView::OpenHelpCenter() {
  if (web_contents()) {
    if (auto* controller =
            safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
                web_contents())) {
      controller->OnLearnMoreClicked();
      return;
    }
    web_contents()->OpenURL(
        content::OpenURLParams(GURL(chrome::kSafeBrowsingHelpCenterURL),
                               content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false),
        /*navigation_handle_callback=*/{});
  }
}

void ShowSuspiciousSiteBubble(BrowserWindowInterface* browser,
                              content::WebContents* web_contents) {
  if (!browser) {
    return;
  }

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPageInfoAnchorConfiguration(
          browser, bubble_anchor_util::Anchor::kLocationBar);
  gfx::Rect anchor_rect =
      configuration.anchor.IsNull()
          ? bubble_anchor_util::GetPageInfoAnchorRect(browser)
          : gfx::Rect();
  gfx::NativeWindow parent_window = browser->GetWindow()->GetNativeWindow();
  gfx::NativeView parent_view = platform_util::GetViewForWindow(parent_window);

  views::BubbleDialogDelegateView* bubble = new SuspiciousSiteBubbleView(
      configuration.anchor, anchor_rect, parent_view, web_contents);

  if (configuration.highlighted_element) {
    bubble->SetHighlightedElement(*configuration.highlighted_element);
  }
  bubble->SetArrow(configuration.bubble_arrow);
  bubble->GetWidget()->Show();
}

void ShowSuspiciousSiteBubble(content::WebContents* web_contents) {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  ShowSuspiciousSiteBubble(browser, web_contents);
}

namespace safe_browsing {
void ShowSuspiciousSiteBubble(content::WebContents* web_contents) {
  ::ShowSuspiciousSiteBubble(web_contents);
}
}  // namespace safe_browsing

// IN-TEST
PageInfoBubbleViewBase* CreateSuspiciousSiteBubbleForTesting(
    gfx::NativeView parent_view,
    content::WebContents* web_contents) {
  return new SuspiciousSiteBubbleView(views::BubbleAnchor(), gfx::Rect(),
                                      parent_view, web_contents);
}

BEGIN_METADATA(SuspiciousSiteBubbleView)
END_METADATA
