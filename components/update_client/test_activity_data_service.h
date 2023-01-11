// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TEST_ACTIVITY_DATA_SERVICE_H_
#define COMPONENTS_UPDATE_CLIENT_TEST_ACTIVITY_DATA_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/update_client/activity_data_service.h"

namespace update_client {

class PersistedData;

namespace test {

// Synchronously set date-last data.
void SetDateLastData(PersistedData* metadata,
                     const std::vector<std::string>& ids,
                     int daynum);

// Synchronously fetch the active bit.
bool GetActiveBit(PersistedData* metadata, const std::string& id);

}  // namespace test

class TestActivityDataService final : public ActivityDataService {
 public:
  TestActivityDataService();
  TestActivityDataService(const TestActivityDataService&) = delete;
  TestActivityDataService& operator=(const TestActivityDataService&) = delete;
  ~TestActivityDataService() override;

  // ActivityDataService overrides
  void GetActiveBits(const std::vector<std::string>& ids,
                     base::OnceCallback<void(const std::set<std::string>&)>
                         callback) const override;
  void GetAndClearActiveBits(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::set<std::string>&)> callback) override;
  int GetDaysSinceLastActive(const std::string& id) const override;
  int GetDaysSinceLastRollCall(const std::string& id) const override;

  bool GetActiveBit(const std::string& id) const;
  void SetActiveBit(const std::string& id, bool value);
  void SetDaysSinceLastActive(const std::string& id, int daynum);
  void SetDaysSinceLastRollCall(const std::string& id, int daynum);

 private:
  std::map<std::string, bool> actives_;
  std::map<std::string, int> days_since_last_actives_;
  std::map<std::string, int> days_since_last_rollcalls_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TEST_ACTIVITY_DATA_SERVICE_H_
