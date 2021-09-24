// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_ACCURACY_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_ACCURACY_TIPS_ACCURACY_WEB_CONTENTS_OBSERVER_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace accuracy_tips {

// Observes navigations and triggers a warning if a visited site is determined
// if a visited site is determined to be news-related.
class AccuracyWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AccuracyWebContentsObserver> {
 public:
  static bool IsEnabled(content::WebContents* web_contents);

  ~AccuracyWebContentsObserver() override;

  AccuracyWebContentsObserver(const AccuracyWebContentsObserver&) = delete;
  AccuracyWebContentsObserver& operator=(const AccuracyWebContentsObserver&) =
      delete;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Callback handler for accuracy result from AccuracyService.
  void OnAccuracyStatusObtained(const GURL& url, AccuracyTipStatus result);

  friend class content::WebContentsUserData<AccuracyWebContentsObserver>;

  AccuracyWebContentsObserver(content::WebContents* web_contents,
                              AccuracyService* accuracy_service);

  raw_ptr<AccuracyService> accuracy_service_;

  base::WeakPtrFactory<AccuracyWebContentsObserver> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace accuracy_tips
#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_WEB_CONTENTS_OBSERVER_H_
