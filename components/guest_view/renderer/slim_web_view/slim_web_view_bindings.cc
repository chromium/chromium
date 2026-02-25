// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/slim_web_view/slim_web_view_bindings.h"

#include "base/no_destructor.h"
#include "components/guest_view/common/guest_view.mojom.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/renderer/guest_view_container.h"
#include "components/guest_view/renderer/guest_view_request.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/v8_value_converter.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace guest_view {

namespace {

// This name matches the name in slim_web_view_guest.h, but we can't depend on
// that header file.
static constexpr char kSlimWebViewType[] = "SlimWebView";

static constexpr char kSlimWebViewPrivate[] = "slimWebViewPrivate";

struct ViewHolder {
  v8::Global<v8::Object> view;
  mojo::Remote<mojom::ViewHandle> keep_alive_handle_remote;
};

class RenderFrameStatus final : public content::RenderFrameObserver {
 public:
  explicit RenderFrameStatus(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame) {}
  ~RenderFrameStatus() final = default;

  bool is_ok() { return render_frame() != nullptr; }

  // RenderFrameObserver implementation.
  void OnDestruct() final {}
};

mojo::AssociatedRemote<guest_view::mojom::GuestViewHost> GetGuestViewHost(
    content::RenderFrame* render_frame) {
  mojo::AssociatedRemote<guest_view::mojom::GuestViewHost> guest_view_host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&guest_view_host);

  return guest_view_host;
}

// A map from view instance ID to ViewHolder (managed via weak V8 reference to
// the view object). Views are registered into this map via RegisterView().
using ViewMap = absl::flat_hash_map<int, ViewHolder>;

ViewMap& GetViewMap() {
  static base::NoDestructor<ViewMap> view_map;
  return *view_map;
}

void ResetMapEntry(const v8::WeakCallbackInfo<void>& data) {
  auto view_instance_id = reinterpret_cast<uintptr_t>(data.GetParameter());
  ViewMap& view_map = GetViewMap();
  auto entry = view_map.find(static_cast<int>(view_instance_id));
  CHECK(entry != view_map.end());
  view_map.erase(entry);
}

int GetNextId() {
  static int next_id = 0;
  return ++next_id;
}

content::RenderFrame* GetRenderFrame(v8::Isolate* isolate,
                                     v8::Local<v8::Object> object) {
  v8::Local<v8::Context> context;
  if (!object->GetCreationContext(isolate).ToLocal(&context)) {
    if (context.IsEmpty()) {
      return nullptr;
    }
  }
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForContext(context);
  if (!frame) {
    return nullptr;
  }
  return content::RenderFrame::FromWebFrame(frame);
}

void RegisterView(v8::Isolate* isolate,
                  int view_instance_id,
                  v8::Local<v8::Object> view) {
  CHECK_NE(view_instance_id, kInstanceIDNone);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForContext(context);
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);

  auto result = GetViewMap().insert(
      {view_instance_id, ViewHolder{v8::Global<v8::Object>(isolate, view)}});
  CHECK(result.second);
  auto& view_holder = result.first->second;
  view_holder.view.SetWeak(
      reinterpret_cast<void*>(static_cast<uintptr_t>(view_instance_id)),
      &ResetMapEntry, v8::WeakCallbackType::kParameter);

  auto receiver =
      view_holder.keep_alive_handle_remote.BindNewPipeAndPassReceiver();
  GetGuestViewHost(render_frame)
      ->ViewCreated(view_instance_id, kSlimWebViewType, std::move(receiver));
}

v8::Local<v8::Value> GetViewFromId(v8::Isolate* isolate, int view_instance_id) {
  const ViewMap& view_map = GetViewMap();
  const auto it = view_map.find(view_instance_id);
  if (it == view_map.end()) {
    return v8::Null(isolate);
  }
  return it->second.view.Get(isolate);
}

void AttachIframeGuest(v8::Isolate* isolate,
                       int container_id,
                       int guest_instance_id,
                       v8::Local<v8::Value> params,
                       v8::Local<v8::Object> content_window,
                       v8::Local<v8::Function> callback) {
  content::RenderFrame* render_frame = GetRenderFrame(isolate, content_window);
  CHECK(render_frame);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  blink::WebFrame* parent_frame = frame->Parent();
  CHECK(parent_frame);
  CHECK(parent_frame->IsWebLocalFrame());
  content::RenderFrame* embedder_parent_frame =
      content::RenderFrame::FromWebFrame(parent_frame->ToWebLocalFrame());

  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(container_id);
  // This is the first time we hear about the |element_instance_id|.
  DCHECK(!guest_view_container);
  // The <webview> element's GC takes ownership of |guest_view_container|
  // in the client code.
  guest_view_container =
      new guest_view::GuestViewContainer(embedder_parent_frame, container_id);
  // We track the status of the RenderFrame via an observer in case it is
  // deleted during user code execution while getting the params.
  RenderFrameStatus render_frame_status(render_frame);
  auto params_value =
      content::V8ValueConverter::Create()->FromV8Value(params, context);
  CHECK(render_frame_status.is_ok());
  CHECK(params_value->is_dict());
  auto request = std::make_unique<guest_view::GuestViewAttachRequest>(
      guest_view_container, render_frame, guest_instance_id,
      std::move(params_value->GetDict()), callback, isolate);
  guest_view_container->IssueRequest(std::move(request));
}

void AllowGuestViewElementDefinition(v8::Isolate* isolate,
                                     v8::Local<v8::Function> callback) {
  auto context = isolate->GetCurrentContext();
  blink::WebCustomElement::EmbedderNamesAllowedScope embedder_names_scope;
  v8::TryCatch try_catch(isolate);
  auto result = callback->Call(context, context->Global(), 0, nullptr);

  if (result.IsEmpty()) {
    v8::String::Utf8Value exception(isolate, try_catch.Exception());
    NOTREACHED() << "AllowGuestViewElementDefinition failed: " << *exception;
  }
}

void DestroyContainer(int container_id) {
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(container_id);
  if (!guest_view_container) {
    return;
  }
  guest_view_container->Destroy(/*embedder_frame_destroyed=*/false);
}

}  // namespace

// static
void SlimWebViewBindings::MaybeInstall(content::RenderFrame& render_frame) {
  if (!render_frame.GetEnabledBindings().Has(
          content::BindingsPolicyValue::kSlimWebView)) {
    return;
  }
  blink::WebLocalFrame* frame = render_frame.GetWebFrame();
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  v8::Local<v8::Object> slim_web_view_internal =
      content::GetOrCreateObject(isolate, context, chrome, kSlimWebViewPrivate);
  auto bind = [&](const char* name, auto func) {
    slim_web_view_internal
        ->Set(context, gin::StringToSymbol(isolate, name),
              gin::CreateFunctionTemplate(isolate, base::BindRepeating(func))
                  ->GetFunction(context)
                  .ToLocalChecked())
        .Check();
  };
  blink::WebCustomElement::AddEmbedderCustomElementName(
      blink::WebString("webview"));

  bind("getNextId", &GetNextId);
  bind("registerView", &RegisterView);
  bind("getViewFromId", &GetViewFromId);
  bind("attachIframeGuest", &AttachIframeGuest);
  bind("allowGuestViewElementDefinition", &AllowGuestViewElementDefinition);
  bind("destroyContainer", &DestroyContainer);
}

}  // namespace guest_view
