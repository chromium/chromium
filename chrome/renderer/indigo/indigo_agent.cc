// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indigo/indigo_agent.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "gin/persistent.h"
#include "gin/try_catch.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_image_replacement.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8.h"

namespace indigo {

class IndigoContext final : public gin::Wrappable<IndigoContext> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  explicit IndigoContext(base::WeakPtr<IndigoAgent> indigo_agent)
      : indigo_agent_(indigo_agent) {}

  gin::WrapperInfo* wrapper_info() const final { return &kWrapperInfo; }

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final {
    return Wrappable<IndigoContext>::GetObjectTemplateBuilder(isolate)
        .SetMethod("setup", &IndigoContext::Setup)
        .SetMethod("startImageReplacement",
                   &IndigoContext::StartImageReplacement);
  }

  void Trace(cppgc::Visitor* visitor) const final {
    visitor->Trace(context_);
    visitor->Trace(content_agent_);
  }

  void CallInvokeCallback(v8::Isolate* isolate) {
    if (context_.IsEmpty()) {
      LOG(WARNING) << "Indigo context is empty.";
      return;
    }
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = context_.Get(isolate);
    v8::Context::Scope context_scope(context);
    v8::MicrotasksScope microtasks_scope(context,
                                         v8::MicrotasksScope::kRunMicrotasks);
    gin::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);
    v8::Local<v8::Value> content_agent = content_agent_.Get(isolate);
    if (content_agent.IsEmpty() || !content_agent->IsObject()) {
      LOG(WARNING) << "Indigo content agent is empty or not an object.";
      return;
    }
    gin::Dictionary content_agent_dict(isolate, content_agent.As<v8::Object>());
    v8::Local<v8::Function> invoke_function;
    if (!content_agent_dict.Get("invoke", &invoke_function)) {
      LOG(WARNING) << "Indigo content agent does not have an invoke function.";
      return;
    }
    std::ignore =
        invoke_function->Call(isolate, context, content_agent, 0, nullptr);
  }

 private:
  void Setup(gin::Arguments* args, v8::Local<v8::Value> content_agent) {
    context_.Reset(args->isolate(), args->GetHolderCreationContext());
    content_agent_.Reset(args->isolate(), content_agent);
  }

  // startImageReplacement(img: HTMLImageElement,
  //                       params?: {
  //                         disposition?: 'primary' | 'mirror',
  //                       }): void
  // Note: If params.disposition is unspecified, it defaults to 'primary'.
  void StartImageReplacement(v8::Isolate* isolate,
                             v8::Local<v8::Value> element_wrapper,
                             gin::Arguments* args) {
    blink::WebElement element =
        blink::WebElement::FromV8Value(isolate, element_wrapper);
    if (element.IsNull()) {
      isolate->ThrowException(v8::Exception::TypeError(
          gin::StringToV8(isolate, "Invalid element wrapper.")));
      return;
    }

    bool is_primary = true;
    v8::Local<v8::Value> params_value;
    if (args->GetNext(&params_value) && !params_value->IsUndefined()) {
      if (!params_value->IsObject()) {
        isolate->ThrowException(v8::Exception::TypeError(
            gin::StringToV8(isolate, "Invalid params object.")));
        return;
      }
      v8::Local<v8::Context> context = isolate->GetCurrentContext();
      v8::Local<v8::Object> params_obj = params_value.As<v8::Object>();
      v8::Local<v8::Value> disp_val;
      if (params_obj->Get(context, gin::StringToV8(isolate, "disposition"))
              .ToLocal(&disp_val) &&
          !disp_val->IsUndefined()) {
        std::string disposition;
        gin::Converter<std::string>::FromV8(isolate, disp_val, &disposition);
        if (disposition == "primary") {
          is_primary = true;
        } else if (disposition == "mirror") {
          is_primary = false;
        } else {
          isolate->ThrowException(v8::Exception::Error(gin::StringToV8(
              isolate, disposition.empty()
                           ? "Invalid disposition value."
                           : base::StrCat({"Invalid disposition value \"",
                                           disposition, "\"."}))));
          return;
        }
      }
    }

    auto result = blink::WebImageReplacement::CreateAndBindReceiver(element);
    if (!result.has_value()) {
      isolate->ThrowException(v8::Exception::Error(
          gin::StringToV8(isolate, blink::WebString(result.error()).Utf8())));
      return;
    }

    if (indigo_agent_) {
      indigo_agent_->GetHost().StartImageReplacement(
          std::move(result.value()), is_primary, base::DoNothing());
    }
  }

  const base::WeakPtr<IndigoAgent> indigo_agent_;
  v8::TracedReference<v8::Context> context_;
  v8::TracedReference<v8::Value> content_agent_;
};

