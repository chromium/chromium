// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pdf/chrome_pdf_internal_plugin_delegate.h"

#include <memory>

#include "base/check.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/webui_url_constants.h"
#include "pdf/pdf_view_web_plugin.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_frame.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/renderer/pdf/chrome_pdf_view_web_plugin_print_client.h"
#endif  // BUILDFLAG(ENABLE_PRINTING)

ChromePdfInternalPluginDelegate::ChromePdfInternalPluginDelegate(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {
  DCHECK(render_frame_);
}

ChromePdfInternalPluginDelegate::~ChromePdfInternalPluginDelegate() = default;

bool ChromePdfInternalPluginDelegate::IsAllowedFrame(
    const blink::WebFrame& frame) const {
  // The in-process plugin should only be created if the parent frame has an
  // allowed origin.
  const blink::WebFrame* parent_frame = frame.Parent();
  if (!parent_frame) {
    // TODO(crbug.com/1225756): Until this is fixed, allow Print Preview to
    // create the in-process plugin directly within its own frames.
    return frame.GetSecurityOrigin().IsSameOriginWith(
        blink::WebSecurityOrigin::Create(GURL(chrome::kChromeUIPrintURL)));
  }

  return IsPdfInternalPluginAllowedOrigin(parent_frame->GetSecurityOrigin());
}

std::unique_ptr<chrome_pdf::PdfViewWebPlugin::PrintClient>
ChromePdfInternalPluginDelegate::CreatePrintClient() {
#if BUILDFLAG(ENABLE_PRINTING)
  return std::make_unique<ChromePdfViewWebPluginPrintClient>(render_frame_);
#else   // !BUILDFLAG(ENABLE_PRINTING)
  return nullptr;
#endif  // BUILDFLAG(ENABLE_PRINTING)
}
