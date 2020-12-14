// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_RESTORE_DATA_H_
#define COMPONENTS_FULL_RESTORE_RESTORE_DATA_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "components/full_restore/app_restore_data.h"

namespace full_restore {

// This class is responsible for saving all app launch and app windows
// information. It can be converted to JSON format to be written to the
// FullRestoreData file.
//
// TODO(crbug.com/1146900):
// 1. Add the interface to convert this struct to JSON format.
// 2. Add the interface to add a LaunchAndWindowInfo with AppLaunchInfo.
// 3. Add the interface to modify LaunchAndWindowInfo when the window
// information is updated.
// 4. Add the interface to remove LaunchAndWindowInfo.
class COMPONENT_EXPORT(FULL_RESTORE) RestoreData {
 public:
  // Map from a window id to AppRestoreData.
  using LaunchList = std::map<int, std::unique_ptr<AppRestoreData>>;

  // Map from an app id to LaunchList.
  using AppIdToLaunchList = std::map<std::string, LaunchList>;

  RestoreData();
  ~RestoreData();

  RestoreData(const RestoreData&) = delete;
  RestoreData& operator=(const RestoreData&) = delete;

 private:
  AppIdToLaunchList app_id_to_launch_list_;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_RESTORE_DATA_H_
