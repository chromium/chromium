// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_VALUE_UTIL_H_
#define COMPONENTS_MIRRORING_SERVICE_VALUE_UTIL_H_

#include <string>

#include "base/values.h"

namespace mirroring {

// Reads a string from dictionary |value| if |key| exits. Returns
// false if |key| exists and the value is not a string. Returns true
// otherwise.
bool GetString(const base::Value& value,
               const std::string& key,
               std::string* result);

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_VALUE_UTIL_H_
