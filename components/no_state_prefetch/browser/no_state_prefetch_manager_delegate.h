// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_MANAGER_DELEGATE_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_MANAGER_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents_delegate.h"
#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"
#include "url/gurl.h"

namespace content_settings {
class CookieSettings;
}

namespace prerender {

// NoStatePrefetchManagerDelegate allows content embedders to override
// NoStatePrefetchManager logic.
class NoStatePrefetchManagerDelegate {
 public:
  NoStatePrefetchManagerDelegate();
  virtual ~NoStatePrefetchManagerDelegate() = default;

  // Checks whether third party cookies should be blocked.
  virtual scoped_refptr<content_settings::CookieSettings>
  GetCookieSettings() = 0;

  // Perform preconnect, if feasible.
  virtual void MaybePreconnect(const GURL& url);

  // Get the prerender contents delegate.
  virtual std::unique_ptr<NoStatePrefetchContentsDelegate>
  GetNoStatePrefetchContentsDelegate() = 0;

  // Check whether the user has enabled predictive loading of web pages.
  virtual bool IsNetworkPredictionPreferenceEnabled();

  // Gets the reason why predictive loading of web pages was disabld.
  virtual std::string GetReasonForDisablingPrediction();
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_MANAGER_DELEGATE_H_
