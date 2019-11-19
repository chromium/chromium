// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_

#include <string>

#include "base/optional.h"

namespace arc {

class ArcIntentHelperObserver {
 public:
  virtual ~ArcIntentHelperObserver() = default;
  // Called when intent filters are added, removed or updated.
  // A base::nullopt |package_name| indicates that intent filters were updated
  // for all packages. Otherwise, |package_name| contains the name of the
  // package whose filters were changed.
  virtual void OnIntentFiltersUpdated(
      const base::Optional<std::string>& package_name) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_
