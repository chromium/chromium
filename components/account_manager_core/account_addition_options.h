// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_OPTIONS_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_OPTIONS_H_

#include "base/component_export.h"

namespace account_manager {

// Options passed to the account addition request.
struct COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountAdditionOptions {
  // The default value for ARC availability for the account to be added.
  bool is_available_in_arc = false;
  // Whether the account picker that allows to change ARC availability should be
  // shown. When set to `true` - the ARC availability toggle in account addition
  // flow will be hidden.
  bool show_arc_availability_picker = false;
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_OPTIONS_H_
