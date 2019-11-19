// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_H_

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action.pb.h"

namespace app_list {

class CrOSActionHistoryProto;

// CrOSActionRecorder is a singleton that used to record any CrOSAction.
// CrOSAction may contains:
//   (1) App-launchings.
//   (2) File-openings.
//   (3) Settings.
//   (4) Tab-navigations.
// and so on.
class CrOSActionRecorder {
 public:
  using CrOSActionName = std::string;
  using CrOSAction = std::tuple<CrOSActionName>;

  CrOSActionRecorder();
  ~CrOSActionRecorder();
  // Get the pointer of the singleton.
  static CrOSActionRecorder* GetCrosActionRecorder();

  // Record a user |action| with |conditions|.
  void RecordAction(
      const CrOSAction& action,
      const std::vector<std::pair<std::string, int>>& conditions = {});

 private:
  friend class CrOSActionRecorderTest;

  // kSaveInternal controls how often we save the action history to disk.
  static constexpr base::TimeDelta kSaveInternal =
      base::TimeDelta::FromHours(1);
  // The sub-directory in profile path where the action history is stored.
  static constexpr char kActionHistoryDir[] = "cros_action_history/";

  // Saves the current |actions_| to disk and clear it when certain
  // criteria is met.
  void MaybeFlushToDisk();

  // Get CrOSActionRecorderType from
  // app_list_features::kEnableCrOSActionRecorder and set |should_log_|,
  // |should_hash_| accordingly.
  void SetCrOSActionRecorderType();

  // Hashes the |input| if |should_hash| is true; otherwise return |input|.
  static std::string MaybeHashed(const std::string& input, bool should_hash);

  // Controls whether the logging is enabled.
  bool should_log_ = false;
  // Controls whether to hash the action and condition names before log.
  bool should_hash_ = true;
  // The timestamp of last save to disk.
  base::Time last_save_timestamp_;
  // A list of actions since last save.
  CrOSActionHistoryProto actions_;
  // Profile path to save the actions.
  base::FilePath profile_path_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CrOSActionRecorder);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_H_
