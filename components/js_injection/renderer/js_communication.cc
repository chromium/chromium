// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/renderer/js_communication.h"

#include "components/js_injection/common/origin_matcher.h"
#include "components/js_injection/renderer/js_binding.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8.h"

namespace js_injection {
namespace {

// If enabled will bind browser->js pipes lazily instead of when the window
// object is cleared.
BASE_FEATURE(kLazyBindJsInjection,
             "LazyBindJsInjection",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

class JsCommunication::JsObjectInfo
    : public mojom::BrowserToJsMessagingFactory {
 public:
  explicit JsObjectInfo(mojom::JsObjectPtr js_object)
      : origin_matcher_(js_object->origin_matcher),
        js_to_java_messaging_(std::move(js_object->js_to_browser_messaging)),
        factory_receiver_(this, std::move(js_object->browser_to_js_factory)) {}

  // mojom::BrowserToJsMessagingFactory:
  void SendBrowserToJsMessaging(
      mojo::PendingAssociatedReceiver<mojom::BrowserToJsMessaging>
          browser_to_js_messaging) override {
    if (!js_binding_) {
      return;
    }

    js_binding_->Bind(std::move(browser_to_js_messaging));
  }

  void SetBinding(base::WeakPtr<JsBinding> js_binding) {
    js_binding_ = std::move(js_binding);
  }

  const OriginMatcher& origin_matcher() const { return origin_matcher_; }

  mojom::JsToBrowserMessaging* js_to_java_messaging() const {
    return js_to_java_messaging_.get();
  }

 private:
  OriginMatcher origin_matcher_;
  mojo::AssociatedRemote<mojom::JsToBrowserMessaging> js_to_java_messaging_;
  mojo::AssociatedReceiver<mojom::BrowserToJsMessagingFactory>
      factory_receiver_;
  base::WeakPtr<JsBinding> js_binding_;
};

struct JsCommunication::DocumentStartJavaScript {
  OriginMatcher origin_matcher;
  blink::WebString script;
  int32_t script_id;
};

JsCommunication::JsCommunication(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<JsCommunication>(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::JsCommunication>(base::BindRepeating(
          &JsCommunication::BindPendingReceiver, base::Unretained(this)));
}

JsCommunication::~JsCommunication() = default;

void JsCommunication::SetJsObjects(
    std::vector<mojom::JsObjectPtr> js_object_ptrs,
    mojo::PendingAssociatedRemote<mojom::JsObjectsClient> client) {
  JsObjectMap js_objects;
  for (auto& js_object : js_object_ptrs) {
    std::u16string name = js_object->js_object_name;
    js_objects.insert(
        {name, std::make_unique<JsObjectInfo>(std::move(js_object))});
  }
  js_objects_.swap(js_objects);
  client_remote_.reset();
  client_remote_.Bind(std::move(client));
}

void JsCommunication::AddDocumentStartScript(
    mojom::DocumentStartJavaScriptPtr script_ptr) {
  DocumentStartJavaScript* script = new DocumentStartJavaScript{
      script_ptr->origin_matcher,
      blink::WebString::FromUTF16(script_ptr->script), script_ptr->script_id};
  scripts_.push_back(std::unique_ptr<DocumentStartJavaScript>(script));
}

void JsCommunication::RemoveDocumentStartScript(int32_t script_id) {
  for (auto it = scripts_.begin(); it != scripts_.end(); ++it) {
    if ((*it)->script_id == script_id) {
      scripts_.erase(it);
      break;
    }
  }
}

void JsCommunication::DidClearWindowObject() {
  if (inside_did_clear_window_object_)
    return;

  base::AutoReset<bool> flag_entry(&inside_did_clear_window_object_, true);

  // Invalidate `weak_ptr_factory_for_bindings_` so that existing bindings
  // can not send messages back to the browser (JsBinding is owned by v8,
  // so we can't delete it here).
  weak_ptr_factory_for_bindings_.InvalidateWeakPtrs();

  // As an optimization, we may set up the v8 scopes here for all the JS
  // binding installations.
  v8::Isolate* isolate = nullptr;
  v8::Local<v8::Context> context;
  std::optional<v8::HandleScope> handle_scope;
  std::optional<v8::Context::Scope> context_scope;
  if (base::FeatureList::IsEnabled(kLazyBindJsInjection)) {
    blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
    isolate = web_frame->GetAgentGroupScheduler()->Isolate();
    handle_scope.emplace(isolate);
    context = web_frame->MainWorldScriptContext();
    if (context.IsEmpty()) {
      return;
    }

    context_scope.emplace(context);
  }

  url::Origin frame_origin =
      url::Origin(render_frame()->GetWebFrame()->GetSecurityOrigin());
  std::vector<base::WeakPtr<JsBinding>> js_bindings;
  js_bindings.reserve(js_objects_.size());

  for (const auto& js_object : js_objects_) {
    if (!js_object.second->origin_matcher().Matches(frame_origin)) {
      js_object.second->SetBinding(nullptr);
      continue;
    }
    base::WeakPtr<JsBinding> js_binding = JsBinding::Install(
        render_frame(), js_object.first,
        weak_ptr_factory_for_bindings_.GetWeakPtr(), isolate, context);
    if (js_binding) {
      if (base::FeatureList::IsEnabled(kLazyBindJsInjection)) {
        js_object.second->SetBinding(js_binding);
      } else {
        mojom::JsToBrowserMessaging* js_to_java_messaging =
            GetJsToJavaMessage(js_object.first);
        if (js_to_java_messaging) {
          mojo::PendingAssociatedRemote<mojom::BrowserToJsMessaging> remote;
          js_binding->Bind(remote.InitWithNewEndpointAndPassReceiver());
          js_to_java_messaging->SetBrowserToJsMessaging(std::move(remote));
        }
      }
      js_bindings.push_back(std::move(js_binding));
    }
  }
  js_bindings_.swap(js_bindings);
  if (client_remote_ && base::FeatureList::IsEnabled(kLazyBindJsInjection)) {
    client_remote_->OnWindowObjectCleared();
  }
}

void JsCommunication::WillReleaseScriptContext(v8::Local<v8::Context> context,
                                               int32_t world_id) {
  // We created v8 global objects only in the main world, should clear them only
  // when this is for main world.
  if (world_id != content::ISOLATED_WORLD_ID_GLOBAL)
    return;

  for (const auto& js_binding : js_bindings_) {
    if (js_binding)
      js_binding->ReleaseV8GlobalObjects();
  }
}

void JsCommunication::OnDestruct() {
  delete this;
}

void JsCommunication::RunScriptsAtDocumentStart() {
  url::Origin frame_origin =
      url::Origin(render_frame()->GetWebFrame()->GetSecurityOrigin());
  for (const auto& script : scripts_) {
    if (!script->origin_matcher.Matches(frame_origin))
      continue;
    render_frame()->GetWebFrame()->ExecuteScript(
        blink::WebScriptSource(script->script));
  }
}

void JsCommunication::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::JsCommunication> pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver),
                 render_frame()->GetTaskRunner(
                     blink::TaskType::kInternalNavigationAssociated));
}

mojom::JsToBrowserMessaging* JsCommunication::GetJsToJavaMessage(
    const std::u16string& js_object_name) {
  auto iterator = js_objects_.find(js_object_name);
  if (iterator == js_objects_.end())
    return nullptr;
  return iterator->second->js_to_java_messaging();
}

}  // namespace js_injection
