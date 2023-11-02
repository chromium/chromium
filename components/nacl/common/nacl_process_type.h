// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_PROCESS_TYPE_H_
#define COMPONENTS_NACL_COMMON_NACL_PROCESS_TYPE_H_

#include "content/public/common/process_type.h"

// Defines the trusted process types that are custom to NaCl.
enum NaClTrustedProcessType {
  // Start at +1 because we removed an unused value and didn't want to change
  // the IDs as they're used in UMA (see the comment for ProcessType).
  PROCESS_TYPE_NACL_LOADER = content::PROCESS_TYPE_CONTENT_END + 1,
  PROCESS_TYPE_NACL_BROKER,
};

#endif  // COMPONENTS_NACL_COMMON_NACL_PROCESS_TYPE_H_
