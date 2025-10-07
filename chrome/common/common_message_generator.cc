// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#include "chrome/common/common_message_generator.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
#include "chrome/common/safe_browsing/protobuf_message_write_macros.h"
namespace IPC {
#include "chrome/common/common_message_generator.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
#include "chrome/common/safe_browsing/protobuf_message_read_macros.h"
namespace IPC {
#include "chrome/common/common_message_generator.h"
}  // namespace IPC

