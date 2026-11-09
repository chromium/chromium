// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_contents/chrome_web_contents_view_delegate_android.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"
#include "components/search_engines/template_url_service.h"
#include "components/web_distiller/core/url_constants.h"
#include "components/web_distiller/core/web_distiller_features.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/drop_data.h"
#include "ui/base/device_form_factor.h"
#include "ui/gfx/animation/animation.h"

namespace {

bool IsSrpNavigation(TemplateURLService* template_url_service,
                     const GURL& url,
                     const GURL& last_committed_url) {
  if (!template_url_service) {
    return false;
  }

  return template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
             url) ||
         template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
             last_committed_url);
}

bool IsReaderModeNavigation(const GURL& url, const GURL& last_committed_url) {
  return url.SchemeIs(web_distiller::kWebDistillerScheme) ||
         last_committed_url.SchemeIs(web_distiller::kWebDistillerScheme);
}

}  // namespace

ChromeWebContentsViewDelegateAndroid::ChromeWebContentsViewDelegateAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ChromeWebContentsViewDelegateAndroid::~ChromeWebContentsViewDelegateAndroid() =
    default;

content::WebDragDestDelegate*
ChromeWebContentsViewDelegateAndroid::GetDragDestDelegate() {
  // GetDragDestDelegate is a pure virtual method from WebContentsViewDelegate
  // and must have an implementation although android doesn't use it.
  NOTREACHED();
}

bool ChromeWebContentsViewDelegateAndroid::ShouldShowBlurTransitionAnimation(
    content::NavigationHandle* navigation_handle) {
  if (gfx::Animation::PrefersReducedMotion()) {
    return false;
  }
  // Only show animation for phone form factor.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return false;
  }
  const GURL& url = navigation_handle->GetURL();
  const GURL& last_committed_url = web_contents_->GetLastCommittedURL();
  if (base::FeatureList::IsEnabled(
          features::kAndroidNavigationBlurTransitionAnimation)) {
    static const base::FeatureParam<bool> kSkipSrp{
        &features::kAndroidNavigationBlurTransitionAnimation, "skip_srp",
        false};
    if (!kSkipSrp.Get()) {
      return true;
    }
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    if (!IsSrpNavigation(template_url_service, url, last_committed_url)) {
      return true;
    }

    // Skip SRP is true, and the navigation involves SRP. Check if we want to
    // show Reading Mode navigation.
    if (IsReaderModeNavigation(url, last_committed_url) &&
        base::FeatureList::IsEnabled(
            web_distiller::kReaderModeBlurTransitionAnimation)) {
      return true;
    }
    return false;
  }

  if (base::FeatureList::IsEnabled(
          web_distiller::kReaderModeBlurTransitionAnimation)) {
    return IsReaderModeNavigation(url, last_committed_url);
  }

  return false;
}

void ChromeWebContentsViewDelegateAndroid::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // TODO(dtrainor, kouhei): Give WebView a Populator/delegate so it can use
  // the same context menu code.
  ContextMenuHelper* helper = ContextMenuHelper::FromWebContents(web_contents_);
  if (helper) {
    helper->ShowContextMenu(render_frame_host, params);
  }
}

void ChromeWebContentsViewDelegateAndroid::DismissContextMenu() {
  // ContextMenuHelper is a WebContentsUserData, so it will be the same obj used
  // in #ShowContextMenu().
  ContextMenuHelper* helper = ContextMenuHelper::FromWebContents(web_contents_);
  if (helper) {
    helper->DismissContextMenu();
  }
}

void ChromeWebContentsViewDelegateAndroid::OnPerformingDrop(
    const content::DropData& drop_data,
    DropCompletionCallback callback) {
  HandleOnPerformingDrop(web_contents_, drop_data, std::move(callback));
}

std::unique_ptr<content::WebContentsViewDelegate> CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<ChromeWebContentsViewDelegateAndroid>(web_contents);
}
