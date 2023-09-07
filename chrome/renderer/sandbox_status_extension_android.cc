// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/sandbox_status_extension_android.h"

#include <utility>

#include "base/android/build_info.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/common/url_constants.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/seccomp_sandbox_status_android.h"
#include "content/public/renderer/v8_value_converter.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

SandboxStatusExtension::SandboxStatusExtension(content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {
  // Don't do anything else for subframes.
  if (!frame->IsMainFrame())
    return;
  frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<chrome::mojom::SandboxStatusExtension>(base::BindRepeating(
          &SandboxStatusExtension::OnSandboxStatusExtensionRequest,
          base::RetainedRef(this)));
}

SandboxStatusExtension::~SandboxStatusExtension() {}

// static
void SandboxStatusExtension::Create(content::RenderFrame* frame) {
  auto* extension = new SandboxStatusExtension(frame);
  extension->AddRef();  // Balanced in OnDestruct().
}

void SandboxStatusExtension::OnDestruct() {
  // This object is ref-counted, since a callback could still be in-flight.
  Release();
}

void SandboxStatusExtension::DidClearWindowObject() {
  Install();
}

void SandboxStatusExtension::AddSandboxStatusExtension() {
  should_install_ = true;
}

void SandboxStatusExtension::OnSandboxStatusExtensionRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::SandboxStatusExtension>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void SandboxStatusExtension::Install() {
  if (!should_install_)
    return;

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  v8::Local<v8::Function> function;
  bool success =
      gin::CreateFunctionTemplate(
          isolate,
          base::BindRepeating(&SandboxStatusExtension::GetSandboxStatus, this))
          ->GetFunction(context)
          .ToLocal(&function);
  if (success) {
    success = chrome
                  ->Set(context,
                        gin::StringToSymbol(isolate, "getAndroidSandboxStatus"),
                        function)
                  .IsJust();
  }
  DCHECK(success);
}

void SandboxStatusExtension::GetSandboxStatus(gin::Arguments* args) {
  if (!render_frame())
    return;

  if (render_frame()->GetWebFrame()->GetSecurityOrigin().Host() !=
      chrome::kChromeUISandboxHost) {
    args->ThrowTypeError("Not allowed on this origin");
    return;
  }

  v8::HandleScope handle_scope(args->isolate());

  v8::Local<v8::Function> callback;
  if (!args->GetNext(&callback)) {
    args->ThrowError();
    return;
  }

  auto global_callback =
      std::make_unique<v8::Global<v8::Function>>(args->isolate(), callback);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SandboxStatusExtension::ReadSandboxStatus, this),
      base::BindOnce(&SandboxStatusExtension::RunCallback, this,
                     std::move(global_callback)));
}

base::Value::Dict SandboxStatusExtension::ReadSandboxStatus() {
  std::string secontext;
  base::FilePath path(FILE_PATH_LITERAL("/proc/self/attr/current"));
  base::ReadFileToString(path, &secontext);

  std::string proc_status;
  path = base::FilePath(FILE_PATH_LITERAL("/proc/self/status"));
  base::ReadFileToString(path, &proc_status);

  base::Value::Dict status;
  status.Set("uid", static_cast<int>(getuid()));
  status.Set("pid", getpid());
  status.Set("secontext", secontext);
  status.Set("seccompStatus",
             static_cast<int>(content::GetSeccompSandboxStatus()));
  status.Set("procStatus", proc_status);
  status.Set("androidBuildId",
             base::android::BuildInfo::GetInstance()->android_build_id());
  return status;
}

void SandboxStatusExtension::RunCallback(
    std::unique_ptr<v8::Global<v8::Function>> callback,
    base::Value::Dict status) {
  if (!render_frame())
    return;

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> callback_local =
      v8::Local<v8::Function>::New(isolate, *callback);

  v8::Local<v8::Value> argv[] = {
      content::V8ValueConverter::Create()->ToV8Value(status, context)};
  web_frame->CallFunctionEvenIfScriptDisabled(
      callback_local, v8::Object::New(isolate), 1, argv);
}