// static
gin::WrapperInfo IndigoContext::kWrapperInfo = {{gin::kEmbedderNativeGin},
                                                gin::kIndigoContext};

// static
void IndigoAgent::MaybeCreate(content::RenderFrame* render_frame,
                              blink::AssociatedInterfaceRegistry* registry) {
  if (render_frame->IsMainFrame() &&
      base::FeatureList::IsEnabled(features::kIndigo)) {
    new IndigoAgent(render_frame, registry);
  }
}

IndigoAgent::IndigoAgent(content::RenderFrame* render_frame,
                         blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  registry->AddInterface<chrome::mojom::IndigoAgent>(
      base::BindRepeating(&IndigoAgent::BindReceiver, base::Unretained(this)));
}

IndigoAgent::~IndigoAgent() = default;

void IndigoAgent::InjectScript(
    const std::string& script_content,
    const GURL& script_url,
    const url::Origin& origin,
    mojo::PendingAssociatedRemote<chrome::mojom::IndigoAgentHost> host,
    base::OnceClosure done) {
  host_.Bind(std::move(host));

  // Associate the isolated world with the provided origin.
  blink::WebIsolatedWorldInfo info;
  info.human_readable_name = "Indigo";
  info.security_origin = blink::WebSecurityOrigin(origin);
  blink::SetIsolatedWorldInfo(ISOLATED_WORLD_ID_INDIGO, info);

  blink::WebScriptSource source(blink::WebString::FromUtf8(script_content),
                                script_url);
  render_frame()->GetWebFrame()->ExecuteScriptInIsolatedWorld(
      ISOLATED_WORLD_ID_INDIGO, source, blink::BackForwardCacheAware::kAllow);
  std::move(done).Run();
}

void IndigoAgent::Invoke(base::OnceClosure done) {
  if (indigo_context_) {
    indigo_context_->CallInvokeCallback(GetIsolate());
  }
  std::move(done).Run();
}

void IndigoAgent::OnDestruct() {
  delete this;
}

void IndigoAgent::DidCreateScriptContext(v8::Local<v8::Context> context,
                                         int world_id) {
  if (world_id != ISOLATED_WORLD_ID_INDIGO) {
    return;
  }
  v8::Isolate* isolate = GetIsolate();
  IndigoContext* indigo_context = cppgc::MakeGarbageCollected<IndigoContext>(
      isolate->GetCppHeap()->GetAllocationHandle(), weak_factory_.GetWeakPtr());
  indigo_context_ = indigo_context;

  v8::HandleScope handle_scope(isolate);
  gin::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> indigo_context_v8;
  if (!gin::ConvertToV8(isolate, indigo_context).ToLocal(&indigo_context_v8)) {
    LOG(WARNING) << "Failed to build IndigoContext v8 wrapper.";
    return;
  }
  bool created_property = false;
  if (!context->Global()
           ->CreateDataProperty(context, gin::StringToSymbol(isolate, "indigo"),
                                indigo_context_v8)
           .To(&created_property) ||
      !created_property) {
    LOG(WARNING) << "Failed to create window.indigo property.";
    return;
  }
}

void IndigoAgent::WillReleaseScriptContext(v8::Local<v8::Context> context,
                                           int world_id) {
  if (world_id == ISOLATED_WORLD_ID_INDIGO) {
    indigo_context_.Clear();
  }
}

v8::Isolate* IndigoAgent::GetIsolate() const {
  return render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
}

void IndigoAgent::BindReceiver(
    mojo::PendingAssociatedReceiver<chrome::mojom::IndigoAgent>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

}  // namespace indigo
