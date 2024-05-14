// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_profile.h"

#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace ash {

std::string NetworkProfile::ToDebugString() const {
  if (type() == NetworkProfile::TYPE_SHARED) {
    return base::StringPrintf("NetworkProfile(SHARED, %s)",
                              path.c_str());
  } else if (type() == NetworkProfile::TYPE_USER) {
    return base::StringPrintf("NetworkProfile(USER, %s, %s)",
                              path.c_str(),
                              userhash.c_str());
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace ash
