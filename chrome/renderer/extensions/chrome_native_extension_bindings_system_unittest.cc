// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_builder.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

TEST_F(NativeExtensionBindingsSystemUnittest, InitializeContext) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .AddPermissions({"idle", "power", "webRequest", "tabs"})
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // Test that chrome-specified APIs (like tabs) are present.
  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> tabs =
      GetPropertyFromObject(chrome.As<v8::Object>(), context, "tabs");
  ASSERT_FALSE(tabs.IsEmpty());
  ASSERT_TRUE(tabs->IsObject());

  v8::Local<v8::Value> query =
      GetPropertyFromObject(tabs.As<v8::Object>(), context, "query");
  ASSERT_FALSE(query.IsEmpty());
  ASSERT_TRUE(query->IsFunction());
}

}  // namespace extensions
