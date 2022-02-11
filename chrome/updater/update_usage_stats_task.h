// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_
#define CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"

namespace updater {

class PersistedData;

class UpdateUsageStatsTask
    : public base::RefCountedThreadSafe<UpdateUsageStatsTask> {
 public:
  explicit UpdateUsageStatsTask(scoped_refptr<PersistedData> persisted_data);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<UpdateUsageStatsTask>;
  virtual ~UpdateUsageStatsTask();

  bool UsageStatsAllowed(const std::vector<std::string>& app_ids);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<PersistedData> persisted_data_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_
