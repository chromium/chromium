// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/sandbox_status_extension_android.h"

#include <utility>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "chrome/common/url_constants.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/seccomp_sandbox_status_android.h"
#include "content/public/renderer/v8_value_converter.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

SandboxStatusExtension::SandboxStatusExtension(content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {
  // Don't do anything else for subframes.
  if (!frame->IsMainFrame())
    return;
  frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::Bind(&SandboxStatusExtension::OnSandboxStatusExtensionRequest,
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

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  v8::Local<v8::Function> function;
  bool success =
      gin::CreateFunctionTemplate(
          isolate, base::Bind(&SandboxStatusExtension::GetSandboxStatus, this))
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

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::Bind(&SandboxStatusExtension::ReadSandboxStatus, this),
      base::Bind(&SandboxStatusExtension::RunCallback, this,
                 base::Passed(&global_callback)));
}

std::unique_ptr<base::Value> SandboxStatusExtension::ReadSandboxStatus() {
  std::string secontext;
  base::FilePath path(FILE_PATH_LITERAL("/proc/self/attr/current"));
  base::ReadFileToString(path, &secontext);

  std::string proc_status;
  path = base::FilePath(FILE_PATH_LITERAL("/proc/self/status"));
  base::ReadFileToString(path, &proc_status);

  auto status = std::make_unique<base::DictionaryValue>();
  status->SetInteger("uid", getuid());
  status->SetInteger("pid", getpid());
  status->SetString("secontext", secontext);
  status->SetInteger("seccompStatus",
                     static_cast<int>(content::GetSeccompSandboxStatus()));
  status->SetString("procStatus", proc_status);
  status->SetString(
      "androidBuildId",
      base::android::BuildInfo::GetInstance()->android_build_id());

  return std::move(status);
}

void SandboxStatusExtension::RunCallback(
    std::unique_ptr<v8::Global<v8::Function>> callback,
    std::unique_ptr<base::Value> status) {
  if (!render_frame())
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> callback_local =
      v8::Local<v8::Function>::New(isolate, *callback);

  v8::Local<v8::Value> argv[] = {
      content::V8ValueConverter::Create()->ToV8Value(status.get(), context)};
  render_frame()->GetWebFrame()->CallFunctionEvenIfScriptDisabled(
      callback_local, v8::Object::New(isolate), 1, argv);
}
