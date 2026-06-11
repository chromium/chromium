// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/renderer/js_communication.h"

#include <algorithm>

#include "base/feature_list.h"
#include "components/js_injection/common/interfaces.mojom-shared.h"
#include "components/js_injection/renderer/js_binding.h"
#include "components/origin_matcher/origin_matcher.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/cppgc/persistent.h"
#include "v8/include/v8.h"

namespace js_injection {
namespace {

// If enabled will bind browser->js pipes lazily instead of when the window
// object is cleared.
BASE_FEATURE(kLazyBindJsInjection, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

class JsCommunication::JsObjectInfo
    : public mojom::BrowserToJsMessagingFactory {
 public:
  explicit JsObjectInfo(mojom::JsObjectPtr js_object)
      : origin_matcher_(js_object->origin_matcher),
        js_to_java_messaging_(std::move(js_object->js_to_browser_messaging)),
        factory_receiver_(this, std::move(js_object->browser_to_js_factory)),
        world_id_(js_object->js_world) {}

  // mojom::BrowserToJsMessagingFactory:
  void SendBrowserToJsMessaging(
      mojo::PendingAssociatedReceiver<mojom::BrowserToJsMessaging>
          browser_to_js_messaging) override {
    if (!js_binding_) {
      return;
    }

    js_binding_->Bind(std::move(browser_to_js_messaging));
  }

  void SetBinding(cppgc::WeakPersistent<JsBinding> js_binding) {
    js_binding_ = std::move(js_binding);
  }

  const origin_matcher::OriginMatcher& origin_matcher() const {
    return origin_matcher_;
  }

  mojom::JsToBrowserMessaging* js_to_java_messaging() const {
    return js_to_java_messaging_.get();
  }

  int32_t world_id() const { return world_id_; }

 private:
  origin_matcher::OriginMatcher origin_matcher_;
  mojo::AssociatedRemote<mojom::JsToBrowserMessaging> js_to_java_messaging_;
  mojo::AssociatedReceiver<mojom::BrowserToJsMessagingFactory>
      factory_receiver_;
  int32_t world_id_;
  cppgc::WeakPersistent<JsBinding> js_binding_;
};

struct JsCommunication::JavaScriptExecutable {
  origin_matcher::OriginMatcher origin_matcher;
  blink::WebString script;
  int32_t script_id;
  mojom::DocumentInjectionTime injection_time;
  int32_t js_world;
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
    int32_t world_id = js_object->js_world;
    std::u16string name = js_object->js_object_name;
    js_objects[world_id][name] =
        std::make_unique<JsObjectInfo>(std::move(js_object));
  }
  js_objects_.swap(js_objects);
  client_remote_.reset();
  client_remote_.Bind(std::move(client));
}

void JsCommunication::AddPersistentJavaScript(
    mojom::JavaScriptExecutablePtr script_ptr) {
  JavaScriptExecutable* script = new JavaScriptExecutable{
      script_ptr->origin_matcher,
      blink::WebString::FromUtf16(script_ptr->script), script_ptr->script_id,
      script_ptr->injection_time, script_ptr->js_world};
  scripts_.push_back(std::unique_ptr<JavaScriptExecutable>(script));
}

void JsCommunication::RemovePersistentJavaScript(int32_t script_id) {
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

  // We can set up a single isolate and handle scope as an optimization.
  v8::Isolate* isolate = nullptr;
  std::optional<v8::HandleScope> handle_scope;
  v8::Local<v8::Context> main_world_context;
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  if (base::FeatureList::IsEnabled(kLazyBindJsInjection)) {
    isolate = web_frame->GetAgentGroupScheduler()->Isolate();
    handle_scope.emplace(isolate);
    main_world_context = web_frame->MainWorldScriptContext();
    if (main_world_context.IsEmpty()) {
      // If we don't have a main world script context, we should not proceed
      // with installation at all.
      return;
    }
  }

  url::Origin frame_origin =
      url::Origin(render_frame()->GetWebFrame()->GetSecurityOrigin());
  std::vector<cppgc::WeakPersistent<JsBinding>> js_bindings;
  size_t binding_count = std::ranges::fold_left(
      js_objects_, 0, [](size_t acc, const auto& world_entries) {
        return acc + world_entries.second.size();
      });
  js_bindings.reserve(binding_count);

  for (const auto& [world_id, world_objects] : js_objects_) {
    // Set up a context and context scope for all object installations in this
    // world as an optimization.
    v8::Local<v8::Context> current_world_context;
    std::optional<v8::Context::Scope> context_scope;
    if (base::FeatureList::IsEnabled(kLazyBindJsInjection)) {
      if (world_id == content::ISOLATED_WORLD_ID_GLOBAL) {
        current_world_context = main_world_context;
      } else {
        current_world_context =
            web_frame->GetScriptContextFromWorldId(isolate, world_id);
      }
      if (current_world_context.IsEmpty()) {
        continue;
      }
      context_scope.emplace(current_world_context);
    }

    for (const auto& [js_object_name, js_object_info] : world_objects) {
      if (!js_object_info->origin_matcher().Matches(frame_origin)) {
        js_object_info->SetBinding(nullptr);
        continue;
      }
      cppgc::WeakPersistent<JsBinding> js_binding =
          JsBinding::Install(render_frame(), js_object_name,
                             weak_ptr_factory_for_bindings_.GetWeakPtr(),
                             isolate, current_world_context, world_id);
      if (js_binding) {
        if (base::FeatureList::IsEnabled(kLazyBindJsInjection)) {
          js_object_info->SetBinding(js_binding);
        } else {
          mojom::JsToBrowserMessaging* js_to_java_messaging =
              GetJsToJavaMessage(js_object_name, world_id);
          if (js_to_java_messaging) {
            mojo::PendingAssociatedRemote<mojom::BrowserToJsMessaging> remote;
            js_binding->Bind(remote.InitWithNewEndpointAndPassReceiver());
            js_to_java_messaging->SetBrowserToJsMessaging(std::move(remote));
          }
        }
        js_bindings.push_back(std::move(js_binding));
      }
    }
  }

  js_bindings_.swap(js_bindings);
  if (client_remote_ && base::FeatureList::IsEnabled(kLazyBindJsInjection)) {
    client_remote_->OnWindowObjectCleared();
  }
}

void JsCommunication::WillReleaseScriptContext(v8::Local<v8::Context> context,
                                               int32_t world_id) {
  for (const auto& js_binding : js_bindings_) {
    if (js_binding && js_binding->world_id() == world_id) {
      js_binding->ReleaseV8GlobalObjects();
    }
  }
}

void JsCommunication::OnDestruct() {
  delete this;
}

void JsCommunication::RunScripts(mojom::DocumentInjectionTime injection_time) {
  RunScriptsInternal(weak_ptr_factory_.GetWeakPtr(), injection_time);
  // Careful `this` may be destroyed.
}

// static
void JsCommunication::RunScriptsInternal(
    base::WeakPtr<JsCommunication> js_communication,
    mojom::DocumentInjectionTime injection_time) {
  CHECK(js_communication);
  url::Origin frame_origin = url::Origin(
      js_communication->render_frame()->GetWebFrame()->GetSecurityOrigin());
  for (const auto& script : js_communication->scripts_) {
    if (!script->origin_matcher.Matches(frame_origin)) {
      continue;
    }
    if (script->injection_time == injection_time) {
      if (script->js_world == content::ISOLATED_WORLD_ID_GLOBAL) {
        js_communication->render_frame()->GetWebFrame()->ExecuteScript(
            blink::WebScriptSource(script->script));
      } else {
        js_communication->render_frame()
            ->GetWebFrame()
            ->ExecuteScriptInIsolatedWorld(
                script->js_world, blink::WebScriptSource(script->script),
                blink::BackForwardCacheAware::kAllow);
      }
    }
    // Careful, executing a script may cause JsCommunication object to be
    // destroyed.
    if (!js_communication) {
      return;
    }
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
    const std::u16string& js_object_name,
    int32_t world_id) {
  auto world_iterator = js_objects_.find(world_id);
  if (world_iterator == js_objects_.end()) {
    return nullptr;
  }
  auto name_iterator = world_iterator->second.find(js_object_name);
  if (name_iterator == world_iterator->second.end()) {
    return nullptr;
  }
  return name_iterator->second->js_to_java_messaging();
}

}  // namespace js_injection
