// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/supervised_user/supervised_user_error_page_controller.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/supervised_user/supervised_user_error_page_controller_delegate.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/renderer/render_frame.h"
#include "gin/object_template_builder.h"
#include "gin/persistent.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/cppgc/visitor.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-cppgc.h"
#include "v8/include/v8-microtask-queue.h"

void SupervisedUserErrorPageController::Install(
    content::RenderFrame* render_frame,
    base::WeakPtr<SupervisedUserErrorPageControllerDelegate> delegate) {
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  auto* controller =
      cppgc::MakeGarbageCollected<SupervisedUserErrorPageController>(
          isolate->GetCppHeap()->GetAllocationHandle(), delegate, render_frame);
  v8::Local<v8::Object> wrapper;
  if (!controller->GetWrapper(isolate).ToLocal(&wrapper)) {
    return;
  }

  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context,
            gin::StringToV8(isolate, "supervisedUserErrorPageController"),
            wrapper)
      .Check();
}

SupervisedUserErrorPageController::SupervisedUserErrorPageController(
    base::WeakPtr<SupervisedUserErrorPageControllerDelegate> delegate,
    content::RenderFrame* render_frame)
    : delegate_(delegate), render_frame_(render_frame) {}

SupervisedUserErrorPageController::~SupervisedUserErrorPageController() =
    default;

void SupervisedUserErrorPageController::Trace(cppgc::Visitor* visitor) const {
  gin::Wrappable<SupervisedUserErrorPageController>::Trace(visitor);
  visitor->Trace(weak_factory_);
}

void SupervisedUserErrorPageController::GoBack() {
  if (delegate_)
    delegate_->GoBack();
}

void SupervisedUserErrorPageController::RequestUrlAccessRemote() {
  if (delegate_) {
    delegate_->RequestUrlAccessRemote(base::BindOnce(
        &SupervisedUserErrorPageController::OnRequestUrlAccessRemote,
        gin::WrapPersistent(weak_factory_.GetWeakCell(
            v8::Isolate::GetCurrent()->GetCppHeap()->GetAllocationHandle()))));
  }
}

void SupervisedUserErrorPageController::RequestUrlAccessLocal() {
  if (delegate_) {
    delegate_->RequestUrlAccessLocal(base::BindOnce([](bool success) {
      // We might want to handle the error in starting local approval flow
      // later. For now just log a result.
      VLOG(0) << "Local URL approval initiation result: " << success;
    }));
  }
}

#if BUILDFLAG(IS_ANDROID)
void SupervisedUserErrorPageController::LearnMore() {
  if (delegate_) {
    delegate_->LearnMore(base::BindOnce(
        &SupervisedUserErrorPageController::OnLearnMore,
        gin::WrapPersistent(weak_factory_.GetWeakCell(
            v8::Isolate::GetCurrent()->GetCppHeap()->GetAllocationHandle()))));
  }
}

void SupervisedUserErrorPageController::OnLearnMore() {
  // Navigate to the learn more resource from the error page in the same tab,
  // while also allowing the user to go back.
  std::string js =
      base::StrCat({"window.location.href = '",
                    supervised_user::kDeviceFiltersHelpCenterUrl, "';"});
  render_frame_->ExecuteJavaScript(base::ASCIIToUTF16(js));
}
#endif  // BUILDFLAG(IS_ANDROID)

void SupervisedUserErrorPageController::OnRequestUrlAccessRemote(bool success) {
  std::string result = base::ToString(success);
  std::string is_outermost_main_frame =
      base::ToString(render_frame_->GetWebFrame()->IsOutermostMainFrame());
  std::string js =
      base::StringPrintf("setRequestStatus(%s, %s)", result.c_str(),
                         is_outermost_main_frame.c_str());
  render_frame_->ExecuteJavaScript(base::ASCIIToUTF16(js));
}

gin::ObjectTemplateBuilder
SupervisedUserErrorPageController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<
      SupervisedUserErrorPageController>::GetObjectTemplateBuilder(isolate)
          .SetMethod("goBack", &SupervisedUserErrorPageController::GoBack)
          .SetMethod("requestUrlAccessRemote",
                     &SupervisedUserErrorPageController::RequestUrlAccessRemote)
          .SetMethod("requestUrlAccessLocal",
                     &SupervisedUserErrorPageController::RequestUrlAccessLocal)
#if BUILDFLAG(IS_ANDROID)
          .SetMethod("learnMore", &SupervisedUserErrorPageController::LearnMore)
#endif  // BUILDFLAG(IS_ANDROID)
      ;
}

const gin::WrapperInfo* SupervisedUserErrorPageController::wrapper_info() const {
  return &kWrapperInfo;
}
