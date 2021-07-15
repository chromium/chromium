// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_internal_plugin_delegate.h"

#include <memory>

#include "pdf/pdf_view_web_plugin.h"

namespace pdf {

PdfInternalPluginDelegate::PdfInternalPluginDelegate() = default;
PdfInternalPluginDelegate::~PdfInternalPluginDelegate() = default;

bool PdfInternalPluginDelegate::IsAllowedFrame(
    const blink::WebFrame& frame) const {
  return false;
}

std::unique_ptr<chrome_pdf::PdfViewWebPlugin::PrintClient>
PdfInternalPluginDelegate::CreatePrintClient() {
  return nullptr;
}

}  // namespace pdf
