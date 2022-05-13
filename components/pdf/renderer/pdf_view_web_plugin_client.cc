// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_view_web_plugin_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/values.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/renderer/print_render_frame_helper.h"
#endif  // BUILDFLAG(ENABLE_PRINTING)

namespace pdf {

PdfViewWebPluginClient::PdfViewWebPluginClient(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame),
      v8_value_converter_(content::V8ValueConverter::Create()),
      isolate_(blink::MainThreadIsolate()) {
  DCHECK(render_frame_);
}

PdfViewWebPluginClient::~PdfViewWebPluginClient() = default;

std::unique_ptr<base::Value> PdfViewWebPluginClient::FromV8Value(
    v8::Local<v8::Value> value,
    v8::Local<v8::Context> context) {
  return v8_value_converter_->FromV8Value(value, context);
}

base::WeakPtr<chrome_pdf::PdfViewWebPlugin::Client>
PdfViewWebPluginClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PdfViewWebPluginClient::SetPluginContainer(
    blink::WebPluginContainer* container) {
  plugin_container_ = container;
}

blink::WebPluginContainer* PdfViewWebPluginClient::PluginContainer() {
  return plugin_container_;
}

void PdfViewWebPluginClient::PostMessage(base::Value::Dict message) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      plugin_container_->GetDocument().GetFrame()->MainWorldScriptContext();
  DCHECK_EQ(isolate_, context->GetIsolate());
  v8::Context::Scope context_scope(context);

  base::Value message_as_value(std::move(message));
  v8::Local<v8::Value> converted_message =
      v8_value_converter_->ToV8Value(&message_as_value, context);

  plugin_container_->EnqueueMessageEvent(
      blink::WebSerializedScriptValue::Serialize(isolate_, converted_message));
}

void PdfViewWebPluginClient::Print(const blink::WebElement& element) {
  DCHECK(!element.IsNull());
#if BUILDFLAG(ENABLE_PRINTING)
  printing::PrintRenderFrameHelper::Get(render_frame_)->PrintNode(element);
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

void PdfViewWebPluginClient::RecordComputedAction(const std::string& action) {
  content::RenderThread::Get()->RecordComputedAction(action);
}

std::unique_ptr<chrome_pdf::PdfAccessibilityDataHandler>
PdfViewWebPluginClient::CreateAccessibilityDataHandler(
    chrome_pdf::PdfAccessibilityActionHandler* action_handler) {
  return std::make_unique<PdfAccessibilityTree>(render_frame_, action_handler);
}

}  // namespace pdf
