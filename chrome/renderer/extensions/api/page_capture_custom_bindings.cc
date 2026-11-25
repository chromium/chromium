// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/page_capture_custom_bindings.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/script_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/web/web_blob.h"
#include "v8/include/v8.h"

namespace extensions {

PageCaptureCustomBindings::PageCaptureCustomBindings(
    ScriptContext* context,
    IPCMessageSender* ipc_message_sender)
    : ObjectBackedNativeHandler(context),
      ipc_message_sender_(ipc_message_sender) {}

PageCaptureCustomBindings::~PageCaptureCustomBindings() = default;

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
      blink::WebString::FromUtf8(*v8::String::Utf8Value(isolate, args[0])));
  blink::WebBlob blob = blink::WebBlob::CreateFromFile(
      isolate, path, args[1].As<v8::Int32>()->Value());
  args.GetReturnValue().Set(blob.ToV8Value(isolate));
}

void PageCaptureCustomBindings::SendResponseAck(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 2);
  CHECK(args[0]->IsString());
  v8::Isolate* isolate = args.GetIsolate();

  std::string request_uuid_str(*v8::String::Utf8Value(isolate, args[0]));
  base::Uuid uuid = base::Uuid::ParseLowercase(request_uuid_str);
  CHECK(uuid.is_valid());

  // A frame registers the blob and sends the ack over the same message pipe,
  // so the browser always sees the registration first. Ack right away.
  if (!context()->IsForServiceWorker()) {
    ipc_message_sender_->SendResponseAckIPC(context(), uuid);
    return;
  }

  blink::WebBlob blob = blink::WebBlob::FromV8Value(isolate, args[1]);
  CHECK(!blob.IsNull());

  // The browser deletes the MHTML file once it gets the ack. A service worker
  // registers the blob over a different message pipe and asynchronously, so
  // the registry may not hold its own reference to the file yet. A message on
  // the blob's pipe is only answered once the blob has been built, so wait for
  // that before sending the ack.
  // TODO(crbug.com/40529364): Remove this use of the deprecated
  // `GetInternalUUID()` once the browser creates the blob and passes it with
  // `ExtensionFunction::SetTransferredBlobs()`.
  mojo::Remote<blink::mojom::Blob> remote(blob.CloneBlobRemote());
  blink::mojom::Blob* blob_ptr = remote.get();
  blob_ptr->GetInternalUUID(
      base::BindOnce(&PageCaptureCustomBindings::OnBlobRegistered,
                     weak_factory_.GetWeakPtr(), std::move(remote), uuid));
}

void PageCaptureCustomBindings::OnBlobRegistered(
    mojo::Remote<blink::mojom::Blob> blob,
    const base::Uuid& request_uuid,
    const std::string& blob_uuid) {
  // The worker stopped while the reply was pending, so the ack cannot be
  // sent anymore. The browser then keeps its reference to the file. See
  // crbug.com/40673405.
  if (!is_valid()) {
    return;
  }
  ipc_message_sender_->SendResponseAckIPC(context(), request_uuid);
}

}  // namespace extensions
