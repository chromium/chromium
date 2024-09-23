// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/pdf_plugin_placeholder.h"

#include "base/command_line.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/plugin.mojom.h"
#include "components/pdf/common/pdf_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

gin::WrapperInfo PDFPluginPlaceholder::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
PDFPluginPlaceholder* PDFPluginPlaceholder::CreatePDFPlaceholder(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  std::string html_data = GetPDFPlaceholderHTML(params.url);
  auto* placeholder = new PDFPluginPlaceholder(render_frame, params);
  placeholder->Init(html_data);
  return placeholder;
}

PDFPluginPlaceholder::PDFPluginPlaceholder(content::RenderFrame* render_frame,
                                           const blink::WebPluginParams& params)
    : plugins::PluginPlaceholderBase(render_frame, params) {}

PDFPluginPlaceholder::~PDFPluginPlaceholder() {}

v8::Local<v8::Value> PDFPluginPlaceholder::GetV8Handle(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, this).ToV8();
}

gin::ObjectTemplateBuilder PDFPluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  gin::ObjectTemplateBuilder builder =
      gin::Wrappable<PDFPluginPlaceholder>::GetObjectTemplateBuilder(isolate)
          .SetMethod<void (PDFPluginPlaceholder::*)()>(
              "openPDF", &PDFPluginPlaceholder::OpenPDFCallback);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePluginPlaceholderTesting)) {
    builder.SetMethod<void (PDFPluginPlaceholder::*)()>(
        "notifyPlaceholderReadyForTesting",
        &PDFPluginPlaceholder::NotifyPlaceholderReadyForTestingCallback);
  }

  return builder;
}

void PDFPluginPlaceholder::OpenPDFCallback() {
  ReportPDFLoadStatus(PDFLoadStatus::kViewPdfClickedInPdfPluginPlaceholder);
  mojo::AssociatedRemote<chrome::mojom::PluginHost> plugin_host;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      plugin_host.BindNewEndpointAndPassReceiver());
  plugin_host->OpenPDF(GetPluginParams().url);
}
