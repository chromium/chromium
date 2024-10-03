// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_render_frame_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/types/fixed_array.h"
#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "components/printing/common/print_params.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_number.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/css/page_orientation.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "third_party/blink/public/mojom/page/prerender_page_param.mojom.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"
#include "third_party/blink/public/mojom/partitioned_popins/partitioned_popin_params.mojom.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_non_composited_widget_client.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

using blink::web_pref::WebPreferences;

namespace printing {

namespace {

#ifndef STATIC_ASSERT_ENUM
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)
#endif

enum PrintPreviewHelperEvents {
  PREVIEW_EVENT_REQUESTED,        // Received a request for a preview document.
  PREVIEW_EVENT_CACHE_HIT,        // Unused.
  PREVIEW_EVENT_CREATE_DOCUMENT,  // Started creating a preview document.
  PREVIEW_EVENT_NEW_SETTINGS,     // Unused.
  PREVIEW_EVENT_INITIATED,        // Initiated print preview.
  PREVIEW_EVENT_MAX,
};

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
bool g_is_preview_enabled = true;
#else
bool g_is_preview_enabled = false;
#endif

const char kPageLoadScriptFormat[] =
    "document.open(); document.write(%s); document.close();";

const char kPageSetupScriptFormat[] = "setupHeaderFooterTemplate(%s);";

constexpr int kAllowedIpcDepthForPrint = 1;

struct PageSizeMarginsWithOrientation {
  mojom::PageSizeMarginsPtr page_size_margins;
  mojom::PageOrientation page_orientation =
      printing::mojom::PageOrientation::kUpright;
};

// TODO(crbug.com/40822424): Remove this and related code when the bug is fixed.
enum class DebugEvent {
  kNone = 0,
  kPrintBegin1 = 1,
  kPrintBegin2 = 2,
  kPrintBegin3 = 3,
  kSetPrintSettings1 = 4,
  kSetPrintSettings2 = 5,
  kSetPrintSettings3 = 6,
  kSetPrintSettings4 = 7,
  kSetPrintSettings5 = 8,
  kSetPrintSettings6 = 9,
  kSetPrintSettings7 = 10,
  kSetPrintSettings8 = 11,
  kInitWithFrame1 = 12,
  kInitWithFrame2 = 13,
  kInitWithNode = 14,
  kRequestPrintPreviewScripted = 15,
  kRequestPrintPreviewUserInitiatedEntireFrame = 16,
  kRequestPrintPreviewUserInitiatedSelection = 17,
  kRequestPrintPreviewUserInitiatedContextNode = 18,
  kPrintPreviewForPlugin = 19,
  kPrintPreviewForNonPlugin = 20,
  kPrintPreviewIsModifiable = 21,
  kPrintPreviewIsNotModifiable = 22,
};

constexpr size_t kDebugEventMaxCount = 10;
size_t g_debug_events_index = 0;

base::FixedArray<DebugEvent>& GetDebugEvents() {
  static base::NoDestructor<base::FixedArray<DebugEvent>> debug_events(
      kDebugEventMaxCount);
  return *debug_events;
}

void RecordDebugEvent(DebugEvent event) {
  GetDebugEvents()[g_debug_events_index] = event;
  ++g_debug_events_index;
  g_debug_events_index %= kDebugEventMaxCount;
}

void ExecuteScript(blink::WebLocalFrame* frame,
                   const char* script_format,
                   const base::Value& parameters) {
  std::string json;
  base::JSONWriter::Write(parameters, &json);
  std::string script =
      base::StringPrintfNonConstexpr(script_format, json.c_str());
  frame->ExecuteScript(
      blink::WebScriptSource(blink::WebString::FromUTF8(script)));
}

int GetDPI(const mojom::PrintParams& print_params) {
#if BUILDFLAG(IS_APPLE)
  // On Mac, the printable area is in points, don't do any scaling based on DPI.
  return kPointsPerInch;
#else
  // Render using the higher of the two resolutions in both dimensions to
  // prevent bad quality print jobs on rectantular DPI printers.
  return static_cast<int>(
      std::max(print_params.dpi.width(), print_params.dpi.height()));
#endif  // BUILDFLAG(IS_APPLE)
}

// Helper function to check for center on page (and shrink the contents to fit,
// if needed). This is what's done when printing HTML to a printer (not when
// generating a PDF).
bool IsPrintScalingOptionCenterOnPaper(const mojom::PrintParams& params) {
  return params.print_scaling_option ==
         mojom::PrintScalingOption::kCenterShrinkToFitPaper;
}

bool ShouldIgnoreCssPageSize(bool ignore_css_margins,
                             const mojom::PrintParams& params) {
  return ignore_css_margins && IsPrintScalingOptionCenterOnPaper(params);
}

mojom::PageOrientation FromBlinkPageOrientation(
    blink::PageOrientation orientation) {
  switch (orientation) {
    case blink::PageOrientation::kUpright:
      return printing::mojom::PageOrientation::kUpright;
    case blink::PageOrientation::kRotateLeft:
      return printing::mojom::PageOrientation::kRotateLeft;
    case blink::PageOrientation::kRotateRight:
      return printing::mojom::PageOrientation::kRotateRight;
  }
}

blink::WebPrintPageDescription GetDefaultPageDescription(
    const mojom::PrintParams& page_params) {
  int dpi = GetDPI(page_params);

  blink::WebPrintPageDescription description;
  description.size.SetSize(
      ConvertUnitFloat(page_params.page_size.width(), dpi, kPixelsPerInch),
      ConvertUnitFloat(page_params.page_size.height(), dpi, kPixelsPerInch));
  description.margin_top =
      ConvertUnitFloat(page_params.margin_top, dpi, kPixelsPerInch);
  description.margin_right = ConvertUnitFloat(
      page_params.page_size.width() - page_params.content_size.width() -
          page_params.margin_left,
      dpi, kPixelsPerInch);
  description.margin_bottom = ConvertUnitFloat(
      page_params.page_size.height() - page_params.content_size.height() -
          page_params.margin_top,
      dpi, kPixelsPerInch);
  description.margin_left =
      ConvertUnitFloat(page_params.margin_left, dpi, kPixelsPerInch);

  return description;
}

mojom::PrintParamsPtr GetCssPrintParams(blink::WebLocalFrame* frame,
                                        uint32_t page_index,
                                        const mojom::PrintParams& page_params) {
  blink::WebPrintPageDescription description;
  if (frame) {
    description = frame->GetPageDescription(page_index);
  } else {
    description = GetDefaultPageDescription(page_params);
  }

  float new_content_width = description.size.width() - description.margin_left -
                            description.margin_right;
  float new_content_height = description.size.height() -
                             description.margin_top - description.margin_bottom;
  DCHECK_GT(new_content_width, 0.0f);
  DCHECK_GT(new_content_height, 0.0f);

  mojom::PrintParamsPtr page_css_params = page_params.Clone();
  page_css_params->page_orientation =
      FromBlinkPageOrientation(description.orientation);

  int dpi = GetDPI(page_params);
  page_css_params->page_size = gfx::SizeF(
      ConvertUnitFloat(description.size.width(), kPixelsPerInch, dpi),
      ConvertUnitFloat(description.size.height(), kPixelsPerInch, dpi));
  page_css_params->content_size =
      gfx::SizeF(ConvertUnitFloat(new_content_width, kPixelsPerInch, dpi),
                 ConvertUnitFloat(new_content_height, kPixelsPerInch, dpi));

  page_css_params->margin_top =
      ConvertUnitFloat(description.margin_top, kPixelsPerInch, dpi);
  page_css_params->margin_left =
      ConvertUnitFloat(description.margin_left, kPixelsPerInch, dpi);
  return page_css_params;
}

mojom::PageSizeMarginsPtr CalculatePageLayoutFromPrintParams(
    const mojom::PrintParams& params) {
  float content_width = params.content_size.width();
  float content_height = params.content_size.height();

  float margin_bottom =
      params.page_size.height() - content_height - params.margin_top;
  float margin_right =
      params.page_size.width() - content_width - params.margin_left;

  return mojom::PageSizeMargins::New(content_width, content_height,
                                     params.margin_top, margin_right,
                                     margin_bottom, params.margin_left);
}

mojom::PageSizeMarginsPtr ConvertedPageSizeMargins(
    const mojom::PageSizeMarginsPtr& orig_page_layout,
    float old_unit,
    float new_unit) {
  mojom::PageSizeMarginsPtr page_layout = orig_page_layout.Clone();
  page_layout->content_width =
      ConvertUnitFloat(page_layout->content_width, old_unit, new_unit);
  page_layout->content_height =
      ConvertUnitFloat(page_layout->content_height, old_unit, new_unit);
  page_layout->margin_top =
      ConvertUnitFloat(page_layout->margin_top, old_unit, new_unit);
  page_layout->margin_right =
      ConvertUnitFloat(page_layout->margin_right, old_unit, new_unit);
  page_layout->margin_bottom =
      ConvertUnitFloat(page_layout->margin_bottom, old_unit, new_unit);
  page_layout->margin_left =
      ConvertUnitFloat(page_layout->margin_left, old_unit, new_unit);

  return page_layout;
}

blink::WebPrintParams ComputeWebKitPrintParamsInDesiredDpi(
    const mojom::PrintParams& print_params,
    bool source_is_pdf,
    bool ignore_css_margins) {
  blink::WebPrintParams webkit_print_params;
  int dpi = GetDPI(print_params);
  webkit_print_params.printer_dpi = dpi;
  webkit_print_params.scale_factor = print_params.scale_factor;

  webkit_print_params.ignore_css_margins = ignore_css_margins;
  webkit_print_params.ignore_page_size =
      ShouldIgnoreCssPageSize(ignore_css_margins, print_params);

  if (source_is_pdf) {
#if BUILDFLAG(IS_APPLE)
    // For Mac, GetDPI() returns a value that avoids DPI-based scaling. This is
    // correct except when rastering PDFs, which uses |printer_dpi|, and the
    // value for |printer_dpi| is too low. Adjust that here.
    // See https://crbug.com/943462
    webkit_print_params.printer_dpi = kDefaultPdfDpi;
#endif

    if (print_params.rasterize_pdf && print_params.rasterize_pdf_dpi > 0)
      webkit_print_params.printer_dpi = print_params.rasterize_pdf_dpi;
  }
  webkit_print_params.rasterize_pdf = print_params.rasterize_pdf;
  webkit_print_params.print_scaling_option = print_params.print_scaling_option;

  webkit_print_params.printable_area_in_css_pixels = gfx::RectF(
      ConvertUnitFloat(print_params.printable_area.x(), dpi, kPixelsPerInch),
      ConvertUnitFloat(print_params.printable_area.y(), dpi, kPixelsPerInch),
      ConvertUnitFloat(print_params.printable_area.width(), dpi,
                       kPixelsPerInch),
      ConvertUnitFloat(print_params.printable_area.height(), dpi,
                       kPixelsPerInch));

  // The following settings is for N-up mode.
  webkit_print_params.pages_per_sheet = print_params.pages_per_sheet;

  webkit_print_params.default_page_description =
      GetDefaultPageDescription(print_params);

  return webkit_print_params;
}

bool IsPrintingPdfFrame(blink::WebLocalFrame* frame,
                        const blink::WebNode& node) {
  blink::WebPlugin* plugin = frame->GetPluginToPrint(node);
  return plugin && plugin->SupportsPaginatedPrint();
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
bool IsPrintToPdfRequested(const base::Value::Dict& job_settings) {
  mojom::PrinterType type = static_cast<mojom::PrinterType>(
      job_settings.FindInt(kSettingPrinterType).value());
  return type == mojom::PrinterType::kPdf;
}

void GetPageSizeAndOrientationInfo(blink::WebLocalFrame* frame,
                                   uint32_t total_page_count,
                                   bool* all_pages_have_custom_size,
                                   bool* all_pages_have_custom_orientation) {
  *all_pages_have_custom_size = true;
  *all_pages_have_custom_orientation = true;
  if (!frame) {
    return;
  }
  // See if there are pages in the document whose size or orientation may be
  // controlled by the UI. If all pages specify size or orientation (via CSS),
  // the respective options in the print preview UI should be hidden (since they
  // will have no effect).
  for (uint32_t i = 0; i < total_page_count; ++i) {
    auto page_size_type = frame->GetPageDescription(i).page_size_type;
    // A "fixed" page size implies that both page size and orientation are set,
    // also when well-known page sizes (such as A4) are specified.
    if (page_size_type != blink::PageSizeType::kFixed) {
      // We found a page that doesn't specify the size.
      *all_pages_have_custom_size = false;
      if (page_size_type == blink::PageSizeType::kAuto) {
        // We found a page that also doesn't specify the orientation. We can
        // stop searching. This document has at least one page that should be
        // fully customizable by the user via the print preview UI.
        *all_pages_have_custom_orientation = false;
        break;
      }
    }
  }
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ENABLE_PRINTING)
// Disable scaling when either:
// - The PDF specifies disabling scaling.
// - All the pages in the PDF are the same size,
// - |ignore_page_size| is false and the uniform size is the same as the paper
//   size.
bool PDFShouldDisableScalingBasedOnPreset(
    const blink::WebPrintPresetOptions& options,
    const mojom::PrintParams& params,
    bool ignore_page_size) {
  if (options.is_scaling_disabled)
    return true;

  if (!options.uniform_page_size.has_value())
    return false;

  int dpi = GetDPI(params);
  if (!dpi) {
    // Likely |params| is invalid, in which case the return result does not
    // matter. Check for this so ConvertUnit() does not divide by zero.
    return true;
  }

  if (ignore_page_size)
    return false;

  gfx::Size page_size(
      ConvertUnit(params.page_size.width(), dpi, kPointsPerInch),
      ConvertUnit(params.page_size.height(), dpi, kPointsPerInch));
  return options.uniform_page_size == page_size;
}

bool PDFShouldDisableScaling(blink::WebLocalFrame* frame,
                             const blink::WebNode& node,
                             const mojom::PrintParams& params,
                             bool ignore_page_size) {
  const bool kDefaultPDFShouldDisableScalingSetting = true;
  blink::WebPrintPresetOptions preset_options;
  if (!frame->GetPrintPresetOptionsForPlugin(node, &preset_options))
    return kDefaultPDFShouldDisableScalingSetting;
  return PDFShouldDisableScalingBasedOnPreset(preset_options, params,
                                              ignore_page_size);
}
#endif

mojom::MarginType GetMarginsForPdf(blink::WebLocalFrame* frame,
                                   const blink::WebNode& node,
                                   const mojom::PrintParams& params) {
  return PDFShouldDisableScaling(frame, node, params, false)
             ? mojom::MarginType::kNoMargins
             : mojom::MarginType::kPrintableAreaMargins;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
gfx::SizeF GetPdfPageSize(const gfx::SizeF& page_size, int dpi) {
  return gfx::SizeF(ConvertUnitFloat(page_size.width(), dpi, kPointsPerInch),
                    ConvertUnitFloat(page_size.height(), dpi, kPointsPerInch));
}

ScalingType ScalingTypeFromJobSettings(const base::Value::Dict& job_settings) {
  return static_cast<ScalingType>(
      job_settings.FindInt(kSettingScalingType).value());
}

// Returns the print scaling option to retain/scale/crop the source page size
// to fit the printable area of the paper.
//
// We retain the source page size when the current destination printer is
// SAVE_AS_PDF.
//
// We crop the source page size to fit the printable area or we print only the
// left top page contents when
// (1) Source is PDF and the user has requested not to fit to printable area
// via |job_settings|.
// (2) Source is PDF. This is the first preview request and print scaling
// option is disabled for initiator renderer plugin.
//
// In all other cases, we scale the source page to fit the printable area.
mojom::PrintScalingOption GetPrintScalingOption(
    blink::WebLocalFrame* frame,
    const blink::WebNode& node,
    bool source_is_html,
    const base::Value::Dict& job_settings,
    const mojom::PrintParams& params) {
  if (params.print_to_pdf)
    return mojom::PrintScalingOption::kSourceSize;

  if (!source_is_html) {
    ScalingType scaling_type = ScalingTypeFromJobSettings(job_settings);
    // The following conditions are ordered for an optimization that avoids
    // calling PDFShouldDisableScaling(), which has to make a call using PPAPI.
    if (scaling_type == DEFAULT || scaling_type == CUSTOM)
      return mojom::PrintScalingOption::kNone;
    if (params.is_first_request &&
        PDFShouldDisableScaling(frame, node, params, true)) {
      return mojom::PrintScalingOption::kNone;
    }
    if (scaling_type == FIT_TO_PAPER)
      return mojom::PrintScalingOption::kFitToPaper;
    return mojom::PrintScalingOption::kFitToPrintableArea;
  }
  return mojom::PrintScalingOption::kCenterShrinkToFitPaper;
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

// Get page layout and orientation. The layout is in device pixels.
PageSizeMarginsWithOrientation ComputePageLayoutForCss(
    blink::WebLocalFrame* frame,
    uint32_t page_index,
    const mojom::PrintParams& page_params,
    bool ignore_css_margins) {
  mojom::PrintParamsPtr css_params =
      GetCssPrintParams(frame, page_index, page_params);
  return {CalculatePageLayoutFromPrintParams(*css_params),
          css_params->page_orientation};
}

bool CopyMetafileDataToReadOnlySharedMem(
    const MetafileSkia& metafile,
    base::ReadOnlySharedMemoryRegion* region) {
  uint32_t buf_size = metafile.GetDataSize();
  if (buf_size == 0)
    return false;

  TRACE_EVENT1("print", "CopyMetafileDataToReadOnlySharedMem", "size",
               buf_size);

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(buf_size);
  if (!region_mapping.IsValid())
    return false;

  if (!metafile.GetData(region_mapping.mapping.memory(), buf_size))
    return false;

  *region = std::move(region_mapping.region);
  return true;
}

bool CopyMetafileDataToDidPrintContentParams(
    const MetafileSkia& metafile,
    mojom::DidPrintContentParams* params) {
  base::ReadOnlySharedMemoryRegion region;
  if (!CopyMetafileDataToReadOnlySharedMem(metafile, &region))
    return false;

  params->metafile_data_region = std::move(region);
  params->subframe_content_info = metafile.GetSubframeContentInfo();
  return true;
}

// Given the `canvas` to draw on, prints the appropriate headers and footers on
// the canvas using `frame`, with data from the remaining parameters.
void PrintHeaderAndFooter(cc::PaintCanvas* canvas,
                          blink::WebLocalFrame& frame,
                          uint32_t page_index,
                          uint32_t total_pages,
                          const blink::WebLocalFrame& source_frame,
                          const mojom::PageSizeMargins& page_layout,
                          const mojom::PrintParams& params) {
  DCHECK_LE(total_pages, kMaxPageCount);
  DCHECK_LT(page_index, kMaxPageCount);

  base::Value html(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PRINT_HEADER_FOOTER_TEMPLATE_PAGE));
  // Load page with script to avoid async operations.
  ExecuteScript(&frame, kPageLoadScriptFormat, html);

  const gfx::SizeF page_size(
      page_layout.margin_left + page_layout.margin_right +
          page_layout.content_width,
      page_layout.margin_top + page_layout.margin_bottom +
          page_layout.content_height);

  base::Value::Dict options;
  options.Set(kSettingHeaderFooterDate,
              base::Time::Now().InMillisecondsFSinceUnixEpoch());
  options.Set("width", static_cast<double>(page_size.width()));
  options.Set("height", static_cast<double>(page_size.height()));
  options.Set("topMargin", page_layout.margin_top);
  options.Set("bottomMargin", page_layout.margin_bottom);
  options.Set("leftMargin", page_layout.margin_left);
  options.Set("rightMargin", page_layout.margin_right);
  // `page_index` is 0-based, so 1 is added to get the page number.
  options.Set("pageNumber", base::checked_cast<int>(page_index + 1));
  options.Set("totalPages", base::checked_cast<int>(total_pages));
  options.Set("url", params.url);
  std::u16string title = source_frame.GetDocument().Title().Utf16();
  options.Set("title", title.empty() ? params.title : title);
  options.Set("headerTemplate", params.header_template);
  options.Set("footerTemplate", params.footer_template);
  options.Set("isRtl", base::i18n::IsRTL());

  ExecuteScript(&frame, kPageSetupScriptFormat,
                base::Value(std::move(options)));

  blink::WebPrintParams webkit_params(page_size);
  webkit_params.printer_dpi = GetDPI(params);

  // Avoid fragmentation. Everything (header + footer) should fit on one
  // page. Anything that partially overflows the page should be clipped rather
  // than pushed to a next page that is never to be seen. This may happen for
  // custom headers and (especially) footers.
  webkit_params.use_paginated_layout = false;

  RecordDebugEvent(DebugEvent::kPrintBegin1);
  frame.PrintBegin(webkit_params, blink::WebNode());
  frame.PrintPage(0, canvas);
  frame.PrintEnd();
}

// Renders page contents from `frame` to `content_area` of `canvas`.
// `page_index` is zero-based.
void RenderPageContent(blink::WebLocalFrame* frame,
                       uint32_t page_index,
                       cc::PaintCanvas* canvas) {
  TRACE_EVENT1("print", "RenderPageContent", "page_index", page_index);
  frame->PrintPage(page_index, canvas);
}

class HeaderAndFooterContext {
 public:
  class HeaderAndFooterClient final : public blink::WebLocalFrameClient {
   public:
    // WebLocalFrameClient:
    void BindToFrame(blink::WebNavigationControl* frame) override {
      frame_ = frame;
    }
    void FrameDetached() override {
      frame_->Close();
      frame_ = nullptr;
    }

   private:
    raw_ptr<blink::WebNavigationControl> frame_ = nullptr;
  };

  explicit HeaderAndFooterContext(const blink::WebLocalFrame& source_frame)
      : web_view_(CreateWebView(source_frame)), frame_(CreateFrame()) {
    CHECK(web_view_);
    CHECK(frame_);
    InitWebView();
  }
  ~HeaderAndFooterContext() { web_view_->Close(); }

  blink::WebLocalFrame* frame() { return frame_; }

 private:
  static blink::WebView* CreateWebView(
      const blink::WebLocalFrame& source_frame) {
    auto* view = blink::WebView::Create(
        /*client=*/nullptr,
        /*is_hidden=*/false,
        /*prerender_param=*/nullptr,
        /*fenced_frame_mode=*/std::nullopt,
        /*compositing_enabled=*/false, /*widgets_never_composited=*/false,
        /*opener=*/nullptr, mojo::NullAssociatedReceiver(),
        *source_frame.GetAgentGroupScheduler(),
        /*session_storage_namespace_id=*/std::string(),
        /*page_base_background_color=*/std::nullopt,
        blink::BrowsingContextGroupInfo::CreateUnique(),
        /*color_provider_colors=*/nullptr,
        /*partitioned_popin_params=*/nullptr);
    view->GetSettings()->SetJavaScriptEnabled(true);
    return view;
  }

  blink::WebLocalFrame* CreateFrame() {
    return blink::WebLocalFrame::CreateMainFrame(
        web_view_, &frame_client_, nullptr, mojo::NullRemote(),
        blink::LocalFrameToken(), blink::DocumentToken(), nullptr);
  }

  void InitWebView() {
    mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget;
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
        frame_widget_receiver =
            frame_widget.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> frame_widget_host;
    std::ignore = frame_widget_host.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::Widget> widget_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::Widget> widget_receiver =
        widget_remote.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::WidgetHost> widget_host_remote;
    std::ignore = widget_host_remote.BindNewEndpointAndPassDedicatedReceiver();

    blink::WebFrameWidget* web_frame_widget = frame_->InitializeFrameWidget(
        frame_widget_host.Unbind(), std::move(frame_widget_receiver),
        widget_host_remote.Unbind(), std::move(widget_receiver),
        viz::FrameSinkId());
    web_frame_widget->InitializeNonCompositing(&widget_client_);
    web_view_->DidAttachLocalMainFrame();
  }

  HeaderAndFooterClient frame_client_;
  blink::WebNonCompositedWidgetClient widget_client_;
  const raw_ptr<blink::WebView, DanglingUntriaged> web_view_;
  const raw_ptr<blink::WebLocalFrame> frame_;
};

}  // namespace

FrameReference::FrameReference(blink::WebLocalFrame* frame) {
  Reset(frame);
}

FrameReference::FrameReference() {
  Reset(nullptr);
}

FrameReference::~FrameReference() = default;

void FrameReference::Reset(blink::WebLocalFrame* frame) {
  if (frame) {
    view_ = frame->View();
    // Make sure this isn't called too early in the |frame| lifecycle... i.e.
    // calling this in WebLocalFrameClient::BindToFrame() doesn't work.
    // TODO(dcheng): It's a bit awkward that lifetime details like this leak out
    // of Blink. Fixing https://crbug.com/727166 should allow this to be
    // addressed.
    DCHECK(view_);
    frame_ = frame;
  } else {
    view_ = nullptr;
    frame_ = nullptr;
  }
}

blink::WebLocalFrame* FrameReference::GetFrame() {
  if (!view_ || !frame_) {
    return nullptr;
  }
  for (blink::WebFrame* frame = view_->MainFrame(); frame;
       frame = frame->TraverseNext()) {
    if (frame == frame_) {
      return frame_;
    }
  }
  return nullptr;
}

blink::WebView* FrameReference::view() {
  return view_;
}

ClosuresForMojoResponse::ClosuresForMojoResponse() = default;

ClosuresForMojoResponse::~ClosuresForMojoResponse() {
  RunScriptedPrintPreviewQuitClosure();
}

void ClosuresForMojoResponse::SetScriptedPrintPreviewQuitClosure(
    base::OnceClosure quit_print_preview) {
  DCHECK(!scripted_print_preview_quit_closure_);
  scripted_print_preview_quit_closure_ = std::move(quit_print_preview);
}

bool ClosuresForMojoResponse::HasScriptedPrintPreviewQuitClosure() const {
  return !scripted_print_preview_quit_closure_.is_null();
}

void ClosuresForMojoResponse::RunScriptedPrintPreviewQuitClosure() {
  if (!scripted_print_preview_quit_closure_) {
    return;
  }

  std::move(scripted_print_preview_quit_closure_).Run();
}

// Class that calls the Begin and End print functions on the frame and changes
// the size of the view temporarily to support full page printing..
class PrepareFrameAndViewForPrint : public blink::WebViewClient,
                                    public blink::WebNonCompositedWidgetClient,
                                    public blink::WebLocalFrameClient {
 public:
  PrepareFrameAndViewForPrint(blink::WebLocalFrame* frame,
                              const blink::WebNode& node);
  PrepareFrameAndViewForPrint(const PrepareFrameAndViewForPrint&) = delete;
  PrepareFrameAndViewForPrint& operator=(const PrepareFrameAndViewForPrint&) =
      delete;
  ~PrepareFrameAndViewForPrint() override;

  // Begin printing and generate print layout. Replaces `frame_` with selection
  // if needed. Will call `on_ready` when completed. This may or may not happen
  // asynchronously.
  void BeginPrinting(const WebPreferences& preferences,
                     const mojom::PrintParams& params,
                     bool ignore_css_margins,
                     base::OnceClosure on_ready);

  // Prepare the frame for printing. Enter print mode and compute print layout.
  // May only be called if what's currently in `frame_` is what's going to be
  // printed. Otherwise, use `BeginPrinting()` instead, to also support printing
  // the currently selection.
  void EnterPrintMode(const mojom::PrintParams& params,
                      bool ignore_css_margins);

  blink::WebLocalFrame* frame() { return frame_.GetFrame(); }

  const blink::WebNode& node() const { return node_to_print_; }

  uint32_t GetPageCount() const { return page_count_; }

  void FinishPrinting();

  bool IsLoadingSelection() {
    // It's not selection if not |owns_web_view_|.
    return owns_web_view_ && frame() && frame()->IsLoading();
  }

 private:
  void EnterPrintModeInternal(const mojom::PrintParams& params,
                              bool ignore_css_margins);

  // blink::WebViewClient:
  void DidStopLoading() override;

  // blink::WebLocalFrameClient:
  void BindToFrame(blink::WebNavigationControl* frame) override;
  blink::WebLocalFrame* CreateChildFrame(
      blink::mojom::TreeScopeType scope,
      const blink::WebString& name,
      const blink::WebString& fallback_name,
      const blink::FramePolicy& frame_policy,
      const blink::WebFrameOwnerProperties& frame_owner_properties,
      blink::FrameOwnerElementType owner_type,
      blink::WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      FinishChildFrameCreationFn finish_creation) override;
  void FrameDetached() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

  void CallOnReady();
  void CopySelection(const mojom::PrintParams& params,
                     const WebPreferences& preferences);

  FrameReference frame_;
  FrameReference original_frame_;
  raw_ptr<blink::WebNavigationControl> navigation_control_ = nullptr;
  blink::WebNode node_to_print_;
  bool owns_web_view_ = false;
  mojom::PrintParamsPtr selection_only_print_params_;
  uint32_t page_count_ = 0;
  base::OnceClosure on_ready_;
  bool is_printing_started_ = false;
  const raw_ref<blink::scheduler::WebAgentGroupScheduler>
      agent_group_scheduler_;

  base::WeakPtrFactory<PrepareFrameAndViewForPrint> weak_ptr_factory_{this};
};

PrepareFrameAndViewForPrint::PrepareFrameAndViewForPrint(
    blink::WebLocalFrame* frame,
    const blink::WebNode& node)
    : frame_(frame),
      original_frame_(frame),
      node_to_print_(node),
      agent_group_scheduler_(*frame->GetAgentGroupScheduler()) {}

PrepareFrameAndViewForPrint::~PrepareFrameAndViewForPrint() {
  FinishPrinting();
}

void PrepareFrameAndViewForPrint::EnterPrintModeInternal(
    const mojom::PrintParams& params,
    bool ignore_css_margins) {
  bool is_pdf = IsPrintingPdfFrame(frame(), node_to_print_);
  blink::WebPrintParams web_print_params =
      ComputeWebKitPrintParamsInDesiredDpi(params, is_pdf, ignore_css_margins);
  blink::WebView* web_view = frame()->View();
  web_view->GetSettings()->SetShouldPrintBackgrounds(
      params.should_print_backgrounds);
  RecordDebugEvent(DebugEvent::kPrintBegin2);
  page_count_ = frame()->PrintBegin(web_print_params, node_to_print_);
  is_printing_started_ = true;
}

void PrepareFrameAndViewForPrint::BeginPrinting(
    const WebPreferences& preferences,
    const mojom::PrintParams& params,
    bool ignore_css_margins,
    base::OnceClosure on_ready) {
  on_ready_ = std::move(on_ready);
  if (params.selection_only) {
    // Printing selection not an option for PDF.
    DCHECK(!IsPrintingPdfFrame(frame(), node_to_print_));

    // Save the parameters. Will be used when the document has loaded the copied
    // selection.
    selection_only_print_params_ = params.Clone();

    CopySelection(params, preferences);
  } else {
    EnterPrintModeInternal(params, ignore_css_margins);

    // Call immediately, async call crashes scripting printing.
    CallOnReady();
  }
}

void PrepareFrameAndViewForPrint::EnterPrintMode(
    const mojom::PrintParams& params,
    bool ignore_css_margins) {
  // Printing the selection isn't allowed here. Use `BeginPrinting()` instead.
  DCHECK(!params.selection_only);

  EnterPrintModeInternal(params, ignore_css_margins);
}

void PrepareFrameAndViewForPrint::CopySelection(
    const mojom::PrintParams& params,
    const WebPreferences& preferences) {
  std::string html = frame()->SelectionAsMarkup().Utf8();

  // Save the base URL before `frame_` gets reset below.
  GURL original_base_url = frame()->GetDocument().BaseURL();

  // Create a new WebView with the same settings as the current display one.
  // Except that we disable javascript (don't want any active content running
  // on the page).
  WebPreferences prefs = preferences;
  prefs.javascript_enabled = false;

  blink::WebView* web_view = blink::WebView::Create(
      /*client=*/this,
      /*is_hidden=*/false,
      /*prerender_param=*/nullptr,
      /*fenced_frame_mode=*/std::nullopt,
      /*compositing_enabled=*/false,
      /*widgets_never_composited=*/false,
      /*opener=*/nullptr, mojo::NullAssociatedReceiver(),
      *agent_group_scheduler_,
      /*session_storage_namespace_id=*/std::string(),
      /*page_base_background_color=*/std::nullopt,
      blink::BrowsingContextGroupInfo::CreateUnique(),
      /*color_provider_colors=*/nullptr,
      /*partitioned_popin_params=*/nullptr);
  blink::WebView::ApplyWebPreferences(prefs, web_view);
  blink::WebLocalFrame* main_frame = blink::WebLocalFrame::CreateMainFrame(
      web_view, this, nullptr, mojo::NullRemote(), blink::LocalFrameToken(),
      blink::DocumentToken(), nullptr);
  frame_.Reset(main_frame);
  mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
      frame_widget_receiver =
          frame_widget.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> frame_widget_host;
  std::ignore = frame_widget_host.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::Widget> widget_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::Widget> widget_receiver =
      widget_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::WidgetHost> widget_host_remote;
  std::ignore = widget_host_remote.BindNewEndpointAndPassDedicatedReceiver();

  blink::WebFrameWidget* main_frame_widget = main_frame->InitializeFrameWidget(
      frame_widget_host.Unbind(), std::move(frame_widget_receiver),
      widget_host_remote.Unbind(), std::move(widget_receiver),
      viz::FrameSinkId());
  main_frame_widget->InitializeNonCompositing(this);

  web_view->DidAttachLocalMainFrame();
  node_to_print_.Reset();

  owns_web_view_ = true;

  // When loading is done this will call didStopLoading() and that will do the
  // actual printing.
  auto web_navigation_params = std::make_unique<blink::WebNavigationParams>();
  // Use the original base URL as the new frame's base url, so relative links
  // can stay as such. Also, set the new frame's url to about:blank as this
  // doesn't have the risks of loading a subframe-only URL like about:srcdoc in
  // a main frame, and it doesn't pose a cross-origin concern like the base URL.
  web_navigation_params->url = GURL(url::kAboutBlankURL);
  web_navigation_params->fallback_base_url = original_base_url;
  blink::WebNavigationParams::FillStaticResponse(
      web_navigation_params.get(), "text/html", "UTF-8", std::move(html));
  navigation_control_->CommitNavigation(std::move(web_navigation_params),
                                        /*extra_data=*/nullptr);
}

void PrepareFrameAndViewForPrint::DidStopLoading() {
  DCHECK(!on_ready_.is_null());

  // The new document (with the selection) has loaded. Now print it.
  EnterPrintModeInternal(*selection_only_print_params_,
                         /*ignore_css_margins=*/false);

  // Don't call callback here, because it can delete `this` and WebView that is
  // called didStopLoading.
  frame()
      ->GetTaskRunner(blink::TaskType::kInternalDefault)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&PrepareFrameAndViewForPrint::CallOnReady,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PrepareFrameAndViewForPrint::BindToFrame(
    blink::WebNavigationControl* navigation_control) {
  navigation_control_ = navigation_control;
}

blink::WebLocalFrame* PrepareFrameAndViewForPrint::CreateChildFrame(
    blink::mojom::TreeScopeType scope,
    const blink::WebString& name,
    const blink::WebString& fallback_name,
    const blink::FramePolicy& frame_policy,
    const blink::WebFrameOwnerProperties& frame_owner_properties,
    blink::FrameOwnerElementType frame_owner_type,
    blink::WebPolicyContainerBindParams policy_container_bind_params,
    ukm::SourceId document_ukm_source_id,
    FinishChildFrameCreationFn finish_creation) {
  // This is called when printing a selection and when this selection contains
  // an iframe. This is not supported yet. An empty rectangle will be displayed
  // instead.
  // Please see: https://crbug.com/732780.
  return nullptr;
}

void PrepareFrameAndViewForPrint::FrameDetached() {
  blink::WebLocalFrame* frame = frame_.GetFrame();
  DCHECK(frame);
  frame->Close();
  navigation_control_ = nullptr;
  frame_.Reset(nullptr);
}

scoped_refptr<network::SharedURLLoaderFactory>
PrepareFrameAndViewForPrint::GetURLLoaderFactory() {
  blink::WebLocalFrame* frame = original_frame_.GetFrame();
  return frame->Client()->GetURLLoaderFactory();
}

void PrepareFrameAndViewForPrint::CallOnReady() {
  if (on_ready_)
    std::move(on_ready_).Run();  // Can delete `this`.
}

void PrepareFrameAndViewForPrint::FinishPrinting() {
  TRACE_EVENT0("print", "PrepareFrameAndViewForPrint::FinishPrinting");

  blink::WebLocalFrame* frame = frame_.GetFrame();
  if (frame) {
    blink::WebView* web_view = frame->View();
    if (is_printing_started_) {
      is_printing_started_ = false;
      if (!owns_web_view_) {
        web_view->GetSettings()->SetShouldPrintBackgrounds(false);
      }

      const auto& debug_events = GetDebugEvents();
      DebugEvent debug_event_alias0 = debug_events[0];
      DebugEvent debug_event_alias1 = debug_events[1];
      DebugEvent debug_event_alias2 = debug_events[2];
      DebugEvent debug_event_alias3 = debug_events[3];
      DebugEvent debug_event_alias4 = debug_events[4];
      DebugEvent debug_event_alias5 = debug_events[5];
      DebugEvent debug_event_alias6 = debug_events[6];
      DebugEvent debug_event_alias7 = debug_events[7];
      DebugEvent debug_event_alias8 = debug_events[8];
      DebugEvent debug_event_alias9 = debug_events[9];
      size_t debug_event_index = g_debug_events_index;
      base::debug::Alias(&debug_event_alias0);
      base::debug::Alias(&debug_event_alias1);
      base::debug::Alias(&debug_event_alias2);
      base::debug::Alias(&debug_event_alias3);
      base::debug::Alias(&debug_event_alias4);
      base::debug::Alias(&debug_event_alias5);
      base::debug::Alias(&debug_event_alias6);
      base::debug::Alias(&debug_event_alias7);
      base::debug::Alias(&debug_event_alias8);
      base::debug::Alias(&debug_event_alias9);
      base::debug::Alias(&debug_event_index);

      frame->PrintEnd();
    }
    if (owns_web_view_) {
      DCHECK(!frame->IsLoading());
      owns_web_view_ = false;
      web_view->Close();
    }
  }
  navigation_control_ = nullptr;
  frame_.Reset(nullptr);
  on_ready_.Reset();
}

bool PrintRenderFrameHelper::Delegate::IsScriptedPrintEnabled() {
  return true;
}

bool PrintRenderFrameHelper::Delegate::ShouldGenerateTaggedPDF() {
  return false;
}

PrintRenderFrameHelper::PrintRenderFrameHelper(
    content::RenderFrame* render_frame,
    std::unique_ptr<Delegate> delegate)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<PrintRenderFrameHelper>(render_frame),
      delegate_(std::move(delegate)),
      closures_for_mojo_responses_(
          base::MakeRefCounted<ClosuresForMojoResponse>()) {
  if (!delegate_->IsPrintPreviewEnabled()) {
    g_is_preview_enabled = false;
  }

  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::PrintRenderFrame>(base::BindRepeating(
          &PrintRenderFrameHelper::BindPrintRenderFrameReceiver,
          weak_ptr_factory_.GetWeakPtr()));
}

PrintRenderFrameHelper::~PrintRenderFrameHelper() = default;

const mojo::AssociatedRemote<mojom::PrintManagerHost>&
PrintRenderFrameHelper::GetPrintManagerHost() {
  // We should not make calls back to the host while handling PrintWithParams().
  DCHECK(!print_with_params_callback_);

  if (!print_manager_host_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &print_manager_host_);
    // Makes sure that it quits the runloop that runs while a Mojo call waits
    // for a reply if |print_manager_host_| is disconnected before the reply.
    print_manager_host_.set_disconnect_handler(
        base::BindOnce(&PrintRenderFrameHelper::QuitScriptedPrintPreviewRunLoop,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return print_manager_host_;
}

void PrintRenderFrameHelper::SetWebDocumentCollectionCallbackForTest(
    PreviewDocumentTestCallback callback) {
  CHECK_IS_TEST();
  preview_document_test_callback_ = std::move(callback);
}

bool PrintRenderFrameHelper::IsScriptInitiatedPrintAllowed(
    blink::WebLocalFrame* frame,
    bool user_initiated) {
  if (!delegate_->IsScriptedPrintEnabled())
    return false;

  bool printing_enabled = false;
  GetPrintManagerHost()->IsPrintingEnabled(&printing_enabled);
  if (!printing_enabled)
    return false;

  // If preview is enabled, then the print dialog is tab modal, and the user
  // can always close the tab on a mis-behaving page (the system print dialog
  // is app modal). If the print was initiated through user action, don't
  // throttle. Or, if the command line flag to skip throttling has been set.
  return user_initiated || g_is_preview_enabled ||
         scripting_throttler_.IsAllowed(frame);
}

void PrintRenderFrameHelper::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
  is_loading_ = true;
}

void PrintRenderFrameHelper::DidFailProvisionalLoad() {
  DidFinishLoad();
}

void PrintRenderFrameHelper::DidFinishLoad() {
  is_loading_ = false;
  if (!on_stop_loading_closure_.is_null())
    std::move(on_stop_loading_closure_).Run();
}

void PrintRenderFrameHelper::DidFinishLoadForPrinting() {
  DidFinishLoad();
}

void PrintRenderFrameHelper::ScriptedPrint(bool user_initiated) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  if (!IsScriptInitiatedPrintAllowed(web_frame, user_initiated))
    return;

  if (delegate_->OverridePrint(web_frame))
    return;

  // Detached documents can't be printed.
  if (!web_frame->GetDocument().GetFrame())
    return;

  if (print_in_progress_) {
    return;
  }

  if (g_is_preview_enabled) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    print_in_progress_ = true;
    RecordDebugEvent(DebugEvent::kInitWithFrame1);
    print_preview_context_.InitWithFrame(web_frame);
    RequestPrintPreview(PrintPreviewRequestType::kScripted,
                        /*already_notified_frame=*/false);
    // Print Preview resets `print_in_progress_` when the dialog closes.
    return;
#else
    NOTREACHED();
#endif
  }

  print_in_progress_ = true;

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  web_frame->DispatchBeforePrintEvent(/*print_client=*/nullptr);
  if (!weak_this) {
    return;
  }

  Print(web_frame, blink::WebNode(), PrintRequestType::kScripted);
  if (!weak_this) {
    return;
  }

  web_frame->DispatchAfterPrintEvent();
  if (!weak_this)
    return;

  print_in_progress_ = false;
}

void PrintRenderFrameHelper::WillBeDestroyed() {
  // TODO(crbug.com/40094746): Handle unpausing here when PrintRenderFrameHelper
  // can safely pause/unpause pages.
  receivers_.Clear();
}

void PrintRenderFrameHelper::OnDestruct() {
  if (ipc_nesting_level_ > 0) {
    render_frame_gone_ = true;
    return;
  }
  delete this;
}

void PrintRenderFrameHelper::BindPrintRenderFrameReceiver(
    mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PrintRenderFrameHelper::PrintRequestedPages() {
  PrintRequestedPagesInternal(/*already_notified_frame=*/false);
}

void PrintRenderFrameHelper::PrintRequestedPagesInternal(
    bool already_notified_frame) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > kAllowedIpcDepthForPrint) {
    return;
  }

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  if (!already_notified_frame) {
    frame->DispatchBeforePrintEvent(/*print_client=*/nullptr);
    // Don't print if the RenderFrame is gone.
    if (render_frame_gone_) {
      return;
    }

    is_loading_ = frame->WillPrintSoon();
    if (is_loading_) {
      on_stop_loading_closure_ = base::BindOnce(
          &PrintRenderFrameHelper::PrintRequestedPagesInternal,
          weak_ptr_factory_.GetWeakPtr(), /*already_notified_frame=*/true);
      SetupOnStopLoadingTimeout();
      return;
    }
  }

  // If we are printing a frame with an internal PDF plugin element, find the
  // plugin node and print that instead.
  auto plugin = delegate_->GetPdfElement(frame);

  Print(frame, plugin, PrintRequestType::kRegular);

  if (render_frame_gone_) {
    return;
  }

  frame->DispatchAfterPrintEvent();
  // WARNING: `this` may be gone at this point. Do not do any more work here and
  // just return.
}

void PrintRenderFrameHelper::PrintWithParams(
    mojom::PrintPagesParamsPtr settings,
    PrintWithParamsCallback callback) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > kAllowedIpcDepthForPrint) {
    std::move(callback).Run(mojom::PrintWithParamsResult::NewFailureReason(
        mojom::PrintFailureReason::kGeneralFailure));
    return;
  }

  if (print_with_params_callback_) {
    std::move(callback).Run(mojom::PrintWithParamsResult::NewFailureReason(
        mojom::PrintFailureReason::kPrintingInProgress));
    return;
  }

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  frame->DispatchBeforePrintEvent(/*print_client=*/nullptr);
  // Don't print if the RenderFrame is gone.
  if (render_frame_gone_) {
    std::move(callback).Run(mojom::PrintWithParamsResult::NewFailureReason(
        mojom::PrintFailureReason::kGeneralFailure));
    return;
  }

  print_with_params_callback_ = std::move(callback);

  // If we are printing a frame with an internal PDF plugin element, find the
  // plugin node and print that instead.
  auto plugin_node = delegate_->GetPdfElement(frame);

  // TODO(caseq): have this logic on the caller side?
  const bool center_on_paper = !IsPrintingPdfFrame(frame, plugin_node);
  settings->params->print_scaling_option =
      center_on_paper && !settings->params->prefer_css_page_size
          ? mojom::PrintScalingOption::kCenterShrinkToFitPaper
          : mojom::PrintScalingOption::kSourceSize;
  RecordDebugEvent(settings->params->printed_doc_type ==
                           mojom::SkiaDocumentType::kMSKP
                       ? DebugEvent::kSetPrintSettings1
                       : DebugEvent::kSetPrintSettings2);
  SetPrintPagesParams(*settings);
  prep_frame_view_ =
      std::make_unique<PrepareFrameAndViewForPrint>(frame, plugin_node);
  prep_frame_view_->EnterPrintMode(*settings->params,
                                   /*ignore_css_margins=*/false);

  PrintPages();
  FinishFramePrinting();

  if (render_frame_gone_) {
    return;
  }

  frame->DispatchAfterPrintEvent();
  // WARNING: `this` may be gone at this point. Do not do any more work here and
  // just return.
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintRenderFrameHelper::PrintForSystemDialog() {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > kAllowedIpcDepthForPrint)
    return;

  if (closures_for_mojo_responses_->HasScriptedPrintPreviewQuitClosure()) {
    // If an in-progress print preview already created a nested loop, avoid
    // creating yet another nested loop. Instead, quit the current nested loop,
    // and call this method again.
    DCHECK(!do_deferred_print_for_system_dialog_);
    do_deferred_print_for_system_dialog_ = true;
    closures_for_mojo_responses_->RunScriptedPrintPreviewQuitClosure();
    return;
  }

  blink::WebLocalFrame* frame = print_preview_context_.source_frame();
  if (!frame) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  Print(frame, print_preview_context_.source_node(),
        PrintRequestType::kRegular);
  if (render_frame_gone_) {
    return;
  }

  print_in_progress_ = false;
  print_preview_context_.DispatchAfterPrintEvent();
  // WARNING: `this` may be gone at this point. Do not do any more work here and
  // just return.
}

