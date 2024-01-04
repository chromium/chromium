// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_activity_data_service.h"

#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "components/update_client/persisted_data.h"

namespace update_client {

namespace {
constexpr int kUnknown = -2;
}  // namespace

namespace test {

void SetDateLastData(PersistedData* metadata,
                     const std::vector<std::string>& ids,
                     int daynum) {
  base::RunLoop loop;
  metadata->SetDateLastData(ids, daynum, loop.QuitClosure());
  loop.Run();
}

bool GetActiveBit(PersistedData* metadata, const std::string& id) {
  base::RunLoop loop;
  bool result = false;
  metadata->GetActiveBits(
      {id},
      base::BindLambdaForTesting([&](const std::set<std::string>& actives) {
        result = actives.find(id) != actives.end();
        loop.QuitClosure().Run();
      }));
  loop.Run();
  return result;
}

}  // namespace test

TestActivityDataService::TestActivityDataService() = default;
TestActivityDataService::~TestActivityDataService() = default;

bool TestActivityDataService::GetActiveBit(const std::string& id) const {
  const auto& it = actives_.find(id);
  return it != actives_.end() ? it->second : false;
}

void TestActivityDataService::GetActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) const {
  std::set<std::string> actives;
  for (const auto& id : ids) {
    auto it = actives_.find(id);
    if (it != actives_.end() && it->second) {
      actives.insert(id);
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), actives));
}

void TestActivityDataService::GetAndClearActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) {
  std::set<std::string> actives;
  for (const auto& id : ids) {
    if (actives_.count(id) > 0 && actives_.at(id)) {
      actives.insert(id);
    }
    actives_[id] = false;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), actives));
}

int TestActivityDataService::GetDaysSinceLastActive(
    const std::string& id) const {
  const auto& it = days_since_last_actives_.find(id);
  return it != days_since_last_actives_.end() ? it->second : kUnknown;
}

int TestActivityDataService::GetDaysSinceLastRollCall(
    const std::string& id) const {
  const auto& it = days_since_last_rollcalls_.find(id);
  return it != days_since_last_rollcalls_.end() ? it->second : kUnknown;
}

void TestActivityDataService::SetActiveBit(const std::string& id, bool value) {
  actives_[id] = value;
}

void TestActivityDataService::SetDaysSinceLastActive(const std::string& id,
                                                     int daynum) {
  days_since_last_actives_[id] = daynum;
}

void TestActivityDataService::SetDaysSinceLastRollCall(const std::string& id,
                                                       int daynum) {
  days_since_last_rollcalls_[id] = daynum;
}

}  // namespace update_client
