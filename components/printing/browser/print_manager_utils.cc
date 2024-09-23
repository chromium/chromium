// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_manager_utils.h"

#include "components/printing/browser/print_composite_client.h"
#include "components/printing/common/print.mojom.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"

namespace printing {

namespace {

// A temporary flag which makes supporting both paths for OOPIF and non-OOPIF
// printing easier.
bool g_oopif_enabled = false;

void SetOopifEnabled() {
  g_oopif_enabled = true;
}

}  // namespace

bool IsOopifEnabled() {
  return g_oopif_enabled;
}

void CreateCompositeClientIfNeeded(content::WebContents* web_contents,
                                   const std::string& user_agent) {
  // TODO(crbug.com/40657857): Once ShouldPdfCompositorBeEnabledForOopifs()
  // always returns true, just remove the check altogether.
  if (site_isolation::SiteIsolationPolicy::
          ShouldPdfCompositorBeEnabledForOopifs()) {
    PrintCompositeClient::CreateForWebContents(web_contents);
    PrintCompositeClient::FromWebContents(web_contents)
        ->SetUserAgent(user_agent);
    SetOopifEnabled();
  }
}

void RenderParamsFromPrintSettings(const PrintSettings& settings,
                                   mojom::PrintParams* params) {
  const auto& page_setup = settings.page_setup_device_units();
  params->page_size = gfx::SizeF(page_setup.physical_size());
  params->content_size = gfx::SizeF(page_setup.content_area().size());
  params->printable_area = gfx::RectF(page_setup.printable_area());
  params->margin_top = page_setup.content_area().y();
  params->margin_left = page_setup.content_area().x();
  params->dpi = settings.dpi_size();
  params->scale_factor = settings.scale_factor();
  params->rasterize_pdf = settings.rasterize_pdf();
  params->rasterize_pdf_dpi = settings.rasterize_pdf_dpi();
  // Always use an invalid cookie.
  params->document_cookie = PrintSettings::NewInvalidCookie();
  params->selection_only = settings.selection_only();
  params->should_print_backgrounds = settings.should_print_backgrounds();
  params->display_header_footer = settings.display_header_footer();
  params->title = settings.title();
  params->url = settings.url();
  params->printed_doc_type = IsOopifEnabled() && settings.is_modifiable()
                                 ? mojom::SkiaDocumentType::kMSKP
                                 : mojom::SkiaDocumentType::kPDF;
  params->pages_per_sheet = settings.pages_per_sheet();
}

}  // namespace printing