void PrintRenderFrameHelper::SetPrintPreviewUI(
    mojo::PendingAssociatedRemote<mojom::PrintPreviewUI> preview) {
  preview_ui_.Bind(std::move(preview));
  preview_ui_.set_disconnect_handler(
      base::BindOnce(&PrintRenderFrameHelper::OnPreviewDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintRenderFrameHelper::InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS_ASH)
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
#endif
    bool has_selection) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > kAllowedIpcDepthForPrint)
    return;

  if (print_in_progress_) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (print_renderer) {
    print_renderer_.Bind(std::move(print_renderer));
    print_preview_context_.SetIsForArc(true);
  }
#endif

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  // If we are printing a frame with an internal PDF plugin element, find the
  // plugin node and print that instead.
  auto plugin = delegate_->GetPdfElement(frame);
  if (!plugin.IsNull()) {
    PrintNode(plugin);
    return;
  }

  print_in_progress_ = true;
  RecordDebugEvent(DebugEvent::kInitWithFrame2);
  print_preview_context_.InitWithFrame(frame);
  RequestPrintPreview(has_selection
                          ? PrintPreviewRequestType::kUserInitiatedSelection
                          : PrintPreviewRequestType::kUserInitiatedEntireFrame,
                      /*already_notified_frame=*/false);
  // Print Preview resets `print_in_progress_` when the dialog closes.
}

