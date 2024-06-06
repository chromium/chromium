// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/cellular/esim_util.h"

#include "base/strings/stringprintf.h"

namespace ash {

EuiccInfo::EuiccInfo(unsigned int id)
    : path_(base::StringPrintf("/hermes/euicc%u", id)),
      eid_(base::StringPrintf("%32u", id)) {}

EuiccInfo::~EuiccInfo() = default;

EsimInfo::EsimInfo(unsigned int id)
    : profile_path_(base::StringPrintf("/hermes/profile%u", id)),
      iccid_(base::StringPrintf("%18u", id)),
      name_(base::StringPrintf("Profile Name %u", id)),
      nickname_(base::StringPrintf("Profile Nickname %u", id)),
      service_provider_(base::StringPrintf("Service Provider %u", id)),
      // TODO(b/339260115): Align Shill and Hermes fakes so that the service
      // path and GUID logic is consistent for cellular networks.
      service_path_(base::StringPrintf("service_path_for_%s", iccid_.c_str())) {
}

EsimInfo::~EsimInfo() = default;

}  // namespace ash
