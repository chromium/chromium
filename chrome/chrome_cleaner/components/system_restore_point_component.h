// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_COMPONENTS_SYSTEM_RESTORE_POINT_COMPONENT_H_
#define CHROME_CHROME_CLEANER_COMPONENTS_SYSTEM_RESTORE_POINT_COMPONENT_H_

#include <windows.h>

#include <srrestoreptapi.h>
#include <stdint.h>

#include <vector>

#include "base/native_library.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/components/component_api.h"

namespace chrome_cleaner {

// This class manages the setting and clearing of a system restore point.
class SystemRestorePointComponent : public ComponentAPI {
 public:
  explicit SystemRestorePointComponent(const base::string16& product_fullname);

  // ComponentAPI methods.
  void PreScan() override;
  void PostScan(const std::vector<UwSId>& found_pups) override;
  void PreCleanup() override;
  void PostCleanup(ResultCode result_code, RebooterAPI* rebooter) override;
  void PostValidation(ResultCode result_code) override;
  void OnClose(ResultCode result_code) override;

 protected:
  // The below typedefs and members have protected visibility for testing.
  typedef BOOL(WINAPI* SetRestorePointInfoWFn)(PRESTOREPOINTINFOW,
                                               PSTATEMGRSTATUS);
  typedef BOOL(WINAPI* RemoveRestorePointFn)(DWORD);

  SetRestorePointInfoWFn set_restore_point_info_fn_;
  RemoveRestorePointFn remove_restore_point_info_fn_;

 private:
  // Perform internal sanity checks to validate the presence of the restore
  // point library.
  bool IsLoadedRestorePointLibrary();
  // Safe wrappers to dynamic calls to the restore point library.
  bool SetRestorePointInfoWrapper(PRESTOREPOINTINFOW, PSTATEMGRSTATUS);
  bool RemoveRestorePointWrapper(DWORD);

  base::NativeLibrary srclient_dll_;
  int64_t sequence_number_;
  base::string16 product_fullname_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_COMPONENTS_SYSTEM_RESTORE_POINT_COMPONENT_H_
