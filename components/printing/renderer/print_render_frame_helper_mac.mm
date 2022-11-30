// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_render_frame_helper.h"

#import <AppKit/AppKit.h>

#include <memory>

#include "base/check.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/metrics/histogram.h"
#include "cc/paint/paint_canvas.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace printing {

void PrintRenderFrameHelper::PrintPageInternal(const mojom::PrintParams& params,
                                               uint32_t page_number,
                                               uint32_t page_count,
                                               double scale_factor,
                                               blink::WebLocalFrame* frame,
                                               MetafileSkia* metafile,
                                               gfx::Size* page_size_in_dpi,
                                               gfx::Rect* content_rect_in_dpi) {
  double css_scale_factor = scale_factor;
  mojom::PageSizeMarginsPtr page_layout_in_points =
      ComputePageLayoutInPointsForCss(frame, page_number, params,
                                      ignore_css_margins_, &css_scale_factor);

  gfx::Size page_size;
  gfx::Rect content_area;
  GetPageSizeAndContentAreaFromPageLayout(*page_layout_in_points, &page_size,
                                          &content_area);

  if (page_size_in_dpi)
    *page_size_in_dpi = page_size;

  if (content_rect_in_dpi)
    *content_rect_in_dpi = content_area;

  gfx::Rect canvas_area =
      params.display_header_footer ? gfx::Rect(page_size) : content_area;

  double webkit_page_shrink_factor = frame->GetPrintPageShrink(page_number);
  float final_scale_factor = css_scale_factor * webkit_page_shrink_factor;

  cc::PaintCanvas* canvas = metafile->GetVectorCanvasForNewPage(
      page_size, canvas_area, final_scale_factor, params.page_orientation);
  if (!canvas)
    return;

  canvas->SetPrintingMetafile(metafile);
  if (params.display_header_footer) {
    PrintHeaderAndFooter(canvas, page_number + 1, page_count, *frame,
                         final_scale_factor, *page_layout_in_points, params);
  }
  RenderPageContent(frame, page_number, canvas_area, content_area,
                    final_scale_factor, canvas);

  // Done printing. Close the canvas to retrieve the compiled metafile.
  bool ret = metafile->FinishPage();
  DCHECK(ret);
}

}  // namespace printing