void PrintRenderFrameHelper::PrintPreview(base::Value::Dict settings) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > kAllowedIpcDepthForPrint)
    return;

  print_preview_context_.OnPrintPreview();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (print_preview_context_.IsForArc()) {
    base::UmaHistogramEnumeration("Arc.PrintPreview.PreviewEvent",
                                  PREVIEW_EVENT_REQUESTED, PREVIEW_EVENT_MAX);
  }
#endif

  if (!print_preview_context_.source_frame()) {
    DidFinishPrinting(PrintingResult::kFailPreview);
    return;
  }

  if (!UpdatePrintSettings(print_preview_context_.source_frame(),
                           print_preview_context_.source_node(),
                           settings.Clone())) {
    DidFinishPrinting(PrintingResult::kInvalidSettings);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Save the job settings if a PrintRenderer will be used to create the preview
  // document.
  if (print_renderer_)
    print_renderer_job_settings_ = std::move(settings);
#endif

  // Set the options from document if we are previewing a pdf and send a
  // message to browser.
  if (print_pages_params_->params->is_first_request &&
      !print_preview_context_.IsModifiable()) {
    mojom::OptionsFromDocumentParamsPtr options = SetOptionsFromPdfDocument();
    if (options && preview_ui_) {
      preview_ui_->SetOptionsFromDocument(
          std::move(options), print_pages_params_->params->preview_request_id);
    }
  }

  is_print_ready_metafile_sent_ = false;

  PrepareFrameForPreviewDocument();
}

