// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/activity_impl.h"

namespace updater {
namespace {
constexpr int kDaysUnknown = -2;
}

ActivityDataService::ActivityDataService(bool is_machine)
    : is_machine_(is_machine) {}

void ActivityDataService::GetActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) const {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const std::vector<std::string>& ids, bool is_machine) {
            std::set<std::string> result;
            for (const auto& id : ids) {
              if (GetActiveBit(id, is_machine))
                result.insert(id);
            }
            return result;
          },
          ids, is_machine_),
      std::move(callback));
}

void ActivityDataService::GetAndClearActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const std::vector<std::string>& ids, bool is_machine) {
            std::set<std::string> result;
            for (const auto& id : ids) {
              if (GetActiveBit(id, is_machine))
                result.insert(id);
              ClearActiveBit(id, is_machine);
            }
            return result;
          },
          ids, is_machine_),
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
