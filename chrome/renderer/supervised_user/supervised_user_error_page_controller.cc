// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/supervised_user/supervised_user_error_page_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/supervised_user/supervised_user_error_page_controller_delegate.h"
#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-microtask-queue.h"

namespace {

bool IsOutermostMainFrame(content::RenderFrame* render_frame) {
  return render_frame->IsMainFrame() && !render_frame->IsInFencedFrameTree();
}

}  // namespace

gin::WrapperInfo SupervisedUserErrorPageController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

void SupervisedUserErrorPageController::Install(
    content::RenderFrame* render_frame,
    base::WeakPtr<SupervisedUserErrorPageControllerDelegate> delegate) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  gin::Handle<SupervisedUserErrorPageController> controller = gin::CreateHandle(
      isolate, new SupervisedUserErrorPageController(delegate, render_frame));
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context,
            gin::StringToV8(isolate, "supervisedUserErrorPageController"),
            controller.ToV8())
      .Check();
}

SupervisedUserErrorPageController::SupervisedUserErrorPageController(
    base::WeakPtr<SupervisedUserErrorPageControllerDelegate> delegate,
    content::RenderFrame* render_frame)
    : delegate_(delegate), render_frame_(render_frame) {}

SupervisedUserErrorPageController::~SupervisedUserErrorPageController() {}

void SupervisedUserErrorPageController::GoBack() {
  if (delegate_)
    delegate_->GoBack();
}

void SupervisedUserErrorPageController::RequestUrlAccessRemote() {
  if (delegate_) {
    delegate_->RequestUrlAccessRemote(base::BindOnce(
        &SupervisedUserErrorPageController::OnRequestUrlAccessRemote,
        weak_factory_.GetWeakPtr()));
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

void SupervisedUserErrorPageController::Feedback() {
  if (delegate_)
    delegate_->Feedback();
}

void SupervisedUserErrorPageController::OnRequestUrlAccessRemote(bool success) {
  std::string result = success ? "true" : "false";
  std::string is_outermost_main_frame =
      IsOutermostMainFrame(render_frame_) ? "true" : "false";
  std::string js =
      base::StringPrintf("setRequestStatus(%s, %s)", result.c_str(),
                         is_outermost_main_frame.c_str());
  render_frame_->ExecuteJavaScript(base::ASCIIToUTF16(js));
}

gin::ObjectTemplateBuilder
SupervisedUserErrorPageController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<SupervisedUserErrorPageController>::
      GetObjectTemplateBuilder(isolate)
          .SetMethod("goBack", &SupervisedUserErrorPageController::GoBack)
          .SetMethod("requestUrlAccessRemote",
                     &SupervisedUserErrorPageController::RequestUrlAccessRemote)
          .SetMethod("requestUrlAccessLocal",
                     &SupervisedUserErrorPageController::RequestUrlAccessLocal)
          .SetMethod("feedback", &SupervisedUserErrorPageController::Feedback);
}
