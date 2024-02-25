// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/file_browser_handler_custom_bindings.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_dom_file_system.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

FileBrowserHandlerCustomBindings::FileBrowserHandlerCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void FileBrowserHandlerCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetExternalFileEntry", "fileBrowserHandler",
      base::BindRepeating(
          &FileBrowserHandlerCustomBindings::GetExternalFileEntryCallback,
          base::Unretained(this)));
}

void FileBrowserHandlerCustomBindings::GetExternalFileEntry(
    const v8::FunctionCallbackInfo<v8::Value>& args,
    ScriptContext* context) {
  // TODO(zelidrag): Make this magic work on other platforms when file browser
  // matures enough on ChromeOS.
  // Lacros supports fileBrowserHandler, but does not use this code path.
  // Therefore this code remains Ash-only.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsObject());
  v8::Local<v8::Object> file_def = args[0].As<v8::Object>();
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> v8_context = context->v8_context();

  std::string file_system_name(*v8::String::Utf8Value(
      isolate, file_def
                   ->Get(v8_context, v8::String::NewFromUtf8(
                                         isolate, "fileSystemName",
                                         v8::NewStringType::kInternalized)
                                         .ToLocalChecked())
                   .ToLocalChecked()));
  GURL file_system_root(*v8::String::Utf8Value(
      isolate, file_def
                   ->Get(v8_context, v8::String::NewFromUtf8(
                                         isolate, "fileSystemRoot",
                                         v8::NewStringType::kInternalized)
                                         .ToLocalChecked())
                   .ToLocalChecked()));
  std::string file_full_path(*v8::String::Utf8Value(
      isolate, file_def
                   ->Get(v8_context, v8::String::NewFromUtf8(
                                         isolate, "fileFullPath",
                                         v8::NewStringType::kInternalized)
                                         .ToLocalChecked())
                   .ToLocalChecked()));
  bool is_directory =
      file_def
          ->Get(v8_context,
                v8::String::NewFromUtf8(isolate, "fileIsDirectory",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          .ToLocalChecked()
          ->BooleanValue(isolate);
  blink::WebDOMFileSystem::EntryType entry_type =
      is_directory ? blink::WebDOMFileSystem::kEntryTypeDirectory
                   : blink::WebDOMFileSystem::kEntryTypeFile;
  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::FrameForContext(v8_context);
  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::Create(
          webframe, blink::kWebFileSystemTypeExternal,
          blink::WebString::FromUTF8(file_system_name), file_system_root)
          .CreateV8Entry(blink::WebString::FromUTF8(file_full_path), entry_type,
                         isolate));
#endif
}

void FileBrowserHandlerCustomBindings::GetExternalFileEntryCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  GetExternalFileEntry(args, context());
}

}  // namespace extensions
