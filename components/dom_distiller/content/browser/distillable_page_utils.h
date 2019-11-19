// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_

#include <ostream>

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "base/optional.h"

namespace content {
class WebContents;
}  // namespace content

namespace dom_distiller {

class DistillablePageDetector;

// Uses the provided DistillablePageDetector to detect if the page is
// distillable. The passed detector must be alive until after the callback is
// called.
//
// |web_contents| and |detector| must be non-null.
void IsDistillablePageForDetector(content::WebContents* web_contents,
                                  const DistillablePageDetector* detector,
                                  base::Callback<void(bool)> callback);

struct DistillabilityResult {
  bool is_distillable;
  bool is_last;
  bool is_mobile_friendly;
};
std::ostream& operator<<(std::ostream& os, const DistillabilityResult& result);

class DistillabilityObserver : public base::CheckedObserver {
 public:
  virtual void OnResult(const DistillabilityResult& result) = 0;
};

// Add/remove objects to the list of observers to notify when the distillability
// service returns a result.
//
// |web_contents| and |observer| must both be non-null.
void AddObserver(content::WebContents* web_contents,
                 DistillabilityObserver* observer);
void RemoveObserver(content::WebContents* web_contents,
                    DistillabilityObserver* observer);

base::Optional<DistillabilityResult> GetLatestResult(
    content::WebContents* web_contents);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_
