// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/sync_file_system_custom_bindings.h"

#include <string>

#include "base/functional/bind.h"
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
    NOTREACHED_IN_MIGRATION();
    return;
  }
  if (!args[0]->IsString()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  if (!args[1]->IsString()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  std::string name(*v8::String::Utf8Value(isolate, args[0]));
  if (name.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  std::string root_url(*v8::String::Utf8Value(isolate, args[1]));
  if (root_url.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::FrameForContext(context()->v8_context());
  args.GetReturnValue().Set(blink::WebDOMFileSystem::Create(
                                webframe, blink::kWebFileSystemTypeExternal,
                                blink::WebString::FromUTF8(name),
                                GURL(root_url))
                                .ToV8Value(isolate));
}

}  // namespace extensions
