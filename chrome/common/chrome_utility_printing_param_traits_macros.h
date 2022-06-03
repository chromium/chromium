// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_UTILITY_PRINTING_PARAM_TRAITS_MACROS_H_
#define CHROME_COMMON_CHROME_UTILITY_PRINTING_PARAM_TRAITS_MACROS_H_

#include "build/build_config.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_param_traits.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"

#if defined(OS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Preview and Cloud Print messages.
IPC_STRUCT_TRAITS_BEGIN(printing::PrinterCapsAndDefaults)
  IPC_STRUCT_TRAITS_MEMBER(printer_capabilities)
  IPC_STRUCT_TRAITS_MEMBER(caps_mime_type)
  IPC_STRUCT_TRAITS_MEMBER(printer_defaults)
  IPC_STRUCT_TRAITS_MEMBER(defaults_mime_type)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(printing::mojom::ColorModel,
                          printing::mojom::ColorModel::kColorModelLast)

IPC_STRUCT_TRAITS_BEGIN(printing::PrinterSemanticCapsAndDefaults::Paper)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(vendor_id)
  IPC_STRUCT_TRAITS_MEMBER(size_um)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MIN_MAX_VALUE(printing::mojom::DuplexMode,
                              printing::mojom::DuplexMode::kUnknownDuplexMode,
                              printing::mojom::DuplexMode::kShortEdge)

IPC_STRUCT_TRAITS_BEGIN(printing::PrinterSemanticCapsAndDefaults)
  IPC_STRUCT_TRAITS_MEMBER(collate_capable)
  IPC_STRUCT_TRAITS_MEMBER(collate_default)
  IPC_STRUCT_TRAITS_MEMBER(copies_max)
  IPC_STRUCT_TRAITS_MEMBER(duplex_modes)
  IPC_STRUCT_TRAITS_MEMBER(duplex_default)
  IPC_STRUCT_TRAITS_MEMBER(color_changeable)
  IPC_STRUCT_TRAITS_MEMBER(color_default)
  IPC_STRUCT_TRAITS_MEMBER(color_model)
  IPC_STRUCT_TRAITS_MEMBER(bw_model)
  IPC_STRUCT_TRAITS_MEMBER(papers)
  IPC_STRUCT_TRAITS_MEMBER(default_paper)
  IPC_STRUCT_TRAITS_MEMBER(dpis)
  IPC_STRUCT_TRAITS_MEMBER(default_dpi)
IPC_STRUCT_TRAITS_END()

#endif  // defined(OS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)

#endif  // CHROME_COMMON_CHROME_UTILITY_PRINTING_PARAM_TRAITS_MACROS_H_
