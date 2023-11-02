// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CHROME_SCHEMA_H_
#define COMPONENTS_POLICY_CORE_COMMON_CHROME_SCHEMA_H_

#include "components/policy/policy_export.h"

namespace policy {

class Schema;

// Returns the policy Schema generated from policy_templates.json
// Takes up very little memory, never destroyed.
POLICY_EXPORT const Schema& GetChromeSchema();

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CHROME_SCHEMA_H_
