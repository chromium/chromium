// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "printing/buildflags/buildflags.h"
#include "ui/gfx/geometry/size.h"

// Generating implementations for all aspects of the IPC message
// handling by setting appropriate IPC macros and including the
// message file, over and over again until all versions have been
// generated.

#define IPC_MESSAGE_IMPL
#undef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#undef COMPONENTS_PRINTING_COMMON_PRINTING_PARAM_TRAITS_MACROS_H_
#include "components/printing/common/print_messages.h"
#ifndef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#error "Failed to include header components/printing/common/print_messages.h"
#endif

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#undef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#undef COMPONENTS_PRINTING_COMMON_PRINTING_PARAM_TRAITS_MACROS_H_
#include "components/printing/common/print_messages.h"
#ifndef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#error "Failed to include header components/printing/common/print_messages.h"
#endif

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#undef COMPONENTS_PRINTING_COMMON_PRINTING_PARAM_TRAITS_MACROS_H_
#include "components/printing/common/print_messages.h"
#ifndef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#error "Failed to include header components/printing/common/print_messages.h"
#endif
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#undef COMPONENTS_PRINTING_COMMON_PRINTING_PARAM_TRAITS_MACROS_H_
#include "components/printing/common/print_messages.h"
#ifndef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#error "Failed to include header components/printing/common/print_messages.h"
#endif
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
// Force multiple inclusion of the param traits file to generate all methods.
#undef COMPONENTS_PRINTING_COMMON_PRINTING_PARAM_TRAITS_MACROS_H_

#include "components/printing/common/print_messages.h"
#ifndef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#error "Failed to include header components/printing/common/print_messages.h"
#endif
}  // namespace IPC

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
PrintHostMsg_RequestPrintPreview_Params::
    PrintHostMsg_RequestPrintPreview_Params()
    : is_from_arc(false),
      is_modifiable(false),
      is_pdf(false),
      webnode_only(false),
      has_selection(false),
      selection_only(false) {}

PrintHostMsg_RequestPrintPreview_Params::
    ~PrintHostMsg_RequestPrintPreview_Params() {}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
