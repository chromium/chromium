// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_
#define COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "pdf/pdf_view_web_plugin.h"

namespace content {
class RenderFrame;
class V8ValueConverter;
}  // namespace content

namespace pdf {

class PdfViewWebPluginClient : public chrome_pdf::PdfViewWebPlugin::Client {
 public:
  explicit PdfViewWebPluginClient(content::RenderFrame* render_frame);
  PdfViewWebPluginClient(const PdfViewWebPluginClient&) = delete;
  PdfViewWebPluginClient& operator=(const PdfViewWebPluginClient&) = delete;
  ~PdfViewWebPluginClient() override;

  // chrome_pdf::PdfViewWebPlugin::Client:
  std::unique_ptr<base::Value> FromV8Value(
      v8::Local<v8::Value> value,
      v8::Local<v8::Context> context) override;
  v8::Local<v8::Value> ToV8Value(const base::Value& value,
                                 v8::Local<v8::Context> context) override;
  base::WeakPtr<chrome_pdf::PdfViewWebPlugin::Client> GetWeakPtr() override;
  void Print(const blink::WebElement& element) override;
  void RecordComputedAction(const std::string& action) override;
  std::unique_ptr<chrome_pdf::PdfAccessibilityDataHandler>
  CreateAccessibilityDataHandler(
      chrome_pdf::PdfAccessibilityActionHandler* action_handler) override;
  bool IsUseZoomForDSFEnabled() const override;

 private:
  content::RenderFrame* const render_frame_;

  const std::unique_ptr<content::V8ValueConverter> v8_value_converter_;

  base::WeakPtrFactory<PdfViewWebPluginClient> weak_factory_{this};
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_
