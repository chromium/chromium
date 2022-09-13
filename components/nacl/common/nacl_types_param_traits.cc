// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "components/nacl/common/nacl_types_param_traits.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#undef COMPONENTS_NACL_COMMON_NACL_TYPES_PARAM_TRAITS_H_
#include "components/nacl/common/nacl_types_param_traits.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef COMPONENTS_NACL_COMMON_NACL_TYPES_PARAM_TRAITS_H_
#include "components/nacl/common/nacl_types_param_traits.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef COMPONENTS_NACL_COMMON_NACL_TYPES_PARAM_TRAITS_H_
#include "components/nacl/common/nacl_types_param_traits.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef COMPONENTS_NACL_COMMON_NACL_TYPES_PARAM_TRAITS_H_
#include "components/nacl/common/nacl_types_param_traits.h"
}  // namespace IPC
