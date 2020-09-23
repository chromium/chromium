// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for printing.
#ifndef COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#define COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/printing_param_traits.h"
#include "components/printing/common/printing_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "printing/print_job_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#ifndef INTERNAL_COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
#define INTERNAL_COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
struct PrintHostMsg_RequestPrintPreview_Params {
  PrintHostMsg_RequestPrintPreview_Params();
  ~PrintHostMsg_RequestPrintPreview_Params();
  bool is_from_arc;
  bool is_modifiable;
  bool is_pdf;
  bool webnode_only;
  bool has_selection;
  bool selection_only;
};
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#endif  // INTERNAL_COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_

#define IPC_MESSAGE_START PrintMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(printing::mojom::PageOrientation,
                          printing::mojom::PageOrientation::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(printing::mojom::PrintScalingOption,
                          printing::mojom::PrintScalingOption::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(printing::mojom::SkiaDocumentType,
                          printing::mojom::SkiaDocumentType::kMaxValue)

// Parameters for a render request.
IPC_STRUCT_TRAITS_BEGIN(printing::mojom::PrintParams)
  // Physical size of the page, including non-printable margins,
  // in pixels according to dpi.
  IPC_STRUCT_TRAITS_MEMBER(page_size)

  // In pixels according to dpi.
  IPC_STRUCT_TRAITS_MEMBER(content_size)

  // Physical printable area of the page in pixels according to dpi.
  IPC_STRUCT_TRAITS_MEMBER(printable_area)

  // The y-offset of the printable area, in pixels according to dpi.
  IPC_STRUCT_TRAITS_MEMBER(margin_top)

  // The x-offset of the printable area, in pixels according to dpi.
  IPC_STRUCT_TRAITS_MEMBER(margin_left)

  // Specifies the page orientation.
  IPC_STRUCT_TRAITS_MEMBER(page_orientation)

  // Specifies dots per inch in the x and y direction.
  IPC_STRUCT_TRAITS_MEMBER(dpi)

  // Specifies the scale factor in percent
  IPC_STRUCT_TRAITS_MEMBER(scale_factor)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_TRAITS_MEMBER(document_cookie)

  // Should only print currently selected text.
  IPC_STRUCT_TRAITS_MEMBER(selection_only)

  // Does the printer support alpha blending?
  IPC_STRUCT_TRAITS_MEMBER(supports_alpha_blend)

  // *** Parameters below are used only for print preview. ***

  // The print preview ui associated with this request.
  IPC_STRUCT_TRAITS_MEMBER(preview_ui_id)

  // The id of the preview request.
  IPC_STRUCT_TRAITS_MEMBER(preview_request_id)

  // True if this is the first preview request.
  IPC_STRUCT_TRAITS_MEMBER(is_first_request)

  // Specifies the page scaling option for preview printing.
  IPC_STRUCT_TRAITS_MEMBER(print_scaling_option)

  // True if print to pdf is requested.
  IPC_STRUCT_TRAITS_MEMBER(print_to_pdf)

  // Specifies if the header and footer should be rendered.
  IPC_STRUCT_TRAITS_MEMBER(display_header_footer)

  // Title string to be printed as header if requested by the user.
  IPC_STRUCT_TRAITS_MEMBER(title)

  // URL string to be printed as footer if requested by the user.
  IPC_STRUCT_TRAITS_MEMBER(url)

  // HTML template to use as a print header.
  IPC_STRUCT_TRAITS_MEMBER(header_template)

  // HTML template to use as a print footer.
  IPC_STRUCT_TRAITS_MEMBER(footer_template)

  // Whether to rasterize a PDF for printing
  IPC_STRUCT_TRAITS_MEMBER(rasterize_pdf)

  // True if print backgrounds is requested by the user.
  IPC_STRUCT_TRAITS_MEMBER(should_print_backgrounds)

  // The document type of printed page(s) from render.
  IPC_STRUCT_TRAITS_MEMBER(printed_doc_type)

  // True if page size defined by css should be preferred.
  IPC_STRUCT_TRAITS_MEMBER(prefer_css_page_size)

  // Number of pages per sheet.  This parameter is for N-up mode.
  // Defaults to 1 if the feature is disabled, and some number greater
  // than 1 otherwise.  See printing::NupParameters for supported values.
  IPC_STRUCT_TRAITS_MEMBER(pages_per_sheet)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(printing::PageRange)
  IPC_STRUCT_TRAITS_MEMBER(from)
  IPC_STRUCT_TRAITS_MEMBER(to)
IPC_STRUCT_TRAITS_END()

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IPC_STRUCT_TRAITS_BEGIN(PrintHostMsg_RequestPrintPreview_Params)
  IPC_STRUCT_TRAITS_MEMBER(is_from_arc)
  IPC_STRUCT_TRAITS_MEMBER(is_modifiable)
  IPC_STRUCT_TRAITS_MEMBER(is_pdf)
  IPC_STRUCT_TRAITS_MEMBER(webnode_only)
  IPC_STRUCT_TRAITS_MEMBER(has_selection)
  IPC_STRUCT_TRAITS_MEMBER(selection_only)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(printing::mojom::PreviewIds)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(ui_id)
IPC_STRUCT_TRAITS_END()
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

IPC_STRUCT_TRAITS_BEGIN(printing::mojom::PageSizeMargins)
  IPC_STRUCT_TRAITS_MEMBER(content_width)
  IPC_STRUCT_TRAITS_MEMBER(content_height)
  IPC_STRUCT_TRAITS_MEMBER(margin_left)
  IPC_STRUCT_TRAITS_MEMBER(margin_right)
  IPC_STRUCT_TRAITS_MEMBER(margin_top)
  IPC_STRUCT_TRAITS_MEMBER(margin_bottom)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(printing::mojom::PrintPagesParams)
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  IPC_STRUCT_TRAITS_MEMBER(params)

  // If empty, this means a request to render all the printed pages.
  IPC_STRUCT_TRAITS_MEMBER(pages)
IPC_STRUCT_TRAITS_END()

// Holds the printed content information.
// The printed content is in shared memory, and passed as a region.
// A map on out-of-process subframe contents is also included so the printed
// content can be composited as needed.
IPC_STRUCT_TRAITS_BEGIN(printing::mojom::DidPrintContentParams)
  // A shared memory region for the metafile data.
  IPC_STRUCT_TRAITS_MEMBER(metafile_data_region)

  // Content id to render frame proxy id mapping for out-of-process subframes.
  IPC_STRUCT_TRAITS_MEMBER(subframe_content_info)
IPC_STRUCT_TRAITS_END()

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Parameters to describe the to-be-rendered preview document.
IPC_STRUCT_TRAITS_BEGIN(printing::mojom::DidStartPreviewParams)
  // Total page count for the rendered preview. (Not the number of pages the
  // user selected to print.)
  IPC_STRUCT_TRAITS_MEMBER(page_count)

  // The list of 0-based page numbers that will be rendered.
  IPC_STRUCT_TRAITS_MEMBER(pages_to_render)

  // number of pages per sheet and should be greater or equal to 1.
  IPC_STRUCT_TRAITS_MEMBER(pages_per_sheet)

  // Physical size of the page, including non-printable margins.
  IPC_STRUCT_TRAITS_MEMBER(page_size)

  // Scaling % to fit to page
  IPC_STRUCT_TRAITS_MEMBER(fit_to_page_scaling)
IPC_STRUCT_TRAITS_END()

// Parameters to describe a rendered preview page.
IPC_STRUCT_TRAITS_BEGIN(printing::mojom::DidPreviewPageParams)
  // Page's content including metafile data and subframe info.
  IPC_STRUCT_TRAITS_MEMBER(content)

  // |page_number| is zero-based and should not be negative.
  IPC_STRUCT_TRAITS_MEMBER(page_number)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_TRAITS_MEMBER(document_cookie)
IPC_STRUCT_TRAITS_END()

// Parameters to describe the final rendered preview document.
IPC_STRUCT_TRAITS_BEGIN(printing::mojom::DidPreviewDocumentParams)
  // Document's content including metafile data and subframe info.
  IPC_STRUCT_TRAITS_MEMBER(content)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_TRAITS_MEMBER(document_cookie)

  // Store the expected pages count.
  IPC_STRUCT_TRAITS_MEMBER(expected_pages_count)
IPC_STRUCT_TRAITS_END()
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

// Parameters to describe a rendered page.
IPC_STRUCT_TRAITS_BEGIN(printing::mojom::DidPrintDocumentParams)
  // Document's content including metafile data and subframe info.
  IPC_STRUCT_TRAITS_MEMBER(content)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_TRAITS_MEMBER(document_cookie)

  // The size of the page the page author specified.
  IPC_STRUCT_TRAITS_MEMBER(page_size)

  // The printable area the page author specified.
  IPC_STRUCT_TRAITS_MEMBER(content_area)

  // The physical offsets of the printer in DPI. Used for PS printing.
  IPC_STRUCT_TRAITS_MEMBER(physical_offsets)
IPC_STRUCT_TRAITS_END()

// TODO(dgn): Rename *ScriptedPrint messages because they are not called only
//           from scripts.
// Parameters for the IPC message PrintHostMsg_ScriptedPrint
IPC_STRUCT_TRAITS_BEGIN(printing::mojom::ScriptedPrintParams)
  IPC_STRUCT_TRAITS_MEMBER(cookie)
  IPC_STRUCT_TRAITS_MEMBER(expected_pages_count)
  IPC_STRUCT_TRAITS_MEMBER(has_selection)
  IPC_STRUCT_TRAITS_MEMBER(is_scripted)
  IPC_STRUCT_TRAITS_MEMBER(is_modifiable)
  IPC_STRUCT_TRAITS_MEMBER(margin_type)
IPC_STRUCT_TRAITS_END()

// Messages sent from the renderer to the browser.

// Sends back to the browser the rendered document that was requested by a
// PrintMsg_PrintPages message or from scripted printing. The memory handle in
// this message is already valid in the browser process. Waits until the
// document is complete ready before replying.
IPC_SYNC_MESSAGE_ROUTED1_1(PrintHostMsg_DidPrintDocument,
                           printing::mojom::DidPrintDocumentParams
                           /* page content */,
                           bool /* completed */)

// The renderer wants to update the current print settings with new
// |job_settings|.
IPC_SYNC_MESSAGE_ROUTED2_2(
    PrintHostMsg_UpdatePrintSettings,
    int /* document_cookie */,
    base::DictionaryValue /* job_settings */,
    printing::mojom::PrintPagesParams /* current_settings */,
    bool /* canceled */)

// It's the renderer that controls the printing process when it is generated
// by javascript. This step is about showing UI to the user to select the
// final print settings. The output parameter is the same as
// PrintMsg_PrintPages which is executed implicitly.
IPC_SYNC_MESSAGE_ROUTED1_1(PrintHostMsg_ScriptedPrint,
                           printing::mojom::ScriptedPrintParams,
                           printing::mojom::PrintPagesParams
                           /* settings chosen by the user*/)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Asks the browser to do print preview.
IPC_MESSAGE_ROUTED1(PrintHostMsg_RequestPrintPreview,
                    PrintHostMsg_RequestPrintPreview_Params /* params */)

// Notify the browser the about the to-be-rendered print preview document.
IPC_MESSAGE_ROUTED2(PrintHostMsg_DidStartPreview,
                    printing::mojom::DidStartPreviewParams /* params */,
                    printing::mojom::PreviewIds /* ids */)

// Notify the browser of preparing to print the document, for cases where
// the document will be collected from the individual pages instead of being
// provided by an extra metafile at end containing all pages.
IPC_MESSAGE_ROUTED2(PrintHostMsg_DidPrepareDocumentForPreview,
                    int /* document_cookie */,
                    printing::mojom::PreviewIds /* ids */)

// Notify the browser of the default page layout according to the currently
// selected printer and page size.
// |printable_area_in_points| Specifies the printable area in points.
// |has_custom_page_size_style| is true when the printing frame has a custom
// page size css otherwise false.
IPC_MESSAGE_ROUTED4(
    PrintHostMsg_DidGetDefaultPageLayout,
    printing::mojom::PageSizeMargins /* page layout in points */,
    gfx::Rect /* printable area in points */,
    bool /* has custom page size style */,
    printing::mojom::PreviewIds /* ids */)

// Notify the browser a print preview page has been rendered.
IPC_MESSAGE_ROUTED2(PrintHostMsg_DidPreviewPage,
                    printing::mojom::DidPreviewPageParams /* params */,
                    printing::mojom::PreviewIds /* ids */)

// Asks the browser whether the print preview has been cancelled.
IPC_SYNC_MESSAGE_ROUTED1_1(PrintHostMsg_CheckForCancel,
                           printing::mojom::PreviewIds /* ids */,
                           bool /* print preview cancelled */)

// Sends back to the browser the complete rendered document (non-draft mode,
// used for printing) that was requested by a PrintMsg_PrintPreview message.
// The memory handle in this message is already valid in the browser process.
IPC_MESSAGE_ROUTED2(PrintHostMsg_MetafileReadyForPrinting,
                    printing::mojom::DidPreviewDocumentParams /* params */,
                    printing::mojom::PreviewIds /* ids */)

// Run a nested run loop in the renderer until print preview for
// window.print() finishes.
IPC_SYNC_MESSAGE_ROUTED0_0(PrintHostMsg_SetupScriptedPrintPreview)

// Tell the browser to show the print preview, when the document is sufficiently
// loaded such that the renderer can determine whether it is modifiable or not.
IPC_MESSAGE_ROUTED1(PrintHostMsg_ShowScriptedPrintPreview,
                    bool /* is_modifiable */)
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#endif  // COMPONENTS_PRINTING_COMMON_PRINT_MESSAGES_H_
