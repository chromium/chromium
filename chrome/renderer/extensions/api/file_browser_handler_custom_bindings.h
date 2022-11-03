// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_API_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {

// Custom bindings for the fileBrowserHandler API.
class FileBrowserHandlerCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit FileBrowserHandlerCustomBindings(ScriptContext* context);

  FileBrowserHandlerCustomBindings(const FileBrowserHandlerCustomBindings&) =
      delete;
  FileBrowserHandlerCustomBindings& operator=(
      const FileBrowserHandlerCustomBindings&) = delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  // Public static implementation of GetExternalFileEntry() for use by
  // FileManagerPrivate native handler.
  static void GetExternalFileEntry(
      const v8::FunctionCallbackInfo<v8::Value>& args,
      ScriptContext* context);

 private:
  void GetExternalFileEntryCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_H_
