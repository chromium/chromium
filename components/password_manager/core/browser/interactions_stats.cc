// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/interactions_stats.h"

namespace password_manager {

bool operator==(const InteractionsStats& lhs, const InteractionsStats& rhs) {
  return lhs.origin_domain == rhs.origin_domain &&
         lhs.username_value == rhs.username_value &&
         lhs.dismissal_count == rhs.dismissal_count &&
         lhs.update_time == rhs.update_time;
}

} // namespace password_manager
 