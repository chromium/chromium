// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "components/version_info/version_info.h"
#include "components/version_info/version_string.h"

namespace chrome {

std::string GetVersionString(WithExtendedStable with_extended_stable) {
  return version_info::GetVersionStringWithModifier(
      GetChannelName(with_extended_stable));
}

}  // namespace chrome
