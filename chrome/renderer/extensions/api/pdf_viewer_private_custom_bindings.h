// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "pdf/buildflags.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2));

namespace extensions {

// Custom bindings for the pdfViewerPrivate API.
class PdfViewerPrivateCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit PdfViewerPrivateCustomBindings(ScriptContext* context);
  PdfViewerPrivateCustomBindings(const PdfViewerPrivateCustomBindings&) =
      delete;
  PdfViewerPrivateCustomBindings& operator=(
      const PdfViewerPrivateCustomBindings&) = delete;
  ~PdfViewerPrivateCustomBindings() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // Represents the javascript function
  // chrome.pdfViewerPrivate.GetTextInfoResult getTextInfo(
  //   textarea: HTMLTextAreaElement, knownFontIds: number[])
  // See also: chrome/common/extensions/api/pdf_viewer_private.idl
  void GetTextInfo(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_CUSTOM_BINDINGS_H_
