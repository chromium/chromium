// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CAST_SYS_INFO_ANDROID_H_
#define CHROMECAST_BASE_CAST_SYS_INFO_ANDROID_H_

#include <jni.h>
#include <vector>

#include "base/macros.h"
#include "chromecast/public/cast_sys_info.h"

namespace base {
namespace android {
class BuildInfo;
}
}

namespace chromecast {

class CastSysInfoAndroid : public CastSysInfo {
 public:
  CastSysInfoAndroid();
  ~CastSysInfoAndroid() override;

  // CastSysInfo implementation:
  BuildType GetBuildType() override;
  std::string GetSerialNumber() override;
  std::string GetProductName() override;
  std::string GetDeviceModel() override;
  std::string GetManufacturer() override;
  std::string GetSystemBuildNumber() override;
  std::string GetSystemReleaseChannel() override;
  std::string GetBoardName() override;
  std::string GetBoardRevision() override;
  std::string GetFactoryCountry() override;
  std::vector<std::string> GetFactoryLocaleList() override;
  std::string GetWifiInterface() override;
  std::string GetApInterface() override;

 private:
  const base::android::BuildInfo* const build_info_;

  DISALLOW_COPY_AND_ASSIGN(CastSysInfoAndroid);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CAST_SYS_INFO_ANDROID_H_
