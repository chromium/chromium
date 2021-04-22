// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_PLIST_WRITER_H_
#define COMPONENTS_POLICY_CORE_COMMON_PLIST_WRITER_H_

#include <stddef.h>
#include <string>
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {

// Given a root node, generates a Plist string and puts it into |plist|.
// The output string is overwritten and not appended.
// Return true on success and false on failure.
// TODO(rodmartin): Should we generate plist if it would be invalid plist
// (e.g., |node| is not a DictionaryValue/ListValue or if there are inf/-inf
// float values)?
POLICY_EXPORT bool PlistWrite(const base::Value& node, std::string* plist);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_PLIST_WRITER_H_
