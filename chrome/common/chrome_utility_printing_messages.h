// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_UTILITY_PRINTING_MESSAGES_H_
#define CHROME_COMMON_CHROME_UTILITY_PRINTING_MESSAGES_H_

#include <string>

#include "build/build_config.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"

#define IPC_MESSAGE_START ChromeUtilityPrintingMsgStart

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

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

// Tells the utility process to get capabilities and defaults for the specified
// printer. Used on Windows to isolate the service process from printer driver
// crashes by executing this in a separate process. This does not run in a
// sandbox.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_GetPrinterCapsAndDefaults,
                     std::string /* printer name */)

// Tells the utility process to get capabilities and defaults for the specified
// printer. Used on Windows to isolate the service process from printer driver
// crashes by executing this in a separate process. This does not run in a
// sandbox. Returns result as printing::PrinterSemanticCapsAndDefaults.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_GetPrinterSemanticCapsAndDefaults,
                     std::string /* printer name */)

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

// Reply when the utility process has succeeded in obtaining the printer
// capabilities and defaults.
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded,
                     std::string /* printer name */,
                     printing::PrinterCapsAndDefaults)

// Reply when the utility process has succeeded in obtaining the printer
// semantic capabilities and defaults.
IPC_MESSAGE_CONTROL2(
    ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Succeeded,
    std::string /* printer name */,
    printing::PrinterSemanticCapsAndDefaults)

// Reply when the utility process has failed to obtain the printer
// capabilities and defaults.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed,
                     std::string /* printer name */)

// Reply when the utility process has failed to obtain the printer
// semantic capabilities and defaults.
IPC_MESSAGE_CONTROL1(
  ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Failed,
  std::string /* printer name */)
#endif  // defined(OS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)

#endif  // CHROME_COMMON_CHROME_UTILITY_PRINTING_MESSAGES_H_
