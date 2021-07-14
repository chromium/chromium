// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pdf/chrome_pdf_internal_plugin_delegate.h"

#include <memory>

#include "base/check.h"
#include "chrome/common/pdf_util.h"
#include "pdf/pdf_view_web_plugin.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/renderer/pdf/chrome_pdf_view_web_plugin_print_client.h"
#endif  // BUILDFLAG(ENABLE_PRINTING)

ChromePdfInternalPluginDelegate::ChromePdfInternalPluginDelegate(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {
  DCHECK(render_frame_);
}

ChromePdfInternalPluginDelegate::~ChromePdfInternalPluginDelegate() = default;

bool ChromePdfInternalPluginDelegate::IsAllowedOrigin(
    const url::Origin& origin) const {
  return IsPdfInternalPluginAllowedOrigin(origin);
}

std::unique_ptr<chrome_pdf::PdfViewWebPlugin::PrintClient>
ChromePdfInternalPluginDelegate::CreatePrintClient() {
#if BUILDFLAG(ENABLE_PRINTING)
  return std::make_unique<ChromePdfViewWebPluginPrintClient>(render_frame_);
#else   // !BUILDFLAG(ENABLE_PRINTING)
  return nullptr;
#endif  // BUILDFLAG(ENABLE_PRINTING)
}
