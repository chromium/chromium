// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_
#define COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "pdf/pdf_view_web_plugin.h"

namespace blink {
class WebLocalFrame;
class WebPluginContainer;
}  // namespace blink

namespace content {
class RenderFrame;
class V8ValueConverter;
}  // namespace content

namespace v8 {
class Isolate;
}  // namespace v8

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
  base::WeakPtr<chrome_pdf::PdfViewWebPlugin::Client> GetWeakPtr() override;
  void SetPluginContainer(blink::WebPluginContainer* container) override;
  blink::WebPluginContainer* PluginContainer() override;
  net::SiteForCookies SiteForCookies() const override;
  blink::WebURL CompleteURL(const blink::WebString& partial_url) const override;
  void PostMessage(base::Value::Dict message) override;
  void Invalidate() override;
  void RequestTouchEventType(
      blink::WebPluginContainer::TouchEventRequestType request_type) override;
  void ReportFindInPageMatchCount(int identifier,
                                  int total,
                                  bool final_update) override;
  void ReportFindInPageSelection(int identifier,
                                 int index,
                                 bool final_update) override;
  void ReportFindInPageTickmarks(
      const std::vector<gfx::Rect>& tickmarks) override;
  float DeviceScaleFactor() override;
  gfx::PointF GetScrollPosition() override;
  void UsePluginAsFindHandler() override;
  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override;
  void Alert(const blink::WebString& message) override;
  bool Confirm(const blink::WebString& message) override;
  blink::WebString Prompt(const blink::WebString& message,
                          const blink::WebString& default_value) override;
  void TextSelectionChanged(const blink::WebString& selection_text,
                            uint32_t offset,
                            const gfx::Range& range) override;
  std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) override;
  void UpdateTextInputState() override;
  void UpdateSelectionBounds() override;
  std::string GetEmbedderOriginString() override;
  bool HasFrame() const override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void Print() override;
  void RecordComputedAction(const std::string& action) override;
  std::unique_ptr<chrome_pdf::PdfAccessibilityDataHandler>
  CreateAccessibilityDataHandler(
      chrome_pdf::PdfAccessibilityActionHandler* action_handler) override;

 private:
  blink::WebLocalFrame* GetFrame() const;

  content::RenderFrame* const render_frame_;

  const std::unique_ptr<content::V8ValueConverter> v8_value_converter_;
  v8::Isolate* const isolate_;

  blink::WebPluginContainer* plugin_container_;

  base::WeakPtrFactory<PdfViewWebPluginClient> weak_factory_{this};
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_VIEW_WEB_PLUGIN_CLIENT_H_
