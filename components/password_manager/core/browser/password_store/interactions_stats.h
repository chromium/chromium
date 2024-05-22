// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_INTERACTIONS_STATS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_INTERACTIONS_STATS_H_

#include "base/time/time.h"
#include "url/gurl.h"

namespace password_manager {

// The statistics containing user interactions with a site.
struct InteractionsStats {
  friend bool operator==(const InteractionsStats&,
                         const InteractionsStats&) = default;

  // The domain of the site.
  GURL origin_domain;

  // The value of the username.
  std::u16string username_value;

  // Number of times the user dismissed the bubble.
  int dismissal_count = 0;

  // The date when the row was updated.
  base::Time update_time;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_INTERACTIONS_STATS_H_
