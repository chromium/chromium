// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indigo/onboarding_agent.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "gin/try_catch.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8.h"

namespace indigo {

// Context object exposed to JavaScript as `window.chromeOnboarding`.
class OnboardingContext final : public gin::Wrappable<OnboardingContext> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  explicit OnboardingContext(base::WeakPtr<OnboardingAgent> agent)
      : agent_(agent) {}

  gin::WrapperInfo* wrapper_info() const final { return &kWrapperInfo; }

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final {
    return Wrappable<OnboardingContext>::GetObjectTemplateBuilder(isolate)
        .SetMethod("acknowledgeChromeDisclaimer",
                   &OnboardingContext::AcknowledgeChromeDisclaimer);
  }

 private:
  void AcknowledgeChromeDisclaimer() {
    if (agent_) {
      agent_->AcknowledgeChromeDisclaimer();
    }
  }

  base::WeakPtr<OnboardingAgent> agent_;
};

gin::WrapperInfo OnboardingContext::kWrapperInfo = {{gin::kEmbedderNativeGin},
                                                    gin::kIndigoOnboarding};

// static
void OnboardingAgent::MaybeCreate(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry) {
  if (render_frame->IsMainFrame() && !render_frame->IsInFencedFrameTree() &&
      render_frame->GetBlinkPreferences().is_indigo_onboarding) {
    new OnboardingAgent(render_frame, registry);
  }
}

OnboardingAgent::OnboardingAgent(content::RenderFrame* render_frame,
                                 blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&host_);
}

OnboardingAgent::~OnboardingAgent() = default;

void OnboardingAgent::OnDestruct() {
  delete this;
}

void OnboardingAgent::DidCreateScriptContext(v8::Local<v8::Context> context,
                                             int world_id) {
  if (world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }
  v8::Isolate* isolate =
      render_frame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);
  OnboardingContext* onboarding_context =
      cppgc::MakeGarbageCollected<OnboardingContext>(
          isolate->GetCppHeap()->GetAllocationHandle(),
          weak_factory_.GetWeakPtr());

  v8::Local<v8::Value> onboarding_context_v8;
  if (!gin::ConvertToV8(isolate, onboarding_context)
           .ToLocal(&onboarding_context_v8)) {
    LOG(WARNING) << "Failed to build OnboardingContext v8 wrapper.";
    return;
  }

  bool created_property = false;
  if (!context->Global()
           ->CreateDataProperty(
               context, gin::StringToSymbol(isolate, "chromeOnboarding"),
               onboarding_context_v8)
           .To(&created_property) ||
      !created_property) {
    LOG(WARNING) << "Failed to create window.chromeOnboarding property.";
    return;
  }
}

void OnboardingAgent::AcknowledgeChromeDisclaimer() {
  host_->AcknowledgeChromeDisclaimer();
}

}  // namespace indigo
