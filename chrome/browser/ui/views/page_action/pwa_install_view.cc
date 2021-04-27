// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "chrome/browser/ui/user_education/feature_promo_text_replacements.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {

const base::Feature kInstallIconExperiment{"InstallIconExperiment",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

enum class ExperimentIcon { kDownloadToDevice, kDownload };

constexpr base::FeatureParam<ExperimentIcon>::Option kIconParamOptions[] = {
    {ExperimentIcon::kDownloadToDevice, "downloadToDevice"},
    {ExperimentIcon::kDownload, "download"}};

constexpr base::FeatureParam<ExperimentIcon> kInstallIconParam{
    &kInstallIconExperiment, "installIcon", ExperimentIcon::kDownloadToDevice,
    &kIconParamOptions};

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
                         page_action_icon_delegate),
      browser_(browser) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_PWA_INSTALL_ICON_LABEL));
  SetUpForInOutAnimation();
  browser_->tab_strip_model()->AddObserver(this);
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
  if (active_tab_changed || web_content_replaced) {
    FeaturePromoControllerViews* controller =
        FeaturePromoControllerViews::GetForView(this);
    controller->CloseBubble(feature_engagement::kIPHDesktopPwaInstallFeature);
  }
}

void PwaInstallView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  if (web_contents->IsCrashed()) {
    SetVisible(false);
    return;
  }

  auto* manager = webapps::AppBannerManager::FromWebContents(web_contents);
  // May not be present e.g. in incognito mode.
  if (!manager)
    return;

  bool is_probably_promotable = manager->IsProbablyPromotableWebApp();
  if (is_probably_promotable && manager->MaybeConsumeInstallAnimation())
    AnimateIn(base::nullopt);
  else
    ResetSlideAnimation(false);

  SetVisible(is_probably_promotable || PWAConfirmationBubbleView::IsShowing());

  // Only try to show IPH when |PwaInstallView.IsDrawn|. This catches the case
  // that view is set to visible but not drawn in fullscreen mode.
  if (is_probably_promotable && ShouldShowIph(web_contents, manager) &&
      IsDrawn()) {
    FeaturePromoControllerViews* controller =
        FeaturePromoControllerViews::GetForView(this);
    if (controller) {
      // Reset the iph flag when it's shown again.
      install_icon_clicked_after_iph_shown_ = false;
      bool iph_shown = controller->MaybeShowPromoWithTextReplacements(
          feature_engagement::kIPHDesktopPwaInstallFeature,
          FeaturePromoTextReplacements::WithString(
              webapps::AppBannerManager::GetInstallableWebAppName(
                  web_contents)),
          base::BindOnce(&PwaInstallView::OnIphClosed,
                         weak_ptr_factory_.GetWeakPtr()));
      if (iph_shown)
        SetHighlighted(true);
    }
  }
}

void PwaInstallView::OnIphClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // IPH is also closed when the install button is clicked. This does not
  // count as an 'ignore'. The button should remain highlighted and will
  // eventually be un-highlighted when PWAConfirmationBubbleView is closed.
  if (install_icon_clicked_after_iph_shown_)
    return;
  SetHighlighted(false);
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  auto* manager = webapps::AppBannerManager::FromWebContents(web_contents);
  if (!manager)
    return;
  auto start_url = manager->GetManifestStartUrl();
  if (start_url.is_empty())
    return;
  PrefService* prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();
  base::UmaHistogramEnumeration("WebApp.InstallIphPromo.Result",
                                web_app::InstallIphResult::kIgnored);
  web_app::RecordInstallIphIgnored(
      prefs, web_app::GenerateAppIdFromURL(start_url), base::Time::Now());
}

void PwaInstallView::OnExecuting(PageActionIconView::ExecuteSource source) {
  base::RecordAction(base::UserMetricsAction("PWAInstallIcon"));

  // Close PWA install IPH if it is showing.
  FeaturePromoControllerViews* controller =
      FeaturePromoControllerViews::GetForView(this);
  chrome::PwaInProductHelpState iph_state =
      chrome::PwaInProductHelpState::kNotShown;
  if (controller) {
    install_icon_clicked_after_iph_shown_ = controller->BubbleIsShowing(
        feature_engagement::kIPHDesktopPwaInstallFeature);
    if (install_icon_clicked_after_iph_shown_)
      iph_state = chrome::PwaInProductHelpState::kShown;

    controller->CloseBubble(feature_engagement::kIPHDesktopPwaInstallFeature);
  }

  web_app::CreateWebAppFromManifest(
      GetWebContents(),
      /*bypass_service_worker_check=*/false,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, base::DoNothing(),
      iph_state);
}

views::BubbleDialogDelegate* PwaInstallView::GetBubble() const {
  // TODO(https://907351): Implement.
  return nullptr;
}

const gfx::VectorIcon& PwaInstallView::GetVectorIcon() const {
  if (base::FeatureList::IsEnabled(kInstallIconExperiment)) {
    ExperimentIcon icon = kInstallIconParam.Get();
    switch (icon) {
      case ExperimentIcon::kDownloadToDevice:
        return omnibox::kInstallDesktopIcon;
      case ExperimentIcon::kDownload:
        return omnibox::kInstallDownloadIcon;
    }
  }
  return omnibox::kPlusIcon;
}

std::u16string PwaInstallView::GetTextForTooltipAndAccessibleName() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return std::u16string();
  return l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents));
}

bool PwaInstallView::ShouldShowIph(content::WebContents* web_contents,
                                   webapps::AppBannerManager* manager) {
  auto start_url = manager->GetManifestStartUrl();
  if (start_url.is_empty())
    return false;

  web_app::AppId app_id = web_app::GenerateAppIdFromURL(start_url);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto score = site_engagement::SiteEngagementService::Get(profile)->GetScore(
      web_contents->GetURL());
  return score > kIphSiteEngagementThresholdParam.Get() &&
         web_app::ShouldShowIph(profile->GetPrefs(), app_id);
}

BEGIN_METADATA(PwaInstallView, PageActionIconView)
END_METADATA
