// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/activity_impl.h"
#include "chrome/updater/updater_scope.h"

namespace updater {
namespace {
constexpr int kDaysUnknown = -2;
}

ActivityDataService::ActivityDataService(UpdaterScope scope) : scope_(scope) {}

void ActivityDataService::GetActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) const {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](UpdaterScope scope, const std::vector<std::string>& ids) {
            std::set<std::string> result;
            for (const auto& id : ids) {
              if (GetActiveBit(scope, id)) {
                result.insert(id);
              }
            }
            return result;
          },
          scope_, ids),
      std::move(callback));
}

void ActivityDataService::GetAndClearActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](UpdaterScope scope, const std::vector<std::string>& ids) {
            std::set<std::string> result;
            for (const auto& id : ids) {
              if (GetActiveBit(scope, id)) {
                result.insert(id);
              }
              ClearActiveBit(scope, id);
            }
            return result;
          },
          scope_, ids),
      std::move(callback));
}

int ActivityDataService::GetDaysSinceLastActive(const std::string& id) const {
  // The updater does not report DaysSince data, only DateLast data.
  return kDaysUnknown;
}

int ActivityDataService::GetDaysSinceLastRollCall(const std::string& id) const {
  // The updater does not report DaysSince data, only DateLast data.
  return kDaysUnknown;
}

}  // namespace updater
