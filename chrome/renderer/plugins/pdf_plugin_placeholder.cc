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
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-cppgc.h"

// static
PDFPluginPlaceholder* PDFPluginPlaceholder::CreatePDFPlaceholder(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  std::string html_data = GetPDFPlaceholderHTML(params.url);
  auto* placeholder = cppgc::MakeGarbageCollected<PDFPluginPlaceholder>(
      render_frame->GetWebFrame()
          ->GetAgentGroupScheduler()
          ->Isolate()
          ->GetCppHeap()
          ->GetAllocationHandle(),
      render_frame, params);
  placeholder->Init(html_data);
  return placeholder;
}

PDFPluginPlaceholder::PDFPluginPlaceholder(content::RenderFrame* render_frame,
                                           const blink::WebPluginParams& params)
    : plugins::PluginPlaceholderBase(render_frame, params) {
  self_ = this;
}

PDFPluginPlaceholder::~PDFPluginPlaceholder() = default;

const gin::WrapperInfo* PDFPluginPlaceholder::wrapper_info() const {
  return &kWrapperInfo;
}

v8::Local<v8::Value> PDFPluginPlaceholder::GetV8Handle(v8::Isolate* isolate) {
  return GetWrapper(isolate).ToLocalChecked();
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

void PDFPluginPlaceholder::OnDestruct() {
  self_.Clear();
}

void PDFPluginPlaceholder::OpenPDFCallback() {
  ReportPDFLoadStatus(PDFLoadStatus::kViewPdfClickedInPdfPluginPlaceholder);
  mojo::AssociatedRemote<chrome::mojom::PluginHost> plugin_host;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      plugin_host.BindNewEndpointAndPassReceiver());
  plugin_host->OpenPDF(GetPluginParams().url);
}
