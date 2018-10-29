// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/pdf_plugin_placeholder.h"

#include "chrome/common/pdf_util.h"
#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_thread.h"
#include "gin/object_template_builder.h"

gin::WrapperInfo PDFPluginPlaceholder::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
PDFPluginPlaceholder* PDFPluginPlaceholder::CreatePDFPlaceholder(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  std::string html_data = GetPDFPlaceholderHTML(params.url);
  return new PDFPluginPlaceholder(render_frame, params, html_data);
}

PDFPluginPlaceholder::PDFPluginPlaceholder(content::RenderFrame* render_frame,
                                           const blink::WebPluginParams& params,
                                           const std::string& html_data)
    : plugins::PluginPlaceholderBase(render_frame, params, html_data) {}

PDFPluginPlaceholder::~PDFPluginPlaceholder() {}

v8::Local<v8::Value> PDFPluginPlaceholder::GetV8Handle(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, this).ToV8();
}

gin::ObjectTemplateBuilder PDFPluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PDFPluginPlaceholder>::GetObjectTemplateBuilder(isolate)
      .SetMethod<void (PDFPluginPlaceholder::*)()>(
          "openPDF", &PDFPluginPlaceholder::OpenPDFCallback);
}

void PDFPluginPlaceholder::OpenPDFCallback() {
  ReportPDFLoadStatus(PDFLoadStatus::kViewPdfClickedInPdfPluginPlaceholder);
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_OpenPDF(routing_id(), GetPluginParams().url));
}
