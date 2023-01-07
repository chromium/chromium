// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef COMPONENTS_NACL_COMMON_NACL_TYPES_PARAM_TRAITS_H_
#define COMPONENTS_NACL_COMMON_NACL_TYPES_PARAM_TRAITS_H_

#include "components/nacl/common/nacl_types.h"
#include "ipc/ipc_message_macros.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

IPC_ENUM_TRAITS_MAX_VALUE(nacl::NaClAppProcessType,
                          nacl::kNumNaClProcessTypes - 1)

IPC_ENUM_TRAITS_MAX_VALUE(NaClErrorCode, NACL_ERROR_CODE_MAX)

#endif  // COMPONENTS_NACL_COMMON_NACL_TYPES_PARAM_TRAITS_H_