void PrintRenderFrameHelper::OnPrintPreviewDialogClosed() {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (render_frame_gone_) {
    return;
  }

  print_in_progress_ = false;
  print_preview_context_.DispatchAfterPrintEvent();
  // WARNING: `this` may be gone at this point. Do not do any more work here and
  // just return.
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintRenderFrameHelper::PrintFrameContent(
    mojom::PrintFrameContentParamsPtr params,
    PrintFrameContentCallback callback) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > kAllowedIpcDepthForPrint)
    return;

  // If the last request is not finished yet, do not proceed.
  if (prep_frame_view_) {
    DLOG(ERROR) << "Previous request is still ongoing";
    return;
  }

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  frame->DispatchBeforePrintEvent(/*print_client=*/nullptr);
  if (!weak_this)
    return;

  ContentProxySet typeface_content_info;
  MetafileSkia metafile(mojom::SkiaDocumentType::kMSKP,
                        params->document_cookie);

  // Provide a typeface context to use with serializing to the print compositor.
  metafile.UtilizeTypefaceContext(&typeface_content_info);

  gfx::Size area_size = params->printable_area.size();
  // Since GetVectorCanvasForNewPage() starts a new recording, it will return
  // a valid canvas.
  cc::PaintCanvas* canvas = metafile.GetVectorCanvasForNewPage(
      area_size, gfx::Rect(area_size), 1.0f, mojom::PageOrientation::kUpright);
  DCHECK(canvas);

  canvas->SetPrintingMetafile(&metafile);

  // This subframe doesn't need to fit to the page size, thus we are not using
  // paginated layout for it. It just prints with the specified size.
  blink::WebPrintParams web_print_params(gfx::SizeF(area_size),
                                         /*use_paginated_layout=*/false);

  // Printing embedded pdf plugin has been broken since pdf plugin viewer was
  // moved out-of-process
  // (https://bugs.chromium.org/p/chromium/issues/detail?id=464269). So don't
  // try to handle pdf plugin element until that bug is fixed.
  {
    TRACE_EVENT0("print", "PrintRenderFrameHelper::PrintFrameContent");
    RecordDebugEvent(DebugEvent::kPrintBegin3);
    if (frame->PrintBegin(web_print_params,
                          /*constrain_to_node=*/blink::WebElement())) {
      frame->PrintPage(0, canvas);
    }
    frame->PrintEnd();
  }

  // Done printing. Close the canvas to retrieve the compiled metafile.
  bool ret = metafile.FinishPage();
  DCHECK(ret);

  metafile.FinishFrameContent();

  // Send the printed result back, if possible. Do not return early here on
  // failure, as DispatchAfterPrintEvent() still need to be called.
  mojom::DidPrintContentParamsPtr printed_frame_params =
      mojom::DidPrintContentParams::New();
  if (CopyMetafileDataToDidPrintContentParams(metafile,
                                              printed_frame_params.get())) {
    std::move(callback).Run(params->document_cookie,
                            std::move(printed_frame_params));
  } else {
    DLOG(ERROR) << "CopyMetafileDataToSharedMem failed";
  }

  if (render_frame_gone_) {
    return;
  }

  frame->DispatchAfterPrintEvent();
  // WARNING: `this` may be gone at this point. Do not do any more work here and
  // just return.
}

void PrintRenderFrameHelper::PrintingDone(bool success) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > kAllowedIpcDepthForPrint)
    return;
  notify_browser_of_print_failure_ = false;
  DidFinishPrinting(success ? PrintingResult::kOk : PrintingResult::kFailPrint);
}

void PrintRenderFrameHelper::ConnectToPdfRenderer() {
  // Deliberately do nothing.
}

void PrintRenderFrameHelper::PrintNodeUnderContextMenu() {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  PrintNode(render_frame()->GetWebFrame()->ContextMenuNode());
}

