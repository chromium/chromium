// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

PWAConfirmationBubbleView* g_bubble_ = nullptr;

bool g_auto_accept_pwa_for_testing = false;

// Returns an ImageView containing the app icon.
std::unique_ptr<views::ImageView> CreateIconView(
    const WebAppInstallInfo& web_app_info) {
  constexpr int kIconSize = 48;
  gfx::ImageSkia image(std::make_unique<WebAppInfoImageSource>(
                           kIconSize, web_app_info.icon_bitmaps.any),
                       gfx::Size(kIconSize, kIconSize));

  auto icon_image_view = std::make_unique<views::ImageView>();
  icon_image_view->SetImage(image);
  return icon_image_view;
}

}  // namespace

// static
bool PWAConfirmationBubbleView::IsShowing() {
  return g_bubble_;
}

// static
PWAConfirmationBubbleView* PWAConfirmationBubbleView::GetBubble() {
  return g_bubble_;
}

PWAConfirmationBubbleView::PWAConfirmationBubbleView(
    views::View* anchor_view,
    PageActionIconView* highlight_icon_button,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    chrome::AppInstallationAcceptanceCallback callback,
    chrome::PwaInProductHelpState iph_state,
    PrefService* prefs,
    feature_engagement::Tracker* tracker)
    : LocationBarBubbleDelegateView(anchor_view, nullptr),
      highlight_icon_button_(highlight_icon_button),
      web_app_info_(std::move(web_app_info)),
      callback_(std::move(callback)),
      iph_state_(iph_state),
      prefs_(prefs),
      tracker_(tracker) {
  DCHECK(web_app_info_);
  DCHECK(prefs_);

  WidgetDelegate::SetShowCloseButton(true);
  WidgetDelegate::SetTitle(
      l10n_util::GetStringUTF16(IDS_INSTALL_TO_OS_LAUNCH_SURFACE_BUBBLE_TITLE));

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_INSTALL_PWA_BUTTON_LABEL));
  base::TrimWhitespace(web_app_info_->title, base::TRIM_ALL,
                       &web_app_info_->title);
  // PWAs should always be configured not to open in a browser tab.
  if (web_app_info_->user_display_mode.has_value()) {
    DCHECK_NE(*web_app_info_->user_display_mode,
              web_app::mojom::UserDisplayMode::kBrowser);
  }

  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // Use CONTROL insets, because the icon is non-text (see documentation for
  // DialogContentType).
  gfx::Insets margin_insets = layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);
  set_margins(margin_insets);

  int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      icon_label_spacing));

  AddChildView(CreateIconView(*web_app_info_).release());

  views::View* labels = new views::View();
  AddChildView(labels);
  labels->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  labels->AddChildView(
      web_app::CreateNameLabel(web_app_info_->title).release());
  labels->AddChildView(web_app::CreateOriginLabel(
                           url::Origin::Create(web_app_info_->start_url), false)
                           .release());

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip) &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings)) {
    // This UI is only for prototyping and is not intended for shipping.
    DCHECK_EQ(features::kDesktopPWAsTabStripSettings.default_state,
              base::FEATURE_DISABLED_BY_DEFAULT);
    tabbed_window_checkbox_ = labels->AddChildView(
        std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
            IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TABBED_WINDOW)));
    tabbed_window_checkbox_->SetChecked(
        web_app_info_->user_display_mode ==
        web_app::mojom::UserDisplayMode::kTabbed);
  }

  SetHighlightedButton(highlight_icon_button_);
}

PWAConfirmationBubbleView::~PWAConfirmationBubbleView() = default;

bool PWAConfirmationBubbleView::OnCloseRequested(
    views::Widget::ClosedReason close_reason) {
  base::UmaHistogramEnumeration("WebApp.InstallConfirmation.CloseReason",
                                close_reason);
  return LocationBarBubbleDelegateView::OnCloseRequested(close_reason);
}

views::View* PWAConfirmationBubbleView::GetInitiallyFocusedView() {
  return nullptr;
}

void PWAConfirmationBubbleView::WindowClosing() {
  DCHECK_EQ(g_bubble_, this);
  g_bubble_ = nullptr;

  if (highlight_icon_button_)
    highlight_icon_button_->Update();

  // If |web_app_info_| is populated, then the bubble was not accepted.
  if (web_app_info_) {
    base::RecordAction(base::UserMetricsAction("WebAppInstallCancelled"));

    if (iph_state_ == chrome::PwaInProductHelpState::kShown) {
      web_app::AppId app_id = web_app::GenerateAppId(web_app_info_->manifest_id,
                                                     web_app_info_->start_url);
      web_app::RecordInstallIphIgnored(prefs_, app_id, base::Time::Now());
    }
  } else {
    base::RecordAction(base::UserMetricsAction("WebAppInstallAccepted"));
  }

  if (callback_) {
    DCHECK(web_app_info_);
    std::move(callback_).Run(false, std::move(web_app_info_));
  }
}

bool PWAConfirmationBubbleView::Accept() {
  DCHECK(web_app_info_);
  web_app_info_->user_display_mode =
      tabbed_window_checkbox_ && tabbed_window_checkbox_->GetChecked()
          ? web_app::mojom::UserDisplayMode::kTabbed
          : web_app::mojom::UserDisplayMode::kStandalone;

  if (iph_state_ == chrome::PwaInProductHelpState::kShown) {
    web_app::AppId app_id = web_app::GenerateAppId(web_app_info_->manifest_id,
                                                   web_app_info_->start_url);
    web_app::RecordInstallIphInstalled(prefs_, app_id);
    tracker_->NotifyEvent(feature_engagement::events::kDesktopPwaInstalled);
  }
  std::move(callback_).Run(true, std::move(web_app_info_));
  return true;
}

void PWAConfirmationBubbleView::OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const {
  params->name = "PWAConfirmationBubbleView";
}

namespace chrome {

void ShowPWAInstallBubble(content::WebContents* web_contents,
                          std::unique_ptr<WebAppInstallInfo> web_app_info,
                          AppInstallationAcceptanceCallback callback,
                          PwaInProductHelpState iph_state) {
  if (g_bubble_)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAnchorView(
          PageActionIconType::kPwaInstall);
  PageActionIconView* icon =
      browser_view->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kPwaInstall);
  auto* browser_context = web_contents->GetBrowserContext();
  PrefService* prefs = Profile::FromBrowserContext(browser_context)->GetPrefs();

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);
  g_bubble_ = new PWAConfirmationBubbleView(
      anchor_view, icon, std::move(web_app_info), std::move(callback),
      iph_state, prefs, tracker);

  views::BubbleDialogDelegateView::CreateBubble(g_bubble_)->Show();
  base::RecordAction(base::UserMetricsAction("WebAppInstallShown"));

  if (g_auto_accept_pwa_for_testing)
    g_bubble_->AcceptDialog();

  if (icon) {
    icon->Update();
    DCHECK(icon->GetVisible());
  }
}

void SetAutoAcceptPWAInstallConfirmationForTesting(bool auto_accept) {
  g_auto_accept_pwa_for_testing = auto_accept;
}

}  // namespace chrome
