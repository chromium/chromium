// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SMART_BUBBLE_STATS_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SMART_BUBBLE_STATS_STORE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace password_manager {

class PasswordStoreConsumer;
struct InteractionsStats;

// Interface for storing stats related to smart bubble behaviour of password
// save and update prompts. All methods are expected to have an asynchronous
// implementation that persists changes to a local database or an external
// service organizing the passwords.
class SmartBubbleStatsStore {
 public:
  // Adds or replaces the statistics for the domain |stats.origin_domain|.
  virtual void AddSiteStats(const InteractionsStats& stats) = 0;

  // TODO(crbug.com/40130501): replace GURL with Origin.
  // Removes the statistics for |origin_domain|.
  virtual void RemoveSiteStats(const GURL& origin_domain) = 0;

  // Retrieves the statistics for |origin_domain| and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  virtual void GetSiteStats(const GURL& origin_domain,
                            base::WeakPtr<PasswordStoreConsumer> consumer) = 0;

  // Removes all the stats created in the given date range.
  // If |origin_filter| is not null, only statistics for matching origins are
  // removed. If |completion| is not null, it will be run after deletions have
  // been completed. Should be called on the UI thread.
  virtual void RemoveStatisticsByOriginAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion) = 0;

 protected:
  virtual ~SmartBubbleStatsStore() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SMART_BUBBLE_STATS_STORE_H_
