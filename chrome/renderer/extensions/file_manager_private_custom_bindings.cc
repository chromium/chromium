// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_manager_private_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "base/check.h"
#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_dom_file_system.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

FileManagerPrivateCustomBindings::FileManagerPrivateCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void FileManagerPrivateCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetFileSystem", "fileManagerPrivate",
      base::BindRepeating(&FileManagerPrivateCustomBindings::GetFileSystem,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "GetExternalFileEntry", "fileManagerPrivate",
      base::BindRepeating(
          &FileManagerPrivateCustomBindings::GetExternalFileEntry,
          base::Unretained(this)));
  RouteHandlerFunction(
      "GetEntryURL", "fileManagerPrivate",
      base::BindRepeating(&FileManagerPrivateCustomBindings::GetEntryURL,
                          base::Unretained(this)));
}

void FileManagerPrivateCustomBindings::GetFileSystem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 2);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsString());
  v8::Isolate* isolate = args.GetIsolate();
  std::string name(*v8::String::Utf8Value(isolate, args[0]));
  std::string root_url(*v8::String::Utf8Value(isolate, args[1]));

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::FrameForContext(context()->v8_context());
  DCHECK(webframe);
  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::Create(
          webframe, blink::kWebFileSystemTypeExternal,
          blink::WebString::FromUTF8(name), GURL(root_url))
          .ToV8Value(context()->v8_context()->Global(), isolate));
}

void FileManagerPrivateCustomBindings::GetExternalFileEntry(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  FileBrowserHandlerCustomBindings::GetExternalFileEntry(args, context());
}

void FileManagerPrivateCustomBindings::GetEntryURL(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsObject());
  const blink::WebURL& url =
      blink::WebDOMFileSystem::CreateFileSystemURL(args[0]);
  args.GetReturnValue().Set(v8_helpers::ToV8StringUnsafe(
      args.GetIsolate(), url.GetString().Utf8().c_str()));
}

}  // namespace extensions
