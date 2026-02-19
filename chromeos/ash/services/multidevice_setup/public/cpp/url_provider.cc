// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/url_provider.h"

#include "ash/constants/url_constants.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"

namespace ash {

namespace multidevice_setup {

GURL GetBoardSpecificBetterTogetherSuiteLearnMoreUrl() {
  return GURL(base::StrCat({ash::external_urls::kMultiDeviceLearnMoreURL,
                            "&b=", base::SysInfo::GetLsbReleaseBoard()}));
}

GURL GetBoardSpecificMessagesLearnMoreUrl() {
  return GURL(base::StrCat({ash::external_urls::kAndroidMessagesLearnMoreURL,
                            "&b=", base::SysInfo::GetLsbReleaseBoard()}));
}

}  // namespace multidevice_setup

}  // namespace ash
