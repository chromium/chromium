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

PrintMsg_Print_Params::PrintMsg_Print_Params()
    : margin_top(0),
      margin_left(0),
      scale_factor(1.0f),
      rasterize_pdf(false),
      document_cookie(0),
      selection_only(false),
      supports_alpha_blend(false),
      preview_ui_id(-1),
      preview_request_id(0),
      is_first_request(false),
      print_scaling_option(blink::kWebPrintScalingOptionSourceSize),
      print_to_pdf(false),
      display_header_footer(false),
      should_print_backgrounds(false),
      printed_doc_type(printing::SkiaDocumentType::PDF),
      prefer_css_page_size(false),
      pages_per_sheet(1) {}

PrintMsg_Print_Params::PrintMsg_Print_Params(
    const PrintMsg_Print_Params& other) = default;

PrintMsg_Print_Params::~PrintMsg_Print_Params() {}

void PrintMsg_Print_Params::Reset() {
  page_size = gfx::Size();
  content_size = gfx::Size();
  printable_area = gfx::Rect();
  margin_top = 0;
  margin_left = 0;
  dpi = gfx::Size();
  scale_factor = 1.0f;
  rasterize_pdf = false;
  document_cookie = 0;
  selection_only = false;
  supports_alpha_blend = false;
  preview_ui_id = -1;
  preview_request_id = 0;
  is_first_request = false;
  print_scaling_option = blink::kWebPrintScalingOptionSourceSize;
  print_to_pdf = false;
  display_header_footer = false;
  title = base::string16();
  url = base::string16();
  header_template = base::string16();
  footer_template = base::string16();
  should_print_backgrounds = false;
  printed_doc_type = printing::SkiaDocumentType::PDF;
  prefer_css_page_size = false;
  pages_per_sheet = 1;
}

PrintMsg_PrintPages_Params::PrintMsg_PrintPages_Params() {}

PrintMsg_PrintPages_Params::PrintMsg_PrintPages_Params(
    const PrintMsg_PrintPages_Params& other) = default;

PrintMsg_PrintPages_Params::~PrintMsg_PrintPages_Params() {}

void PrintMsg_PrintPages_Params::Reset() {
  params.Reset();
  pages = std::vector<int>();
}

PrintMsg_PrintFrame_Params::PrintMsg_PrintFrame_Params() {}

PrintMsg_PrintFrame_Params::~PrintMsg_PrintFrame_Params() {}

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

PrintHostMsg_PreviewIds::PrintHostMsg_PreviewIds()
    : request_id(-1), ui_id(-1) {}

PrintHostMsg_PreviewIds::PrintHostMsg_PreviewIds(int request_id, int ui_id)
    : request_id(request_id), ui_id(ui_id) {}

PrintHostMsg_PreviewIds::~PrintHostMsg_PreviewIds() {}

PrintHostMsg_SetOptionsFromDocument_Params::
    PrintHostMsg_SetOptionsFromDocument_Params()
    : is_scaling_disabled(false),
      copies(0),
      duplex(printing::UNKNOWN_DUPLEX_MODE) {
}

PrintHostMsg_SetOptionsFromDocument_Params::
    ~PrintHostMsg_SetOptionsFromDocument_Params() {
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
