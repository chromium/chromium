// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_view_web_plugin_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/values.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "net/cookies/site_for_cookies.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/accessibility/ax_features.mojom-features.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
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
      isolate_(
          render_frame->GetWebFrame()->GetAgentGroupScheduler()->Isolate()) {
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

v8::Isolate* PdfViewWebPluginClient::GetIsolate() {
  return GetFrame()->GetAgentGroupScheduler()->Isolate();
}

net::SiteForCookies PdfViewWebPluginClient::SiteForCookies() const {
  return plugin_container_->GetDocument().SiteForCookies();
}

blink::WebURL PdfViewWebPluginClient::CompleteURL(
    const blink::WebString& partial_url) const {
  return plugin_container_->GetDocument().CompleteURL(partial_url);
}

void PdfViewWebPluginClient::PostMessage(base::Value::Dict message) {
  blink::WebLocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }

  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  DCHECK_EQ(isolate_, context->GetIsolate());
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> converted_message =
      v8_value_converter_->ToV8Value(message, context);

  plugin_container_->EnqueueMessageEvent(
      blink::WebSerializedScriptValue::Serialize(isolate_, converted_message));
}

void PdfViewWebPluginClient::Invalidate() {
  plugin_container_->Invalidate();
}

void PdfViewWebPluginClient::RequestTouchEventType(
    blink::WebPluginContainer::TouchEventRequestType request_type) {
  plugin_container_->RequestTouchEventType(request_type);
}

void PdfViewWebPluginClient::ReportFindInPageMatchCount(int identifier,
                                                        int total,
                                                        bool final_update) {
  plugin_container_->ReportFindInPageMatchCount(identifier, total,
                                                final_update);
}

void PdfViewWebPluginClient::ReportFindInPageSelection(int identifier,
                                                       int index,
                                                       bool final_update) {
  plugin_container_->ReportFindInPageSelection(identifier, index, final_update);
}

void PdfViewWebPluginClient::ReportFindInPageTickmarks(
    const std::vector<gfx::Rect>& tickmarks) {
  blink::WebLocalFrame* frame = GetFrame();
  if (frame) {
    frame->SetTickmarks(blink::WebElement(),
                        blink::WebVector<gfx::Rect>(tickmarks));
  }
}

float PdfViewWebPluginClient::DeviceScaleFactor() {
  // Do not rely on `blink::WebPluginContainer::DeviceScaleFactor()`, since it
  // doesn't always reflect the real screen's device scale. Instead, get the
  // device scale from the top-level frame's `display::ScreenInfo`.
  blink::WebWidget* widget = GetFrame()->LocalRoot()->FrameWidget();
  return widget->GetOriginalScreenInfo().device_scale_factor;
}

gfx::PointF PdfViewWebPluginClient::GetScrollPosition() {
  // Note that `blink::WebLocalFrame::GetScrollOffset()` actually returns a
  // scroll position (a point relative to the top-left corner).
  return GetFrame()->GetScrollOffset();
}

void PdfViewWebPluginClient::UsePluginAsFindHandler() {
  plugin_container_->UsePluginAsFindHandler();
}

void PdfViewWebPluginClient::SetReferrerForRequest(
    blink::WebURLRequest& request,
    const blink::WebURL& referrer_url) {
  GetFrame()->SetReferrerForRequest(request, referrer_url);
}

void PdfViewWebPluginClient::Alert(const blink::WebString& message) {
  blink::WebLocalFrame* frame = GetFrame();
  if (frame)
    frame->Alert(message);
}

bool PdfViewWebPluginClient::Confirm(const blink::WebString& message) {
  blink::WebLocalFrame* frame = GetFrame();
  return frame && frame->Confirm(message);
}

blink::WebString PdfViewWebPluginClient::Prompt(
    const blink::WebString& message,
    const blink::WebString& default_value) {
  blink::WebLocalFrame* frame = GetFrame();
  return frame ? frame->Prompt(message, default_value) : blink::WebString();
}

