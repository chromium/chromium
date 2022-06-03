// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_profile.h"

#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace chromeos {

std::string NetworkProfile::ToDebugString() const {
  if (type() == NetworkProfile::TYPE_SHARED) {
    return base::StringPrintf("NetworkProfile(SHARED, %s)",
                              path.c_str());
  } else if (type() == NetworkProfile::TYPE_USER) {
    return base::StringPrintf("NetworkProfile(USER, %s, %s)",
                              path.c_str(),
                              userhash.c_str());
  }
  NOTREACHED();
  return std::string();
}

}  // namespace chromeos
