// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/network/shill_service_util.h"

#include "base/strings/stringprintf.h"

namespace ash {

ShillServiceInfo::ShillServiceInfo(unsigned int id)
    : service_name_(base::StringPrintf("service_name_%u", id)),
      service_path_(base::StringPrintf("service_path_%u", id)),
      service_guid_(base::StringPrintf("service_guid_%u", id)) {}

ShillServiceInfo::~ShillServiceInfo() = default;

}  // namespace ash
