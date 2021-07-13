// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_INTERNAL_PLUGIN_DELEGATE_H_
#define COMPONENTS_PDF_RENDERER_PDF_INTERNAL_PLUGIN_DELEGATE_H_

#include <memory>

// TODO(crbug.com/1218971): Refactor this; only needed for
// `chrome_pdf::PdfViewWebPlugin::PrintClient` declaration.
#include "pdf/pdf_view_web_plugin.h"

namespace pdf {

// Interface for embedder-provided operations required to create an instance of
// the internal PDF plugin.
class PdfInternalPluginDelegate {
 public:
  PdfInternalPluginDelegate();
  virtual ~PdfInternalPluginDelegate();

  // Creates the print client, or `nullptr` if printing is not supported.
  virtual std::unique_ptr<chrome_pdf::PdfViewWebPlugin::PrintClient>
  CreatePrintClient();
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_INTERNAL_PLUGIN_DELEGATE_H_
