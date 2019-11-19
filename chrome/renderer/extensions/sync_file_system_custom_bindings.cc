// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/sync_file_system_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "extensions/renderer/script_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/web/web_dom_file_system.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace extensions {

SyncFileSystemCustomBindings::SyncFileSystemCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void SyncFileSystemCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetSyncFileSystemObject", "syncFileSystem",
      base::BindRepeating(
          &SyncFileSystemCustomBindings::GetSyncFileSystemObject,
          base::Unretained(this)));
}

void SyncFileSystemCustomBindings::GetSyncFileSystemObject(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2) {
    NOTREACHED();
    return;
  }
  if (!args[0]->IsString()) {
    NOTREACHED();
    return;
  }
  if (!args[1]->IsString()) {
    NOTREACHED();
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  std::string name(*v8::String::Utf8Value(isolate, args[0]));
  if (name.empty()) {
    NOTREACHED();
    return;
  }
  std::string root_url(*v8::String::Utf8Value(isolate, args[1]));
  if (root_url.empty()) {
    NOTREACHED();
    return;
  }

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::FrameForContext(context()->v8_context());
  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::Create(
          webframe, blink::kWebFileSystemTypeExternal,
          blink::WebString::FromUTF8(name), GURL(root_url))
          .ToV8Value(context()->v8_context()->Global(), isolate));
}

}  // namespace extensions
