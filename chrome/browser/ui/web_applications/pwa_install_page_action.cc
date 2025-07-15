// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/pwa_install_page_action.h"

#include <string>

#include "base/auto_reset.h"
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
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : page_actions::PageActionObserver(kActionInstallPwa),
      tab_interface_(tab_interface),
      record_ignore_delegate_(this),
      page_action_controller_(page_action_controller),
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

  RegisterAsPageActionObserver(page_action_controller_.get());
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
  // Only try to show IPH when drawn. This catches the case
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
      BrowserUserEducationInterface::From(browser_window);
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
  page_action_controller_->OverrideText(
      kActionInstallPwa,
      l10n_util::GetStringUTF16(IDS_OMNIBOX_PWA_INSTALL_ICON_LABEL));
  page_action_controller_->OverrideAccessibleName(
      kActionInstallPwa,
      l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
          webapps::AppBannerManager::GetInstallableWebAppName(web_contents)));
  page_action_controller_->OverrideTooltip(
      kActionInstallPwa,
      l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
          webapps::AppBannerManager::GetInstallableWebAppName(web_contents)));
  page_action_controller_->Show(kActionInstallPwa);
  if (showChip) {
    page_action_controller_->ShowSuggestionChip(kActionInstallPwa);
  } else {
    page_action_controller_->HideSuggestionChip(kActionInstallPwa);
  }
}

void PwaInstallPageActionController::Hide() {
  // Controller responsible for all page actions
  page_action_controller_->HideSuggestionChip(kActionInstallPwa);
  page_action_controller_->Hide(kActionInstallPwa);
  page_action_controller_->ClearOverrideAccessibleName(kActionInstallPwa);
  page_action_controller_->ClearOverrideText(kActionInstallPwa);
  page_action_controller_->ClearOverrideTooltip(kActionInstallPwa);
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

  // TODO(crbug.com/421837877): The user education system natively supports
  // deciding when IPHs should/shouldn't be shown. In the application code, we
  // should just call for the feature promo to be shown and the user education
  // will decide to show it or not. This function should be simplified
  // to only test logic related to the PWA itself. For example:
  // `score > kIphSiteEngagementThresholdParam` is ok to check but the part that
  // reads the profile prefs should be removed.
  return score > kIphSiteEngagementThresholdParam.Get() &&
         !web_app::WebAppPrefGuardrails::GetForDesktopInstallIph(
              profile->GetPrefs())
              .IsBlockedByGuardrails(app_id);
}

void PwaInstallPageActionController::OnIphShown(
    user_education::FeaturePromoResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  iph_pending_ = false;
  iph_activity_ = page_action_controller_->AddActivity(kActionInstallPwa);
}

void PwaInstallPageActionController::OnIphClosed(
    const webapps::ManifestId manifest_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  iph_activity_.reset();
  // IPH is also closed when the install button is clicked and the action is
  // executed.
  if (is_executing_) {
    return;
  }

  record_ignore_delegate_->RecordIgnore(
      web_app::GenerateAppIdFromManifestId(manifest_id), base::Time::Now());
}

void PwaInstallPageActionController::RecordIgnore(const webapps::AppId& app_id,
                                                  base::Time time) {
  content::WebContents* web_contents = tab_interface_->GetContents();
  if (!web_contents) {
    return;
  }
  PrefService* prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();
  web_app::WebAppPrefGuardrails::GetForDesktopInstallIph(prefs).RecordIgnore(
      app_id, base::Time::Now());
}

void PwaInstallPageActionController::ExecuteOnIphClosedForTesting(
    const webapps::ManifestId manifest_id,
    page_actions::RecordIgnoreDelegate* record_ignore_delegate) {
  base::AutoReset<raw_ptr<page_actions::RecordIgnoreDelegate>> auto_reset(
      &record_ignore_delegate_, record_ignore_delegate);
  PwaInstallPageActionController::OnIphClosed(manifest_id);
}
