// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_manager_utils.h"

#include "components/printing/browser/print_composite_client.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/site_isolation_policy.h"
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
  // TODO(weili): We only create pdf compositor client and use pdf compositor
  // service when site-per-process or isolate-origins flag/feature is enabled,
  // or top-document-isolation feature is enabled. This may not cover all cases
  // where OOPIF is used such as isolate-extensions, but should be good for
  // feature testing purpose. Eventually, we will remove this check and use pdf
  // compositor service by default for printing.
  if (content::SiteIsolationPolicy::ShouldPdfCompositorBeEnabledForOopifs()) {
    PrintCompositeClient::CreateForWebContents(web_contents);
    PrintCompositeClient::FromWebContents(web_contents)
        ->SetUserAgent(user_agent);
    SetOopifEnabled();
  }
}

void RenderParamsFromPrintSettings(const PrintSettings& settings,
                                   PrintMsg_Print_Params* params) {
  params->page_size = settings.page_setup_device_units().physical_size();
  params->content_size.SetSize(
      settings.page_setup_device_units().content_area().width(),
      settings.page_setup_device_units().content_area().height());
  params->printable_area.SetRect(
      settings.page_setup_device_units().printable_area().x(),
      settings.page_setup_device_units().printable_area().y(),
      settings.page_setup_device_units().printable_area().width(),
      settings.page_setup_device_units().printable_area().height());
  params->margin_top = settings.page_setup_device_units().content_area().y();
  params->margin_left = settings.page_setup_device_units().content_area().x();
  params->dpi = settings.dpi_size();
  params->scale_factor = settings.scale_factor();
  params->rasterize_pdf = settings.rasterize_pdf();
  // Always use an invalid cookie.
  params->document_cookie = 0;
  params->selection_only = settings.selection_only();
  params->supports_alpha_blend = settings.supports_alpha_blend();
  params->should_print_backgrounds = settings.should_print_backgrounds();
  params->display_header_footer = settings.display_header_footer();
  params->title = settings.title();
  params->url = settings.url();
  params->printed_doc_type = IsOopifEnabled() && settings.is_modifiable()
                                 ? SkiaDocumentType::MSKP
                                 : SkiaDocumentType::PDF;
  params->pages_per_sheet = settings.pages_per_sheet();
}

}  // namespace printing
