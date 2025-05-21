// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/pwa_install_page_action.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/grit/generated_resources.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Site engagement score threshold to show In-Product Help.
// Add x_ prefix so the IPH feature engagement tracker can ignore this.
const base::FeatureParam<int> kIphSiteEngagementThresholdParam{
    &feature_engagement::kIPHDesktopPwaInstallFeature,
    "x_site_engagement_threshold",
    web_app::kIphFieldTrialParamDefaultSiteEngagementThreshold};

}  // namespace

PwaInstallPageActionController::PwaInstallPageActionController(
    tabs::TabInterface& tab_interface)
    : page_actions::PageActionObserver(kActionInstallPwa),
      tab_interface_(tab_interface),
      iph_is_enabled_(base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopPwaInstallFeature)) {
  content::WebContents* web_contents = tab_interface_->GetContents();
  if (web_contents) {
    Observe(web_contents);
    manager_ = webapps::AppBannerManager::FromWebContents(web_contents);
    // May not be present e.g. in incognito mode.
    if (manager_) {
      manager_->AddObserver(this);
    }
  }

  will_discard_contents_subscription_ =
      tab_interface_->RegisterWillDiscardContents(base::BindRepeating(
          &PwaInstallPageActionController::WillDiscardContents,
          base::Unretained(this)));
  will_deactivate_subscription_ = tab_interface_->RegisterWillDeactivate(
      base::BindRepeating(&PwaInstallPageActionController::WillDeactivate,
                          base::Unretained(this)));

  RegisterAsPageActionObserver(GetPageActionController());
}

void PwaInstallPageActionController::SetIsExecuting(bool is_executing) {
  is_executing_ = is_executing;
}

void PwaInstallPageActionController::WillDiscardContents(
    tabs::TabInterface* tab_interface,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (manager_) {
    manager_->RemoveObserver(this);
  }
  if (new_contents) {
    manager_ = webapps::AppBannerManager::FromWebContents(new_contents);
    if (manager_) {
      manager_->AddObserver(this);
    }
  }
  Observe(new_contents);
}

void PwaInstallPageActionController::WillDeactivate(
    tabs::TabInterface* tab_interface) {
  UpdateVisibility();
}

void PwaInstallPageActionController::OnPageActionIconShown(
    const page_actions::PageActionState& page_action) {
  if (!iph_is_enabled_) {
    return;
  }
  // Only try to show IPH when |PwaInstallView.IsDrawn|. This catches the case
  // that view is set to visible but not drawn in fullscreen mode.
  content::WebContents* web_contents = tab_interface_->GetContents();
  if (!web_contents) {
    return;
  }
  std::optional<webapps::WebAppBannerData> data =
      manager_->GetCurrentWebAppBannerData();
  if (!data.has_value()) {
    return;
  }
  if (!ShouldShowIph(web_contents, *data)) {
    return;
  }

  BrowserWindowInterface* browser_window =
      tab_interface_->GetBrowserWindowInterface();
  if (!browser_window) {
    return;
  }
  BrowserUserEducationInterface* user_education =
      browser_window->GetUserEducationInterface();
  if (!user_education) {
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHDesktopPwaInstallFeature);
  params.close_callback =
      base::BindOnce(&PwaInstallPageActionController::OnIphClosed,
                     weak_ptr_factory_.GetWeakPtr(), data.value().manifest_id);
  params.body_params =
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents);
  params.show_promo_result_callback =
      base::BindOnce(&PwaInstallPageActionController::OnIphShown,
                     weak_ptr_factory_.GetWeakPtr());
  user_education->MaybeShowFeaturePromo(std::move(params));
  iph_pending_ = true;
}

PwaInstallPageActionController::~PwaInstallPageActionController() {
  if (manager_) {
    manager_->RemoveObserver(this);
  }
  Observe(nullptr);
}

void PwaInstallPageActionController::UpdateVisibility() {
  content::WebContents* web_contents = tab_interface_->GetContents();
  if (!web_contents) {
    return;
  }

  if (web_contents->IsCrashed()) {
    Hide();
    return;
  }

  if (!manager_) {
    return;
  }

  if (manager_->IsProbablyPromotableWebApp()) {
    Show(web_contents, manager_->MaybeConsumeInstallAnimation());
  } else {
    Hide();
  }
}

void PwaInstallPageActionController::Show(content::WebContents* web_contents,
                                          bool showChip) {
  // Controller responsible for all page actions
  page_actions::PageActionController& all_actions_controller =
      GetPageActionController();
  all_actions_controller.OverrideText(
      kActionInstallPwa,
      l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
          webapps::AppBannerManager::GetInstallableWebAppName(web_contents)));
  all_actions_controller.OverrideTooltip(
      kActionInstallPwa,
      l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
          webapps::AppBannerManager::GetInstallableWebAppName(web_contents)));
  all_actions_controller.Show(kActionInstallPwa);
  if (showChip) {
    all_actions_controller.ShowSuggestionChip(kActionInstallPwa);
  } else {
    all_actions_controller.HideSuggestionChip(kActionInstallPwa);
  }
}

void PwaInstallPageActionController::Hide() {
  // Controller responsible for all page actions
  page_actions::PageActionController& all_actions_controller =
      GetPageActionController();
  all_actions_controller.HideSuggestionChip(kActionInstallPwa);
  all_actions_controller.Hide(kActionInstallPwa);
  all_actions_controller.ClearOverrideText(kActionInstallPwa);
  all_actions_controller.ClearOverrideTooltip(kActionInstallPwa);
}

void PwaInstallPageActionController::OnInstallableWebAppStatusUpdated(
    webapps::InstallableWebAppCheckResult result,
    const std::optional<webapps::WebAppBannerData>& data) {
  UpdateVisibility();
}

void PwaInstallPageActionController::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  UpdateVisibility();
}

page_actions::PageActionController&
PwaInstallPageActionController::GetPageActionController() {
  page_actions::PageActionController* all_actions_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(all_actions_controller);
  return *all_actions_controller;
}

bool PwaInstallPageActionController::ShouldShowIph(
    content::WebContents* web_contents,
    const webapps::WebAppBannerData& data) {
  if (!iph_is_enabled_) {
    return false;
  }
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

void PwaInstallPageActionController::OnIphShown(
    user_education::FeaturePromoResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  iph_pending_ = false;
}

void PwaInstallPageActionController::OnIphClosed(
    const webapps::ManifestId manifest_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // IPH is also closed when the install button is clicked and the action is
  // executed. This does not count as an 'ignore'. The button should remain
  // highlighted and will eventually be un-highlighted when the PWA install
  // bubble is closed.
  if (is_executing_) {
    return;
  }
  content::WebContents* web_contents = tab_interface_->GetContents();
  if (!web_contents) {
    return;
  }

  PrefService* prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();

  web_app::WebAppPrefGuardrails::GetForDesktopInstallIph(prefs).RecordIgnore(
      web_app::GenerateAppIdFromManifestId(manifest_id), base::Time::Now());
}