void PrintRenderFrameHelper::UpdateFrameMarginsCssInfo(
    const base::Value::Dict& settings) {
  constexpr int kDefault = static_cast<int>(mojom::MarginType::kDefaultMargins);
  int margins_type = settings.FindInt(kSettingMarginsType).value_or(kDefault);
  ignore_css_margins_ = margins_type != kDefault;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintRenderFrameHelper::PrepareFrameForPreviewDocument() {
  reset_prep_frame_view_ = false;

  if (!print_pages_params_) {
    print_preview_context_.set_error(PrintPreviewErrorBuckets::kZeroPages);
    DidFinishPrinting(PrintingResult::kFailPreview);
    return;
  }

  if (CheckForCancel()) {
    // No need to set an error, since |notify_browser_of_print_failure_| is
    // false.
    DidFinishPrinting(PrintingResult::kFailPreview);
    return;
  }

  // Don't reset loading frame or WebKit will fail assert. Just retry when
  // current selection is loaded.
  if (prep_frame_view_ && prep_frame_view_->IsLoadingSelection()) {
    reset_prep_frame_view_ = true;
    return;
  }

  const mojom::PrintParams& print_params = *print_pages_params_->params;
  prep_frame_view_ = std::make_unique<PrepareFrameAndViewForPrint>(
      print_preview_context_.source_frame(),
      print_preview_context_.source_node());

  prep_frame_view_->BeginPrinting(
      render_frame()->GetBlinkPreferences(), print_params, ignore_css_margins_,
      base::BindOnce(&PrintRenderFrameHelper::OnFramePreparedForPreviewDocument,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintRenderFrameHelper::OnFramePreparedForPreviewDocument() {
  if (preview_document_test_callback_) {
    std::move(preview_document_test_callback_)
        .Run(prep_frame_view_->frame()->GetDocument());
  }

  if (reset_prep_frame_view_) {
    PrepareFrameForPreviewDocument();
    return;
  }

  CreatePreviewDocumentResult result = CreatePreviewDocument();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (result == CreatePreviewDocumentResult::kInProgress) {
    return;
  }
#endif

  DidFinishPrinting(result == CreatePreviewDocumentResult::kSuccess
                        ? PrintingResult::kOk
                        : PrintingResult::kFailPreview);
}

PrintRenderFrameHelper::CreatePreviewDocumentResult
PrintRenderFrameHelper::CreatePreviewDocument() {
  if (!print_pages_params_ || CheckForCancel() || !preview_ui_)
    return CreatePreviewDocumentResult::kFail;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (print_preview_context_.IsForArc()) {
    base::UmaHistogramEnumeration("Arc.PrintPreview.PreviewEvent",
                                  PREVIEW_EVENT_CREATE_DOCUMENT,
                                  PREVIEW_EVENT_MAX);
  }
#endif

  const mojom::PrintParams& print_params = *print_pages_params_->params;

  bool require_document_metafile =
      print_params.printed_doc_type != mojom::SkiaDocumentType::kMSKP;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  require_document_metafile = require_document_metafile || print_renderer_;
#endif

  if (!print_preview_context_.CreatePreviewDocument(
          std::move(prep_frame_view_), print_pages_params_->pages,
          print_params.printed_doc_type, print_params.document_cookie,
          require_document_metafile)) {
    return CreatePreviewDocumentResult::kFail;
  }

  // If tagged PDF exporting is enabled, we also need to capture an
  // accessibility tree. AXTreeSnapshotter should stay alive through the end of
  // the scope of printing, because text drawing commands are only annotated
  // with a DOMNodeId if accessibility is enabled.
  if (delegate_->ShouldGenerateTaggedPDF())
    snapshotter_ =
        render_frame()->CreateAXTreeSnapshotter(ui::AXMode::kPDFPrinting);

  mojom::PageSizeMarginsPtr default_page_layout =
      ComputePageLayoutForCss(print_preview_context_.prepared_frame(), 0,
                              print_params, ignore_css_margins_)
          .page_size_margins;
  int dpi = GetDPI(print_params);
  // Convert to points.
  default_page_layout =
      ConvertedPageSizeMargins(default_page_layout, dpi, kPointsPerInch);

  bool all_pages_have_custom_size;
  bool all_pages_have_custom_orientation;
  GetPageSizeAndOrientationInfo(print_preview_context_.prepared_frame(),
                                print_preview_context_.total_page_count(),
                                &all_pages_have_custom_size,
                                &all_pages_have_custom_orientation);
  gfx::RectF printable_area_in_points(
      ConvertUnitFloat(print_params.printable_area.x(), dpi, kPointsPerInch),
      ConvertUnitFloat(print_params.printable_area.y(), dpi, kPointsPerInch),
      ConvertUnitFloat(print_params.printable_area.width(), dpi,
                       kPointsPerInch),
      ConvertUnitFloat(print_params.printable_area.height(), dpi,
                       kPointsPerInch));

  // Margins: Send default page layout to browser process.
  preview_ui_->DidGetDefaultPageLayout(
      std::move(default_page_layout), printable_area_in_points,
      all_pages_have_custom_size, all_pages_have_custom_orientation,
      print_params.preview_request_id);

  preview_ui_->DidStartPreview(
      mojom::DidStartPreviewParams::New(
          print_preview_context_.total_page_count(),
          print_preview_context_.pages_to_render(),
          print_params.pages_per_sheet,
          GetPdfPageSize(print_params.page_size, dpi),
          GetFitToPageScaleFactor(printable_area_in_points)),
      print_params.preview_request_id);
  if (CheckForCancel())
    return CreatePreviewDocumentResult::kFail;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If a PrintRenderer has been provided, use it to create the preview
  // document.
  if (print_renderer_) {
    base::TimeTicks begin_time = base::TimeTicks::Now();
    print_renderer_->CreatePreviewDocument(
        print_renderer_job_settings_.Clone(),
        base::BindOnce(&PrintRenderFrameHelper::OnPreviewDocumentCreated,
                       weak_ptr_factory_.GetWeakPtr(),
                       print_params.document_cookie, begin_time));
    return CreatePreviewDocumentResult::kInProgress;
  }
#endif

  if (print_pages_params_->params->printed_doc_type ==
      mojom::SkiaDocumentType::kMSKP) {
    // Want modifiable content of MSKP type to be collected into a document
    // during individual page preview generation (to avoid separate document
    // version for composition), notify to prepare to do this collection.
    preview_ui_->DidPrepareDocumentForPreview(
        print_pages_params_->params->document_cookie,
        print_params.preview_request_id);
  }

  {
    std::unique_ptr<HeaderAndFooterContext> header_footer_context;
    blink::WebLocalFrame* header_footer_frame = nullptr;
    if (print_pages_params_->params->display_header_footer) {
      header_footer_context = std::make_unique<HeaderAndFooterContext>(
          *print_preview_context_.prepared_frame());
      header_footer_frame = header_footer_context->frame();
    }
    while (!print_preview_context_.IsFinalPageRendered()) {
      uint32_t page_index = print_preview_context_.GetNextPageIndex();
      DCHECK_NE(page_index, kInvalidPageIndex);

      if (!RenderPreviewPage(page_index, header_footer_frame)) {
        return CreatePreviewDocumentResult::kFail;
      }

      if (CheckForCancel()) {
        return CreatePreviewDocumentResult::kFail;
      }

      // This code must call PrepareFrameAndViewForPrint::FinishPrinting() (by
      // way of print_preview_context_.AllPagesRendered()) before calling
      // FinalizePrintReadyDocument() when printing a PDF because the plugin
      // code does not generate output until FinishPrinting() gets called.
      // Printing PDFs does not generate draft pages, so IsFinalPageRendered()
      // and IsLastPageOfPrintReadyMetafile() will be true in the same iteration
      // of the loop.
      if (print_preview_context_.IsFinalPageRendered()) {
        print_preview_context_.AllPagesRendered();
      }

      if (print_preview_context_.IsLastPageOfPrintReadyMetafile()) {
        DCHECK(print_preview_context_.IsModifiable() ||
               print_preview_context_.IsFinalPageRendered());
        if (!FinalizePrintReadyDocument()) {
          return CreatePreviewDocumentResult::kFail;
        }
      }
    }
  }
  print_preview_context_.Finished();
  return CreatePreviewDocumentResult::kSuccess;
}

bool PrintRenderFrameHelper::RenderPreviewPage(
    uint32_t page_index,
    blink::WebLocalFrame* header_footer_frame) {
  TRACE_EVENT1("print", "PrintRenderFrameHelper::RenderPreviewPage",
               "page_index", page_index);

  const mojom::PrintParams& print_params = *print_pages_params_->params;
  MetafileSkia* render_metafile = print_preview_context_.metafile();
  std::unique_ptr<MetafileSkia> page_render_metafile;
  if (!render_metafile) {
    // No document metafile means using the print compositor, which will
    // provide the document metafile by combining the individual pages.
    page_render_metafile = std::make_unique<MetafileSkia>(
        print_params.printed_doc_type, print_params.document_cookie);
    CHECK(page_render_metafile->Init());
    render_metafile = page_render_metafile.get();
  }
  render_metafile->UtilizeTypefaceContext(
      print_preview_context_.typeface_content_info());
  base::TimeTicks begin_time = base::TimeTicks::Now();
  PrintPageInternal(print_params, page_index,
                    print_preview_context_.total_page_count(),
                    print_preview_context_.prepared_frame(),
                    header_footer_frame, render_metafile);
  print_preview_context_.RenderedPreviewPage(base::TimeTicks::Now() -
                                             begin_time);

  // For non-modifiable content, there is no need to call PreviewPageRendered()
  // since it generally renders very fast. Just render and send the finished
  // document to the browser.
  if (!print_preview_context_.IsModifiable())
    return true;

  // Let the browser know this page has been rendered. Send
  // |page_render_metafile|, which contains the rendering for just this one
  // page. Then the browser can update the user visible print preview one page
  // at a time, instead of waiting for the entire document to be rendered.
  page_render_metafile =
      render_metafile->GetMetafileForCurrentPage(print_params.printed_doc_type);
  return PreviewPageRendered(page_index, std::move(page_render_metafile));
}

bool PrintRenderFrameHelper::FinalizePrintReadyDocument() {
  TRACE_EVENT0("print", "PrintRenderFrameHelper::FinalizePrintReadyDocument");

  DCHECK(!is_print_ready_metafile_sent_);
  print_preview_context_.FinalizePrintReadyDocument();

  auto preview_params = mojom::DidPreviewDocumentParams::New();
  preview_params->content = mojom::DidPrintContentParams::New();

  // Modifiable content of MSKP type is collected into a document during
  // individual page preview generation, so only need to share a separate
  // document version for composition when it isn't MSKP or is from a
  // separate print renderer (e.g., not print compositor).
  MetafileSkia* metafile = print_preview_context_.metafile();
  if (metafile) {
    if (!CopyMetafileDataToDidPrintContentParams(
            *metafile, preview_params->content.get())) {
      LOG(ERROR) << "CopyMetafileDataToDidPrintContentParams failed";
      print_preview_context_.set_error(
          PrintPreviewErrorBuckets::kMetafileCopyFailed);
      return false;
    }
  }

  preview_params->document_cookie =
      print_pages_params_->params->document_cookie;
  preview_params->expected_pages_count =
      print_preview_context_.pages_rendered_count();

  is_print_ready_metafile_sent_ = true;

  if (preview_ui_) {
    preview_ui_->MetafileReadyForPrinting(
        std::move(preview_params),
        print_pages_params_->params->preview_request_id);
  }
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrintRenderFrameHelper::OnPreviewDocumentCreated(
    int document_cookie,
    base::TimeTicks begin_time,
    base::ReadOnlySharedMemoryRegion preview_document_region) {
  // Since the PrintRenderer renders preview documents asynchronously, multiple
  // preview document requests may be sent before a preview document is
  // returned. If the received preview document's cookie does not match the
  // latest document cookie, ignore it and wait for the final preview document.
  if (document_cookie != print_pages_params_->params->document_cookie) {
    return;
  }

  bool success =
      ProcessPreviewDocument(begin_time, std::move(preview_document_region));
  DidFinishPrinting(success ? PrintingResult::kOk
                            : PrintingResult::kFailPreview);
}
#endif

bool PrintRenderFrameHelper::ProcessPreviewDocument(
    base::TimeTicks begin_time,
    base::ReadOnlySharedMemoryRegion preview_document_region) {
  // Record the render time for the entire document.
  print_preview_context_.RenderedPreviewDocument(base::TimeTicks::Now() -
                                                 begin_time);

  base::ReadOnlySharedMemoryMapping preview_document_mapping =
      preview_document_region.Map();
  if (!preview_document_mapping.IsValid())
    return false;

  CHECK(print_preview_context_.metafile()->InitFromData(
      preview_document_mapping.GetMemoryAsSpan<const uint8_t>()));

  if (CheckForCancel())
    return false;

  print_preview_context_.AllPagesRendered();
  if (!FinalizePrintReadyDocument())
    return false;

  print_preview_context_.Finished();
  return true;
}

int PrintRenderFrameHelper::GetFitToPageScaleFactor(
    const gfx::RectF& printable_area_in_points) {
  if (print_preview_context_.IsModifiable())
    return 100;

  blink::WebLocalFrame* frame = print_preview_context_.source_frame();
  const blink::WebNode& node = print_preview_context_.source_node();
  blink::WebPrintPresetOptions preset_options;
  if (!frame->GetPrintPresetOptionsForPlugin(node, &preset_options))
    return 100;

  if (!preset_options.uniform_page_size.has_value())
    return 0;

  // Ensure we do not divide by 0 later.
  const gfx::Size& uniform_page_size = preset_options.uniform_page_size.value();
  if (uniform_page_size.IsEmpty())
    return 0;

  // Figure out if the sizes have the same orientation
  bool is_printable_area_landscape =
      printable_area_in_points.width() > printable_area_in_points.height();
  bool is_preset_landscape =
      uniform_page_size.width() > uniform_page_size.height();
  bool rotate = is_printable_area_landscape != is_preset_landscape;
  // Match orientation for computing scaling
  double printable_width = rotate ? printable_area_in_points.height()
                                  : printable_area_in_points.width();
  double printable_height = rotate ? printable_area_in_points.width()
                                   : printable_area_in_points.height();

  double scale_width =
      printable_width / static_cast<double>(uniform_page_size.width());
  double scale_height =
      printable_height / static_cast<double>(uniform_page_size.height());
  return static_cast<int>(100.0f * std::min(scale_width, scale_height));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintRenderFrameHelper::PrintNode(const blink::WebNode& node) {
  if (node.IsNull() || !node.GetDocument().GetFrame()) {
    // This can occur when the context menu refers to an invalid WebNode.
    // See http://crbug.com/100890#c17 for a repro case.
    return;
  }

  if (print_in_progress_) {
    // This can happen as a result of processing sync messages when printing
    // from ppapi plugins. It's a rare case, so its OK to just fail here.
    // See http://crbug.com/159165.
    return;
  }

  if (g_is_preview_enabled) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    print_in_progress_ = true;
    RecordDebugEvent(DebugEvent::kInitWithNode);
    print_preview_context_.InitWithNode(node);
    RequestPrintPreview(PrintPreviewRequestType::kUserInitiatedContextNode,
                        /*already_notified_frame=*/false);
    // Print Preview resets `print_in_progress_` when the dialog closes.
    return;
#else
    NOTREACHED();
#endif
  }

  blink::WebLocalFrame* frame = node.GetDocument().GetFrame();
  if (!frame) {
    return;
  }

  print_in_progress_ = true;

  // Make a copy of the node, in case RenderView::OnContextMenuClosed() resets
  // its |context_menu_node_|.
  blink::WebNode duplicate_node(node);

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  frame->DispatchBeforePrintEvent(/*print_client=*/nullptr);
  if (!weak_this) {
    return;
  }

  Print(duplicate_node.GetDocument().GetFrame(), duplicate_node,
        PrintRequestType::kRegular);
  // Check if `this` is still valid.
  if (!weak_this) {
    return;
  }

  frame->DispatchAfterPrintEvent();
  if (!weak_this) {
    return;
  }

  print_in_progress_ = false;
}

void PrintRenderFrameHelper::Print(blink::WebLocalFrame* frame,
                                   const blink::WebNode& node,
                                   PrintRequestType print_request_type) {
  // If still not finished with earlier print request simply ignore.
  if (prep_frame_view_)
    return;

  FrameReference frame_ref(frame);

  if (!InitPrintSettings(frame, node)) {
    // Browser triggered this code path. It already knows about the failure.
    notify_browser_of_print_failure_ = false;

    DidFinishPrinting(PrintingResult::kFailPrintInit);
    return;
  }

  uint32_t expected_page_count = CalculateNumberOfPages(frame, node);

  // Some full screen plugins can say they don't want to print.
  if (!expected_page_count || expected_page_count > kMaxPageCount) {
    DidFinishPrinting(PrintingResult::kFailPrint);
    return;
  }

  // Ask the browser to show UI to retrieve the final print settings.
  {
    // ScriptedPrint() in GetPrintSettingsFromUser() will reset
    // |print_scaling_option|, so save the value here and restore it afterwards.
    mojom::PrintScalingOption scaling_option =
        print_pages_params_->params->print_scaling_option;

    auto self = weak_ptr_factory_.GetWeakPtr();
    mojom::PrintPagesParamsPtr print_settings = GetPrintSettingsFromUser(
        frame_ref.GetFrame(), node, expected_page_count, print_request_type);
    // Check if `this` is still valid.
    if (!self)
      return;

    // GetPrintSettingsFromUser() could return nullptr when
    // |print_manager_host_| is closed, or when the user cancels.
    if (!print_settings) {
      if (print_manager_host_) {
        // Release resources and fail silently if the user cancels.
        DidFinishPrinting(PrintingResult::kOk);
      }
      return;
    }

    print_settings->params->print_scaling_option =
        print_settings->params->prefer_css_page_size
            ? mojom::PrintScalingOption::kSourceSize
            : scaling_option;
    RecordDebugEvent(print_settings->params->printed_doc_type ==
                             mojom::SkiaDocumentType::kMSKP
                         ? DebugEvent::kSetPrintSettings3
                         : DebugEvent::kSetPrintSettings4);
    SetPrintPagesParams(*print_settings);
  }

  // Render Pages for printing.
  if (!RenderPagesForPrint(frame_ref.GetFrame(), node)) {
    LOG(ERROR) << "RenderPagesForPrint failed";
    DidFinishPrinting(PrintingResult::kFailPrint);
  }
  scripting_throttler_.Reset();
}

void PrintRenderFrameHelper::DidFinishPrinting(PrintingResult result) {
  // Code in PrintPagesNative() handles the success case firing the callback,
  // so if we get here with the pending callback it must be the failure case.
  if (print_with_params_callback_) {
    DCHECK_NE(result, PrintingResult::kOk);
    std::move(print_with_params_callback_)
        .Run(mojom::PrintWithParamsResult::NewFailureReason(
            result == PrintingResult::kInvalidPageRange
                ? mojom::PrintFailureReason::kInvalidPageRange
                : mojom::PrintFailureReason::kGeneralFailure));
    Reset();
    return;
  }

  int cookie =
      print_pages_params_ ? print_pages_params_->params->document_cookie : 0;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  int request_id = print_pages_params_
                       ? print_pages_params_->params->preview_request_id
                       : -1;
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  switch (result) {
    case PrintingResult::kOk:
      break;

    case PrintingResult::kFailPrintInit:
      DCHECK(!notify_browser_of_print_failure_);
      break;

    case PrintingResult::kInvalidPageRange:
    case PrintingResult::kFailPrint:
      if (notify_browser_of_print_failure_ && print_pages_params_) {
        GetPrintManagerHost()->PrintingFailed(
            cookie, result == PrintingResult::kInvalidPageRange
                        ? mojom::PrintFailureReason::kInvalidPageRange
                        : mojom::PrintFailureReason::kGeneralFailure);
      }
      break;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    case PrintingResult::kFailPreview:
      if (!is_print_ready_metafile_sent_) {
        if (notify_browser_of_print_failure_) {
          LOG(ERROR) << "CreatePreviewDocument failed";
          if (preview_ui_)
            preview_ui_->PrintPreviewFailed(cookie, request_id);
        } else {
          if (preview_ui_)
            preview_ui_->PrintPreviewCancelled(cookie, request_id);
        }
      }
      print_preview_context_.Failed(notify_browser_of_print_failure_);
      break;
    case PrintingResult::kInvalidSettings:
      if (preview_ui_)
        preview_ui_->PrinterSettingsInvalid(cookie, request_id);
      print_preview_context_.Failed(false);
      break;
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  }

  Reset();
}

void PrintRenderFrameHelper::Reset() {
  prep_frame_view_.reset();
  print_pages_params_.reset();
  notify_browser_of_print_failure_ = true;
  snapshotter_.reset();

  // The callback is supposed to be consumed at this point meaning we
  // reported results to the PrintWithParams() caller.
  DCHECK(!print_with_params_callback_);
}

void PrintRenderFrameHelper::OnFramePreparedForPrintPages() {
  PrintPages();
  FinishFramePrinting();
}

void PrintRenderFrameHelper::PrintPages() {
  if (!prep_frame_view_)  // Printing is already canceled or failed.
    return;

  uint32_t page_count = prep_frame_view_->GetPageCount();
  if (!page_count || page_count > kMaxPageCount) {
    LOG(ERROR) << "Can't print 0 pages and the page count couldn't be greater "
                  "than kMaxPageCount.";
    return DidFinishPrinting(PrintingResult::kFailPrint);
  }

  // TODO(vitalybuka): should be page_count or valid pages from params.pages.
  // See http://crbug.com/161576
  if (!print_with_params_callback_) {
    GetPrintManagerHost()->DidGetPrintedPagesCount(
        print_pages_params_->params->document_cookie, page_count);
  }

  std::vector<uint32_t> pages_to_print =
      PageNumber::GetPages(print_pages_params_->pages, page_count);
  if (pages_to_print.empty())
    return DidFinishPrinting(PrintingResult::kInvalidPageRange);
  if (!PrintPagesNative(prep_frame_view_->frame(), page_count,
                        pages_to_print)) {
    LOG(ERROR) << "Printing failed.";
    return DidFinishPrinting(PrintingResult::kFailPrint);
  }
}

bool PrintRenderFrameHelper::PrintPagesNative(
    blink::WebLocalFrame* frame,
    uint32_t page_count,
    const std::vector<uint32_t>& printed_pages) {
  DCHECK(!printed_pages.empty());

  const mojom::PrintPagesParams& params = *print_pages_params_;
  const mojom::PrintParams& print_params = *params.params;

  // Provide a typeface context to use with serializing to the print compositor.
  ContentProxySet typeface_content_info;
  MetafileSkia metafile(print_params.printed_doc_type,
                        print_params.document_cookie);
  CHECK(metafile.Init());
  metafile.UtilizeTypefaceContext(&typeface_content_info);

  bool generate_tagged_pdf = print_params.generate_tagged_pdf.value_or(
      delegate_->ShouldGenerateTaggedPDF());

  // If tagged PDF exporting is enabled, we also need to capture an
  // accessibility tree and store it in the metafile. AXTreeSnapshotter
  // should stay alive through the end of this function, because text
  // drawing commands are only annotated with a DOMNodeId if accessibility
  // is enabled.
  std::unique_ptr<content::AXTreeSnapshotter> snapshotter;
  ui::AXTreeUpdate accessibility_tree;
  if (generate_tagged_pdf) {
    snapshotter =
        render_frame()->CreateAXTreeSnapshotter(ui::AXMode::kPDFPrinting);
    snapshotter->Snapshot(
        /*max_node_count=*/0,
        /*timeout=*/{},
        print_params.printed_doc_type == mojom::SkiaDocumentType::kMSKP
            ? &accessibility_tree
            : &metafile.accessibility_tree());
    metafile.set_generate_document_outline(
        print_params.generate_document_outline);
  }

  blink::WebString title = frame->GetDocument().Title();
  metafile.set_title(title.IsEmpty() ? base::UTF16ToUTF8(print_params.title)
                                     : title.Utf8());

  mojom::DidPrintDocumentParamsPtr page_params =
      mojom::DidPrintDocumentParams::New();
  page_params->content = mojom::DidPrintContentParams::New();
  page_params->page_size = ToFlooredSize(print_params.page_size);
  page_params->content_area = gfx::Rect(page_params->page_size);

  {
    std::unique_ptr<HeaderAndFooterContext> header_footer_context;
    blink::WebLocalFrame* header_footer_frame = nullptr;
    if (print_params.display_header_footer) {
      header_footer_context = std::make_unique<HeaderAndFooterContext>(*frame);
      header_footer_frame = header_footer_context->frame();
    }
    for (uint32_t printed_page : printed_pages) {
      PrintPageInternal(print_params, printed_page, page_count, frame,
                        header_footer_frame, &metafile);
    }
  }

  // blink::printEnd() for PDF should be called before metafile is closed.
  FinishFramePrinting();

  metafile.FinishDocument();

  if (!CopyMetafileDataToDidPrintContentParams(metafile,
                                               page_params->content.get())) {
    return false;
  }

  page_params->document_cookie = print_params.document_cookie;
#if BUILDFLAG(IS_WIN)
  page_params->physical_offsets = printer_printable_area_.origin();
#endif

  if (print_with_params_callback_) {
    auto result = mojom::PrintWithParamsResultData::New();
    result->params = std::move(page_params);
    result->accessibility_tree = std::move(accessibility_tree);
    result->generate_document_outline = print_params.generate_document_outline;
    std::move(print_with_params_callback_)
        .Run(mojom::PrintWithParamsResult::NewData(std::move(result)));
    Reset();
    return true;
  }

  bool completed = false;
  GetPrintManagerHost()->DidPrintDocument(std::move(page_params), &completed);
  return completed;
}

void PrintRenderFrameHelper::FinishFramePrinting() {
  prep_frame_view_.reset();
}

void PrintRenderFrameHelper::IPCReceived() {
  // The class is not designed to handle recursive messages. This is not
  // expected during regular flow. However, during rendering of content for
  // printing, lower level code may run a nested run loop. E.g. PDF may have a
  // script to show message box (http://crbug.com/502562). In that moment, the
  // browser may receive updated printer capabilities and decide to restart
  // print preview generation. When this happens, message handling functions may
  // choose to ignore messages or safely crash the process.
  ++ipc_nesting_level_;
}

void PrintRenderFrameHelper::IPCProcessed() {
  --ipc_nesting_level_;
  if (ipc_nesting_level_ == 0 && render_frame_gone_ && !delete_pending_) {
    delete_pending_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  this);
  }
}

bool PrintRenderFrameHelper::InitPrintSettings(blink::WebLocalFrame* frame,
                                               const blink::WebNode& node) {
  // Reset to default values.
  ignore_css_margins_ = false;

  mojom::PrintPagesParams settings;
  GetPrintManagerHost()->GetDefaultPrintSettings(&settings.params);

  // Check if the printer returned any settings, if the settings are null,
  // assume there are no printer drivers configured. So safely terminate.
  if (!settings.params) {
    // Caller will reset `print_pages_params_`.
    return false;
  }

  bool center_on_paper = !IsPrintingPdfFrame(frame, node);
  settings.params->print_scaling_option =
      center_on_paper ? mojom::PrintScalingOption::kCenterShrinkToFitPaper
                      : mojom::PrintScalingOption::kSourceSize;
  RecordDebugEvent(settings.params->printed_doc_type ==
                           mojom::SkiaDocumentType::kMSKP
                       ? DebugEvent::kSetPrintSettings5
                       : DebugEvent::kSetPrintSettings6);
  SetPrintPagesParams(settings);
  return true;
}

uint32_t PrintRenderFrameHelper::CalculateNumberOfPages(
    blink::WebLocalFrame* frame,
    const blink::WebNode& node) {
  DCHECK(frame);
  const mojom::PrintParams& params = *print_pages_params_->params;
  PrepareFrameAndViewForPrint prepare(frame, node);
  prepare.EnterPrintMode(params, /*ignore_css_margins=*/false);
  return prepare.GetPageCount();
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
mojom::OptionsFromDocumentParamsPtr
PrintRenderFrameHelper::SetOptionsFromPdfDocument() {
  blink::WebLocalFrame* source_frame = print_preview_context_.source_frame();
  const blink::WebNode& source_node = print_preview_context_.source_node();

  blink::WebPrintPresetOptions preset_options;
  if (!source_frame->GetPrintPresetOptionsForPlugin(source_node,
                                                    &preset_options)) {
    return nullptr;
  }

  return mojom::OptionsFromDocumentParams::New(
      PDFShouldDisableScalingBasedOnPreset(preset_options,
                                           *print_pages_params_->params, false),
      preset_options.copies, preset_options.duplex_mode);
}

bool PrintRenderFrameHelper::UpdatePrintSettings(
    blink::WebLocalFrame* frame,
    const blink::WebNode& node,
    base::Value::Dict passed_job_settings) {
  CHECK(!passed_job_settings.empty());

  base::Value::Dict modified_job_settings;
  const base::Value::Dict* job_settings;
  bool source_is_html = !IsPrintingPdfFrame(frame, node);
  if (source_is_html) {
    job_settings = &passed_job_settings;
  } else {
    modified_job_settings.Merge(std::move(passed_job_settings));
    modified_job_settings.Set(kSettingHeaderFooterEnabled, false);
    modified_job_settings.Set(kSettingMarginsType,
                              static_cast<int>(mojom::MarginType::kNoMargins));
    job_settings = &modified_job_settings;
  }

  mojom::PrintPagesParamsPtr settings;
  GetPrintManagerHost()->UpdatePrintSettings(job_settings->Clone(), &settings);
  if (!settings) {
    print_preview_context_.set_error(
        PrintPreviewErrorBuckets::kEmptyPrinterSettings);
    return false;
  }

  settings->params->preview_ui_id = job_settings->FindInt(kPreviewUIID).value();

  // Validate expected print preview settings.
  settings->params->is_first_request =
      job_settings->FindBool(kIsFirstRequest).value();
  settings->params->preview_request_id =
      job_settings->FindInt(kPreviewRequestID).value();

  settings->params->print_to_pdf = IsPrintToPdfRequested(*job_settings);
  UpdateFrameMarginsCssInfo(*job_settings);
  settings->params->print_scaling_option = GetPrintScalingOption(
      frame, node, source_is_html, *job_settings, *settings->params);

  RecordDebugEvent(settings->params->printed_doc_type ==
                           mojom::SkiaDocumentType::kMSKP
                       ? DebugEvent::kSetPrintSettings7
                       : DebugEvent::kSetPrintSettings8);
  SetPrintPagesParams(*settings);
  return true;
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

mojom::PrintPagesParamsPtr PrintRenderFrameHelper::GetPrintSettingsFromUser(
    blink::WebLocalFrame* frame,
    const blink::WebNode& node,
    uint32_t expected_pages_count,
    PrintRequestType print_request_type) {
  bool is_scripted = print_request_type == PrintRequestType::kScripted;
  DCHECK(is_scripted || print_request_type == PrintRequestType::kRegular);

  auto params = mojom::ScriptedPrintParams::New();
  params->cookie = print_pages_params_->params->document_cookie;
  params->has_selection = frame->HasSelection();
  params->expected_pages_count = expected_pages_count;
  mojom::MarginType margin_type = mojom::MarginType::kDefaultMargins;
  if (IsPrintingPdfFrame(frame, node))
    margin_type = GetMarginsForPdf(frame, node, *print_pages_params_->params);
  params->margin_type = margin_type;
  params->is_scripted = is_scripted;

  GetPrintManagerHost()->DidShowPrintDialog();

  print_pages_params_.reset();

  mojom::PrintPagesParamsPtr print_settings;
  GetPrintManagerHost()->ScriptedPrint(std::move(params), &print_settings);
  return print_settings;
  // WARNING: `this` may be gone at this point. Do not do any more work here
  // and just return.
}

bool PrintRenderFrameHelper::RenderPagesForPrint(blink::WebLocalFrame* frame,
                                                 const blink::WebNode& node) {
  if (!frame || prep_frame_view_)
    return false;

  const mojom::PrintPagesParams& params = *print_pages_params_;
  const mojom::PrintParams& print_params = *params.params;
  prep_frame_view_ = std::make_unique<PrepareFrameAndViewForPrint>(frame, node);
  DCHECK(!print_pages_params_->params->selection_only ||
         print_pages_params_->pages.empty());
  prep_frame_view_->BeginPrinting(
      render_frame()->GetBlinkPreferences(), print_params, ignore_css_margins_,
      base::BindOnce(&PrintRenderFrameHelper::OnFramePreparedForPrintPages,
                     weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void PrintRenderFrameHelper::PrintPageInternal(
    const mojom::PrintParams& params,
    uint32_t page_index,
    uint32_t page_count,
    blink::WebLocalFrame* frame,
    blink::WebLocalFrame* header_footer_frame,
    MetafileSkia* metafile) {
  PageSizeMarginsWithOrientation layout =
      ComputePageLayoutForCss(frame, page_index, params, ignore_css_margins_);
  auto& page_layout_in_device_pixels = layout.page_size_margins;
  mojom::PageSizeMarginsPtr page_layout_in_css_pixels =
      ConvertedPageSizeMargins(page_layout_in_device_pixels, GetDPI(params),
                               kPixelsPerInch);

  cc::PaintCanvas* canvas;
  {
    // Explicit scope for stuff in points. Blink renders in CSS pixels. Convert
    // to points for Skia metafile / PDF, since that's what they want. The PDF
    // generation code expects the values to be in points, so that if we had
    // passed them as pixels, the pages would be 1/kPointsPerPixel (33.333%)
    // larger than they should be. It may be a cleaner approach if the metafile
    // code handled this, so that this code could simply deal with CSS
    // pixels. But for now the conversion is performed here.
    mojom::PageSizeMarginsPtr page_layout_in_points = ConvertedPageSizeMargins(
        page_layout_in_device_pixels, GetDPI(params), kPointsPerInch);

    float page_width = page_layout_in_points->content_width +
                       page_layout_in_points->margin_left +
                       page_layout_in_points->margin_right;
    float page_height = page_layout_in_points->content_height +
                        page_layout_in_points->margin_top +
                        page_layout_in_points->margin_bottom;
    gfx::Size page_size_in_points =
        gfx::ToRoundedSize(gfx::SizeF(page_width, page_height));

    const double scale_factor_for_points = static_cast<double>(kPointsPerInch) /
                                           static_cast<double>(kPixelsPerInch);
    canvas = metafile->GetVectorCanvasForNewPage(
        page_size_in_points, gfx::Rect(page_size_in_points),
        scale_factor_for_points, layout.page_orientation);
  }
  if (!canvas)
    return;

  canvas->SetPrintingMetafile(metafile);

  RenderPageContent(frame, page_index, canvas);

  // Render headers and footers after the page content, as suggested in the spec
  // (the term "page margin boxes" is a generalization of headers and footers):
  // https://drafts.csswg.org/css-page-3/#painting

  CHECK_EQ(params.display_header_footer, !!header_footer_frame);
  if (header_footer_frame) {
    PrintHeaderAndFooter(canvas, *header_footer_frame, page_index, page_count,
                         *frame, *page_layout_in_css_pixels, params);
  }

  // Done printing. Close the canvas to retrieve the compiled metafile.
  bool ret = metafile->FinishPage();
  DCHECK(ret);
}

void PrintRenderFrameHelper::SetupOnStopLoadingTimeout() {
  static constexpr base::TimeDelta kLoadEventTimeout = base::Seconds(2);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrintRenderFrameHelper::DidFinishLoadForPrinting,
                     weak_ptr_factory_.GetWeakPtr()),
      kLoadEventTimeout);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintRenderFrameHelper::ShowScriptedPrintPreview() {
  if (is_scripted_preview_delayed_) {
    is_scripted_preview_delayed_ = false;
    GetPrintManagerHost()->ShowScriptedPrintPreview(
        print_preview_context_.IsModifiable());
  }
}

void PrintRenderFrameHelper::RequestPrintPreview(PrintPreviewRequestType type,
                                                 bool already_notified_frame) {
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  if (!already_notified_frame) {
    print_preview_context_.DispatchBeforePrintEvent(weak_this);
    if (!weak_this) {
      return;
    }

    if (type != PrintPreviewRequestType::kScripted) {
      // Since currently we can not block the `window.print()` call and load
      // the print only resources at the same time, no need to call
      // `WillPrintSoon()`.
      //
      // This is a conscious tradeoff between rendering correctness and
      // expected blocking behavior.
      //
      // The main Bugs that led us to taking this tradeoff are:
      // crbug.com/357784797
      // crbug.com/361375802
      //
      // Potential solutions to these bugs and the current chosen tradeoff
      // were discussed on:
      // https://groups.google.com/u/0/a/chromium.org/g/platform-architecture-dev/c/O45yJShVmZg
      //
      // Bug tracking further investigation into a solution that satisfies
      // both the blocking of the `window.print()` call and loading of
      // print only resources:
      // crbug.com/369111067

      is_loading_ = print_preview_context_.source_frame()->WillPrintSoon();
      if (is_loading_) {
        // Wait for DidStopLoading, for two reasons:
        // * To give the document time to finish loading any pending resources
        //   that are desired for printing.
        // * Plugins may not know the correct `is_modifiable` value until they
        //   are fully loaded, which occurs when DidStopLoading() is called.
        //   Defer showing the preview until then.
        on_stop_loading_closure_ =
            base::BindOnce(&PrintRenderFrameHelper::RequestPrintPreview,
                           weak_ptr_factory_.GetWeakPtr(), type, true);
        SetupOnStopLoadingTimeout();
        return;
      }
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool is_from_arc = print_preview_context_.IsForArc();
#endif
  const bool is_modifiable = print_preview_context_.IsModifiable();
  const bool has_selection = print_preview_context_.HasSelection();

  auto params = mojom::RequestPrintPreviewParams::New();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  params->is_from_arc = is_from_arc;
#endif
  params->is_modifiable = is_modifiable;
  params->has_selection = has_selection;
  switch (type) {
    case PrintPreviewRequestType::kScripted: {
      // Shows scripted print preview in two stages.
      // 1. SetupScriptedPrintPreview() blocks this call and JS by running a
      //    nested run loop.
      // 2. ShowScriptedPrintPreview() shows preview once the document has been
      //    loaded.
      RecordDebugEvent(DebugEvent::kRequestPrintPreviewScripted);
      is_scripted_preview_delayed_ = true;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&PrintRenderFrameHelper::ShowScriptedPrintPreview,
                         weak_ptr_factory_.GetWeakPtr()));
      base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
      closures_for_mojo_responses_->SetScriptedPrintPreviewQuitClosure(
          loop.QuitClosure());
      GetPrintManagerHost()->SetupScriptedPrintPreview(base::BindOnce(
          &ClosuresForMojoResponse::RunScriptedPrintPreviewQuitClosure,
          closures_for_mojo_responses_));
      loop.Run();

      // Check if `this` is still valid.
      if (weak_this) {
        is_scripted_preview_delayed_ = false;

        if (do_deferred_print_for_system_dialog_) {
          // PrintForSystemDialog() quit the |loop| to avoid running 2 levels of
          // nested loops. Resume PrintForSystemDialog().
          do_deferred_print_for_system_dialog_ = false;
          PrintForSystemDialog();
          // WARNING: `this` may be gone at this point. Do not do any more work
          // here and just return.
        }
      }
      return;
    }
    case PrintPreviewRequestType::kUserInitiatedEntireFrame: {
      RecordDebugEvent(
          DebugEvent::kRequestPrintPreviewUserInitiatedEntireFrame);
      break;
    }
    case PrintPreviewRequestType::kUserInitiatedSelection: {
      RecordDebugEvent(DebugEvent::kRequestPrintPreviewUserInitiatedSelection);
      DCHECK(has_selection);
      DCHECK(!print_preview_context_.IsPlugin());
      params->selection_only = has_selection;
      break;
    }
    case PrintPreviewRequestType::kUserInitiatedContextNode: {
      RecordDebugEvent(
          DebugEvent::kRequestPrintPreviewUserInitiatedContextNode);
      params->webnode_only = true;
      break;
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (print_preview_context_.IsForArc()) {
    base::UmaHistogramEnumeration("Arc.PrintPreview.PreviewEvent",
                                  PREVIEW_EVENT_INITIATED, PREVIEW_EVENT_MAX);
  }
#endif
  GetPrintManagerHost()->RequestPrintPreview(std::move(params));
}

bool PrintRenderFrameHelper::CheckForCancel() {
  const mojom::PrintParams& print_params = *print_pages_params_->params;
  bool cancel = false;

  if (!GetPrintManagerHost()->CheckForCancel(print_params.preview_ui_id,
                                             print_params.preview_request_id,
                                             &cancel)) {
    cancel = true;
  }

  if (cancel)
    notify_browser_of_print_failure_ = false;
  return cancel;
}

bool PrintRenderFrameHelper::PreviewPageRendered(
    uint32_t page_index,
    std::unique_ptr<MetafileSkia> metafile) {
  DCHECK_NE(page_index, kInvalidPageIndex);
  DCHECK(metafile);
  DCHECK(print_preview_context_.IsModifiable());

  TRACE_EVENT1("print", "PrintRenderFrameHelper::PreviewPageRendered",
               "page_index", page_index);

  // Make sure the RenderFrame is alive before taking the snapshot.
  if (render_frame_gone_)
    snapshotter_.reset();

  // For tagged PDF exporting, send a snapshot of the accessibility tree
  // along with page 0. The accessibility tree contains the content for
  // all of the pages of the main frame.
  //
  // TODO(dmazzoni) Support multi-frame tagged PDFs.
  // http://crbug.com/1039817
  if (snapshotter_ && page_index == 0) {
    ui::AXTreeUpdate accessibility_tree;
    snapshotter_->Snapshot(/* max_node_count= */ 0,
                           /* timeout= */ {}, &accessibility_tree);
    GetPrintManagerHost()->SetAccessibilityTree(
        print_pages_params_->params->document_cookie, accessibility_tree);
  }

  auto preview_page_params = mojom::DidPreviewPageParams::New();
  preview_page_params->content = mojom::DidPrintContentParams::New();
  if (!CopyMetafileDataToDidPrintContentParams(
          *metafile, preview_page_params->content.get())) {
    LOG(ERROR) << "CopyMetafileDataToDidPrintContentParams failed";
    print_preview_context_.set_error(
        PrintPreviewErrorBuckets::kMetafileCopyFailed);
    return false;
  }

  preview_page_params->page_index = page_index;
  preview_page_params->document_cookie =
      print_pages_params_->params->document_cookie;

  if (preview_ui_) {
    preview_ui_->DidPreviewPage(
        std::move(preview_page_params),
        print_pages_params_->params->preview_request_id);
  }
  return true;
}

void PrintRenderFrameHelper::OnPreviewDisconnect() {
  preview_ui_.reset();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

PrintRenderFrameHelper::PrintPreviewContext::PrintPreviewContext() = default;

PrintRenderFrameHelper::PrintPreviewContext::~PrintPreviewContext() = default;

void PrintRenderFrameHelper::PrintPreviewContext::InitWithFrame(
    blink::WebLocalFrame* web_frame) {
  DCHECK(web_frame);
  DCHECK(!IsRendering());
  state_ = State::kInitialized;
  source_frame_.Reset(web_frame);
  source_node_.Reset();
  CalculatePluginAttributes();
}

void PrintRenderFrameHelper::PrintPreviewContext::InitWithNode(
    const blink::WebNode& web_node) {
  DCHECK(!web_node.IsNull());
  DCHECK(web_node.GetDocument().GetFrame());
  DCHECK(!IsRendering());
  state_ = State::kInitialized;
  source_frame_.Reset(web_node.GetDocument().GetFrame());
  source_node_ = web_node;
  CalculatePluginAttributes();
}

void PrintRenderFrameHelper::PrintPreviewContext::DispatchBeforePrintEvent(
    base::WeakPtr<PrintRenderFrameHelper> weak_this) {
  DCHECK(weak_this);
  source_frame()->DispatchBeforePrintEvent(weak_this);
}

void PrintRenderFrameHelper::PrintPreviewContext::DispatchAfterPrintEvent() {
  source_frame()->DispatchAfterPrintEvent();
}

void PrintRenderFrameHelper::PrintPreviewContext::OnPrintPreview() {
  DCHECK_EQ(State::kInitialized, state_);
  ClearContext();
}

bool PrintRenderFrameHelper::PrintPreviewContext::CreatePreviewDocument(
    std::unique_ptr<PrepareFrameAndViewForPrint> prepared_frame,
    const PageRanges& pages,
    mojom::SkiaDocumentType doc_type,
    int document_cookie,
    bool require_document_metafile) {
  DCHECK_EQ(State::kInitialized, state_);
  state_ = State::kRendering;

  // Need to make sure old object gets destroyed first.
  prep_frame_view_ = std::move(prepared_frame);

  total_page_count_ = prep_frame_view_->GetPageCount();
  if (total_page_count_ == 0 || total_page_count_ > kMaxPageCount) {
    LOG(ERROR) << "CreatePreviewDocument got 0 page count or it's greater than "
                  "kMaxPageCount.";
    set_error(PrintPreviewErrorBuckets::kZeroPages);
    return false;
  }

  if (require_document_metafile) {
    metafile_ = std::make_unique<MetafileSkia>(doc_type, document_cookie);
    CHECK(metafile_->Init());
  }

  current_page_index_ = 0;
  pages_to_render_ = PageNumber::GetPages(pages, total_page_count_);
  // If preview settings along with specified ranges resulted in 0 pages,
  // (e.g. page "2" with a document of a single page), print the entire
  // document. This is a legacy behavior that only makes sense for preview,
  // where the client expects that and will adjust page ranges based on
  // actual document returned.
  if (pages_to_render_.empty())
    pages_to_render_ = PageNumber::GetPages({}, total_page_count_);
  print_ready_metafile_page_count_ = pages_to_render_.size();

  document_render_time_ = base::TimeDelta();
  begin_time_ = base::TimeTicks::Now();

  return true;
}

void PrintRenderFrameHelper::PrintPreviewContext::RenderedPreviewPage(
    const base::TimeDelta& page_time) {
  DCHECK_EQ(State::kRendering, state_);
  document_render_time_ += page_time;
}

void PrintRenderFrameHelper::PrintPreviewContext::RenderedPreviewDocument(
    const base::TimeDelta document_time) {
  DCHECK_EQ(State::kRendering, state_);
  document_render_time_ += document_time;
}

void PrintRenderFrameHelper::PrintPreviewContext::AllPagesRendered() {
  DCHECK_EQ(State::kRendering, state_);
  state_ = State::kDone;
  prep_frame_view_->FinishPrinting();
}

void PrintRenderFrameHelper::PrintPreviewContext::FinalizePrintReadyDocument() {
  DCHECK(IsRendering());

  base::TimeTicks begin_time = base::TimeTicks::Now();

  if (metafile_)
    metafile_->FinishDocument();

  if (print_ready_metafile_page_count_ <= 0) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  base::TimeDelta total_time =
      (base::TimeTicks::Now() - begin_time) + document_render_time_;
  base::TimeDelta avg_time_per_page = total_time / pages_to_render_.size();

  base::UmaHistogramMediumTimes("PrintPreview.RenderToPDFTime",
                                document_render_time_);
  base::UmaHistogramMediumTimes("PrintPreview.RenderAndGeneratePDFTime",
                                total_time);
  base::UmaHistogramMediumTimes(
      "PrintPreview.RenderAndGeneratePDFTimeAvgPerPage", avg_time_per_page);
}

void PrintRenderFrameHelper::PrintPreviewContext::Finished() {
  DCHECK_EQ(State::kDone, state_);
  state_ = State::kInitialized;
  ClearContext();
}

void PrintRenderFrameHelper::PrintPreviewContext::Failed(bool report_error) {
  DCHECK(state_ != State::kUninitialized);
  state_ = State::kInitialized;
  if (report_error) {
    DCHECK_NE(PrintPreviewErrorBuckets::kNone, error_);
    const char* name = "PrintPreview.RendererError";
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (is_for_arc_)
      name = "Arc.PrintPreview.RendererError";
#endif
    base::UmaHistogramEnumeration(name, error_,
                                  PrintPreviewErrorBuckets::kLastEnum);
  }
  ClearContext();
}

uint32_t PrintRenderFrameHelper::PrintPreviewContext::GetNextPageIndex() {
  DCHECK_EQ(State::kRendering, state_);
  if (IsFinalPageRendered())
    return kInvalidPageIndex;
  return pages_to_render_[current_page_index_++];
}

bool PrintRenderFrameHelper::PrintPreviewContext::IsRendering() const {
  return state_ == State::kRendering || state_ == State::kDone;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool PrintRenderFrameHelper::PrintPreviewContext::IsForArc() const {
  DCHECK_NE(state_, State::kUninitialized);
  return is_for_arc_;
}
#endif

bool PrintRenderFrameHelper::PrintPreviewContext::IsPlugin() const {
  DCHECK(state_ != State::kUninitialized);
  return is_plugin_;
}

bool PrintRenderFrameHelper::PrintPreviewContext::IsModifiable() const {
  DCHECK(state_ != State::kUninitialized);
  return is_modifiable_;
}

bool PrintRenderFrameHelper::PrintPreviewContext::HasSelection() {
  return IsModifiable() && source_frame()->HasSelection();
}

bool PrintRenderFrameHelper::PrintPreviewContext::
    IsLastPageOfPrintReadyMetafile() const {
  DCHECK(IsRendering());
  return current_page_index_ == print_ready_metafile_page_count_;
}

bool PrintRenderFrameHelper::PrintPreviewContext::IsFinalPageRendered() const {
  DCHECK(IsRendering());
  return static_cast<size_t>(current_page_index_) == pages_to_render_.size();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrintRenderFrameHelper::PrintPreviewContext::SetIsForArc(bool is_for_arc) {
  is_for_arc_ = is_for_arc;
}
#endif

void PrintRenderFrameHelper::PrintPreviewContext::set_error(
    enum PrintPreviewErrorBuckets error) {
  error_ = error;
}

blink::WebLocalFrame*
PrintRenderFrameHelper::PrintPreviewContext::source_frame() {
  DCHECK(state_ != State::kUninitialized);
  return source_frame_.GetFrame();
}

const blink::WebNode& PrintRenderFrameHelper::PrintPreviewContext::source_node()
    const {
  DCHECK(state_ != State::kUninitialized);
  return source_node_;
}

blink::WebLocalFrame*
PrintRenderFrameHelper::PrintPreviewContext::prepared_frame() {
  DCHECK(state_ != State::kUninitialized);
  return prep_frame_view_->frame();
}

const blink::WebNode&
PrintRenderFrameHelper::PrintPreviewContext::prepared_node() const {
  DCHECK(state_ != State::kUninitialized);
  return prep_frame_view_->node();
}

uint32_t PrintRenderFrameHelper::PrintPreviewContext::total_page_count() const {
  DCHECK(state_ != State::kUninitialized);
  return total_page_count_;
}

const std::vector<uint32_t>&
PrintRenderFrameHelper::PrintPreviewContext::pages_to_render() const {
  DCHECK_EQ(State::kRendering, state_);
  return pages_to_render_;
}

size_t PrintRenderFrameHelper::PrintPreviewContext::pages_rendered_count()
    const {
  DCHECK_EQ(State::kDone, state_);
  return pages_to_render_.size();
}

MetafileSkia* PrintRenderFrameHelper::PrintPreviewContext::metafile() {
  DCHECK(IsRendering());
  return metafile_.get();
}

ContentProxySet*
PrintRenderFrameHelper::PrintPreviewContext::typeface_content_info() {
  DCHECK(IsRendering());
  return &typeface_content_info_;
}

void PrintRenderFrameHelper::PrintPreviewContext::ClearContext() {
  prep_frame_view_.reset();
  metafile_.reset();
  typeface_content_info_.clear();
  pages_to_render_.clear();
  error_ = PrintPreviewErrorBuckets::kNone;
}

void PrintRenderFrameHelper::PrintPreviewContext::CalculatePluginAttributes() {
  is_plugin_ = !!source_frame()->GetPluginToPrint(source_node_);
  is_modifiable_ = !IsPrintingPdfFrame(source_frame(), source_node_);
  RecordDebugEvent(is_plugin_ ? DebugEvent::kPrintPreviewForPlugin
                              : DebugEvent::kPrintPreviewForNonPlugin);
  RecordDebugEvent(is_modifiable_ ? DebugEvent::kPrintPreviewIsModifiable
                                  : DebugEvent::kPrintPreviewIsNotModifiable);
}

void PrintRenderFrameHelper::SetPrintPagesParams(
    const mojom::PrintPagesParams& settings) {
  CHECK(PrintMsgPrintParamsIsValid(*settings.params));
  print_pages_params_ = settings.Clone();
}

void PrintRenderFrameHelper::QuitScriptedPrintPreviewRunLoop() {
  closures_for_mojo_responses_->RunScriptedPrintPreviewQuitClosure();
}

PrintRenderFrameHelper::ScopedIPC::ScopedIPC(
    base::WeakPtr<PrintRenderFrameHelper> weak_this)
    : weak_this_(std::move(weak_this)) {
  DCHECK(weak_this_);
  weak_this_->IPCReceived();
}

PrintRenderFrameHelper::ScopedIPC::~ScopedIPC() {
  if (weak_this_)
    weak_this_->IPCProcessed();
}

PrintRenderFrameHelper::ScriptingThrottler::ScriptingThrottler() = default;

bool PrintRenderFrameHelper::ScriptingThrottler::IsAllowed(
    blink::WebLocalFrame* frame) {
  const int kMinSecondsToIgnoreJavascriptInitiatedPrint = 2;
  const int kMaxSecondsToIgnoreJavascriptInitiatedPrint = 32;
  bool too_frequent = false;

  // Check if there is script repeatedly trying to print and ignore it if too
  // frequent.  The first 3 times, we use a constant wait time, but if this
  // gets excessive, we switch to exponential wait time. So for a page that
  // calls print() in a loop the user will need to cancel the print dialog
  // after: [2, 2, 2, 4, 8, 16, 32, 32, ...] seconds.
  // This gives the user time to navigate from the page.
  if (count_ > 0) {
    base::TimeDelta diff = base::Time::Now() - last_print_;
    int min_wait_seconds = kMinSecondsToIgnoreJavascriptInitiatedPrint;
    if (count_ > 3) {
      min_wait_seconds =
          std::min(kMinSecondsToIgnoreJavascriptInitiatedPrint << (count_ - 3),
                   kMaxSecondsToIgnoreJavascriptInitiatedPrint);
    }
    if (diff.InSeconds() < min_wait_seconds) {
      too_frequent = true;
    }
  }

  if (!too_frequent) {
    ++count_;
    last_print_ = base::Time::Now();
    return true;
  }

  blink::WebString message(
      blink::WebString::FromASCII("Ignoring too frequent calls to print()."));
  frame->AddMessageToConsole(blink::WebConsoleMessage(
      blink::mojom::ConsoleMessageLevel::kWarning, message));
  return false;
}

void PrintRenderFrameHelper::ScriptingThrottler::Reset() {
  // Reset counter on successful print.
  count_ = 0;
}

}  // namespace printing