void PdfViewWebPluginClient::TextSelectionChanged(
    const blink::WebString& selection_text,
    uint32_t offset,
    const gfx::Range& range) {
  // Focus the plugin's containing frame before changing the text selection.
  // TODO(crbug.com/40192026): Would it make more sense not to change the text
  // selection at all in this case? Maybe we only have this problem because we
  // support a "selectAll" message.
  blink::WebLocalFrame* frame = GetFrame();
  frame->View()->SetFocusedFrame(frame);

  frame->TextSelectionChanged(selection_text, offset, range);
}

std::unique_ptr<blink::WebAssociatedURLLoader>
PdfViewWebPluginClient::CreateAssociatedURLLoader(
    const blink::WebAssociatedURLLoaderOptions& options) {
  return GetFrame()->CreateAssociatedURLLoader(options);
}

void PdfViewWebPluginClient::PerformOcr(
    const SkBitmap& image,
    base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)> callback) {
  CHECK(base::FeatureList::IsEnabled(ax::mojom::features::kScreenAIOCREnabled));

  if (!screen_ai_annotator_.is_bound()) {
    render_frame_->GetBrowserInterfaceBroker().GetInterface(
        screen_ai_annotator_.BindNewPipeAndPassReceiver());
    screen_ai_annotator_->SetClientType(
        screen_ai::mojom::OcrClientType::kPdfViewer);
    screen_ai_annotator_.set_disconnect_handler(
        base::BindOnce(&PdfViewWebPluginClient::OnOcrDisconnected,
                       weak_factory_.GetWeakPtr()));
  }
  screen_ai_annotator_->PerformOcrAndReturnAnnotation(image,
                                                      std::move(callback));
}

void PdfViewWebPluginClient::SetOcrDisconnectedCallback(
    base::RepeatingClosure callback) {
  ocr_disconnect_callback_ = std::move(callback);
}

void PdfViewWebPluginClient::OnOcrDisconnected() {
  screen_ai_annotator_.reset();
  CHECK(ocr_disconnect_callback_);
  ocr_disconnect_callback_.Run();
}

void PdfViewWebPluginClient::UpdateTextInputState() {
  // `widget` is null in Print Preview.
  auto* widget = GetFrame()->FrameWidget();
  if (widget)
    widget->UpdateTextInputState();
}

void PdfViewWebPluginClient::UpdateSelectionBounds() {
  // `widget` is null in Print Preview.
  auto* widget = GetFrame()->FrameWidget();
  if (widget)
    widget->UpdateSelectionBounds();
}

std::string PdfViewWebPluginClient::GetEmbedderOriginString() {
  auto* frame = GetFrame();
  if (!frame)
    return {};

  auto* parent_frame = frame->Parent();
  if (!parent_frame)
    return {};

  return GURL(parent_frame->GetSecurityOrigin().ToString().Utf8()).spec();
}

bool PdfViewWebPluginClient::HasFrame() const {
  return plugin_container_ && GetFrame();
}

blink::WebLocalFrame* PdfViewWebPluginClient::GetFrame() const {
  return plugin_container_->GetDocument().GetFrame();
}

void PdfViewWebPluginClient::DidStartLoading() {
  blink::WebLocalFrameClient* frame_client = GetFrame()->Client();
  if (!frame_client)
    return;

  frame_client->DidStartLoading();
}

void PdfViewWebPluginClient::DidStopLoading() {
  blink::WebLocalFrameClient* frame_client = GetFrame()->Client();
  if (!frame_client)
    return;

  frame_client->DidStopLoading();
}

void PdfViewWebPluginClient::Print() {
  blink::WebElement element = plugin_container_->GetElement();
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
    chrome_pdf::PdfAccessibilityActionHandler* action_handler,
    chrome_pdf::PdfAccessibilityImageFetcher* image_fetcher,
    blink::WebPluginContainer* plugin_container,
    bool print_preview) {
  return std::make_unique<PdfAccessibilityTree>(render_frame_, action_handler,
                                                image_fetcher, plugin_container,
                                                print_preview);
}

}  // namespace pdf
