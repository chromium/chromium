// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it handles conditional
// includes
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

// Site engagement score threshold to show In-Product Help.
// Add x_ prefix so the IPH feature engagement tracker can ignore this.
constexpr base::FeatureParam<int> kIphSiteEngagementThresholdParam{
    &feature_engagement::kIPHDesktopPwaInstallFeature,
    "x_site_engagement_threshold",
    web_app::kIphFieldTrialParamDefaultSiteEngagementThreshold};

}  // namespace

PwaInstallView::PwaInstallView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Browser* browser)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "PWAInstall"),
      browser_(browser) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_PWA_INSTALL_ICON_LABEL));
  SetUpForInOutAnimation();
  browser_->tab_strip_model()->AddObserver(this);
  SetProperty(views::kElementIdentifierKey, kInstallPwaElementId);
}

PwaInstallView::~PwaInstallView() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void PwaInstallView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // If the active tab changed, or the content::WebContents in the
  // active tab was replaced, close IPH
  bool active_tab_changed = selection.active_tab_changed();
  bool web_content_replaced =
      change.type() == TabStripModelChange::Type::kReplaced;
  if ((active_tab_changed || web_content_replaced)) {
    browser_->window()->AbortFeaturePromo(
        feature_engagement::kIPHDesktopPwaInstallFeature);
  }
}

void PwaInstallView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  if (web_contents->IsCrashed()) {
    SetVisible(false);
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }

  GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents)));

  auto* manager = webapps::AppBannerManager::FromWebContents(web_contents);

  // May not be present e.g. in incognito mode.
  if (!manager) {
    return;
  }

  // This currently relies on this method being called synchronously from
  // BrowserView::OnInstallableWebAppStatusUpdated, which is called
  // synchronously from the AppBannerManager when installability changes.
  // Ideally this data is passed through the observer, but because views code
  // has to be isolated here, it's difficult to pass an argument along. The
  // right way to 'clean this up' is unclear, but for now it is safe.
  bool is_probably_promotable = manager->IsProbablyPromotableWebApp();
  if (is_probably_promotable && manager->MaybeConsumeInstallAnimation()) {
    AnimateIn(std::nullopt);
  } else {
    ResetSlideAnimation(false);
  }

  // TODO(crbug.com/341254289): Cleanup after Universal Install has launched to
  // 100% on Stable.
  SetVisible(is_probably_promotable || PWAConfirmationBubbleView::IsShowing());

  // See above about safety of this call.
  std::optional<webapps::WebAppBannerData> data =
      manager->GetCurrentWebAppBannerData();

  // Only try to show IPH when |PwaInstallView.IsDrawn|. This catches the case
  // that view is set to visible but not drawn in fullscreen mode.
  if (data && is_probably_promotable && ShouldShowIph(web_contents, *data) &&
      IsDrawn() &&
      base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopPwaInstallFeature)) {
    user_education::FeaturePromoParams params(
        feature_engagement::kIPHDesktopPwaInstallFeature);
    params.close_callback = base::BindOnce(
        &PwaInstallView::OnIphClosed, weak_ptr_factory_.GetWeakPtr(), *data);
    params.body_params =
        webapps::AppBannerManager::GetInstallableWebAppName(web_contents);
    params.show_promo_result_callback = base::BindOnce(
        &PwaInstallView::OnIphShown, weak_ptr_factory_.GetWeakPtr());
    browser_->window()->MaybeShowFeaturePromo(std::move(params));
    iph_pending_ = true;
  }
}

void PwaInstallView::OnIphShown(user_education::FeaturePromoResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  iph_pending_ = false;
  // Reset the IPH flag when it's shown.
  if (result) {
    install_icon_clicked_after_iph_shown_ = false;
    SetHighlighted(true);
  }
}

void PwaInstallView::OnIphClosed(const webapps::WebAppBannerData& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // IPH is also closed when the install button is clicked. This does not
  // count as an 'ignore'. The button should remain highlighted and will
  // eventually be un-highlighted when PWAConfirmationBubbleView is closed.
  if (install_icon_clicked_after_iph_shown_) {
    return;
  }
  SetHighlighted(false);
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  PrefService* prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();

  web_app::WebAppPrefGuardrails::GetForDesktopInstallIph(prefs).RecordIgnore(
      web_app::GenerateAppIdFromManifestId(data.manifest_id),
      base::Time::Now());
}

void PwaInstallView::OnExecuting(PageActionIconView::ExecuteSource source) {
  base::RecordAction(base::UserMetricsAction("PWAInstallIcon"));

  // Close PWA install IPH if it is showing.
  web_app::PwaInProductHelpState iph_state =
      web_app::PwaInProductHelpState::kNotShown;
  install_icon_clicked_after_iph_shown_ =
      browser_->window()->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHDesktopPwaInstallFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  if (install_icon_clicked_after_iph_shown_) {
    iph_state = web_app::PwaInProductHelpState::kShown;
  }

#if BUILDFLAG(IS_CHROMEOS)
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::AppDiscovery_Browser_OmniboxInstallIconClicked().SetIPHShown(
          install_icon_clicked_after_iph_shown_));
#endif

  web_app::CreateWebAppFromManifest(
      GetWebContents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, base::DoNothing(),
      iph_state);
}

views::BubbleDialogDelegate* PwaInstallView::GetBubble() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return nullptr;
  }

  // TODO(crbug.com/341254289): Cleanup after Universal Install has launched to
  // 100% on Stable.
  auto* bubble = PWAConfirmationBubbleView::GetBubble();
  // Only return the active bubble if it's anchored to `this`. (This check takes
  // the more generic approach of verifying that it's the same widget as to
  // avoid depending too heavily on the exact details of how anchoring works.)
  if (bubble && bubble->GetAnchorView() &&
      (bubble->GetAnchorView()->GetWidget() == GetWidget())) {
    return bubble;
  }

  return nullptr;
}

const gfx::VectorIcon& PwaInstallView::GetVectorIcon() const {
  return kInstallDesktopChromeRefreshIcon;
}

bool PwaInstallView::ShouldShowIph(content::WebContents* web_contents,
                                   const webapps::WebAppBannerData& data) {
  if (iph_pending_) {
    return false;
  }
  if (blink::IsEmptyManifest(data.manifest()) || !data.manifest_id.is_valid()) {
    return false;
  }
  webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(data.manifest_id);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto score = site_engagement::SiteEngagementService::Get(profile)->GetScore(
      web_contents->GetVisibleURL());

  return score > kIphSiteEngagementThresholdParam.Get() &&
         !web_app::WebAppPrefGuardrails::GetForDesktopInstallIph(
              profile->GetPrefs())
              .IsBlockedByGuardrails(app_id);
}

BEGIN_METADATA(PwaInstallView)
END_METADATA
