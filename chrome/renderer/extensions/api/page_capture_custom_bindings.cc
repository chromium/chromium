// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/page_capture_custom_bindings.h"

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_blob.h"
#include "v8/include/v8.h"

namespace extensions {

PageCaptureCustomBindings::PageCaptureCustomBindings(
    ScriptContext* context,
    IPCMessageSender* ipc_message_sender)
    : ObjectBackedNativeHandler(context),
      ipc_message_sender_(ipc_message_sender) {}

void PageCaptureCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "CreateBlob", "pageCapture",
      base::BindRepeating(&PageCaptureCustomBindings::CreateBlob,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "SendResponseAck", "pageCapture",
      base::BindRepeating(&PageCaptureCustomBindings::SendResponseAck,
                          base::Unretained(this)));
}

void PageCaptureCustomBindings::CreateBlob(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsInt32());
  v8::Isolate* isolate = args.GetIsolate();
  blink::WebString path(
      blink::WebString::FromUTF8(*v8::String::Utf8Value(isolate, args[0])));
  blink::WebBlob blob = blink::WebBlob::CreateFromFile(
      isolate, path, args[1].As<v8::Int32>()->Value());
  args.GetReturnValue().Set(blob.ToV8Value(isolate));
}

void PageCaptureCustomBindings::SendResponseAck(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsString());

  std::string request_uuid_str(
      *v8::String::Utf8Value(args.GetIsolate(), args[0]));
  base::Uuid uuid = base::Uuid::ParseLowercase(request_uuid_str);
  CHECK(uuid.is_valid());

  ipc_message_sender_->SendResponseAckIPC(context(), uuid);
}

}  // namespace extensions
