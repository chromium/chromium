// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_web_contents_observer.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/page_visibility_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

namespace accuracy_tips {

// static
bool AccuracyWebContentsObserver::IsEnabled(
    content::WebContents* web_contents) {
  return base::FeatureList::IsEnabled(safe_browsing::kAccuracyTipsFeature) &&
         !web_contents->GetBrowserContext()->IsOffTheRecord();
}

AccuracyWebContentsObserver::~AccuracyWebContentsObserver() = default;

AccuracyWebContentsObserver::AccuracyWebContentsObserver(
    content::WebContents* web_contents,
    AccuracyService* accuracy_service)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<AccuracyWebContentsObserver>(*web_contents),
      accuracy_service_(accuracy_service) {
  DCHECK(!web_contents->GetBrowserContext()->IsOffTheRecord());
  DCHECK(accuracy_service);
}

void AccuracyWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  if (!navigation->IsInPrimaryMainFrame() || navigation->IsSameDocument() ||
      !navigation->HasCommitted() || navigation->IsErrorPage()) {
    return;
  }

  if (web_contents()->GetPrimaryMainFrame()->GetVisibilityState() !=
      content::PageVisibilityState::kVisible) {
    return;
  }

  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  accuracy_service_->CheckAccuracyStatus(
      url,
      base::BindOnce(&AccuracyWebContentsObserver::OnAccuracyStatusObtained,
                     weak_factory_.GetWeakPtr(), url));
}

void AccuracyWebContentsObserver::OnAccuracyStatusObtained(
    const GURL& url,
    AccuracyTipStatus result) {
  // We are not on this site any more, so the result is invalid.
  if (url != web_contents()->GetLastCommittedURL())
    return;

  // Don't show tip on insecure pages. This can't be checked in the
  // AccuracyService because it requires a WebContents.
  if (result == AccuracyTipStatus::kShowAccuracyTip &&
      !accuracy_service_->IsSecureConnection(web_contents())) {
    result = AccuracyTipStatus::kNotSecure;
  }

  UMA_HISTOGRAM_ENUMERATION("Privacy.AccuracyTip.PageStatus", result);
  ukm::builders::AccuracyTipStatus(
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId())
      .SetStatus(static_cast<int>(result))
      .Record(ukm::UkmRecorder::Get());

  if (result != AccuracyTipStatus::kShowAccuracyTip)
    return;

  accuracy_service_->MaybeShowAccuracyTip(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AccuracyWebContentsObserver);
}  // namespace accuracy_tips
