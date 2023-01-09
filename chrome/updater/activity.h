// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_ACTIVITY_H_
#define CHROME_UPDATER_ACTIVITY_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/updater/updater_scope.h"
#include "components/update_client/activity_data_service.h"

namespace updater {

class ActivityDataService final : public update_client::ActivityDataService {
 public:
  explicit ActivityDataService(UpdaterScope scope);
  ActivityDataService(const ActivityDataService&) = delete;
  ActivityDataService& operator=(const ActivityDataService&) = delete;
  ~ActivityDataService() override = default;

  // update_client::ActivityDataService:
  void GetActiveBits(const std::vector<std::string>& ids,
                     base::OnceCallback<void(const std::set<std::string>&)>
                         callback) const override;

  void GetAndClearActiveBits(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::set<std::string>&)> callback) override;

  int GetDaysSinceLastActive(const std::string& id) const override;

  int GetDaysSinceLastRollCall(const std::string& id) const override;

 private:
  UpdaterScope scope_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_ACTIVITY_H_
