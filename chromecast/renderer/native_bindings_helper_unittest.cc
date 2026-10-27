// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/native_bindings_helper.h"

#include <string_view>

#include "gin/converter.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace chromecast {
namespace {

void SetFlag(bool* flag) {
  *flag = true;
}

class NativeBindingsHelperTest : public gin::V8Test {
 protected:
  // Compiles and runs |source| in the test context, returning the completion
  // value.
  v8::Local<v8::Value> RunScript(v8::Local<v8::Context> context,
                                 std::string_view source) {
    v8::Isolate* isolate = instance_->isolate();
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, gin::StringToV8(isolate, source))
            .ToLocalChecked();
    return script->Run(context).ToLocalChecked();
  }

  bool RunScriptToBool(v8::Local<v8::Context> context,
                       std::string_view source) {
    return RunScript(context, source)->BooleanValue(instance_->isolate());
  }
};

TEST_F(NativeBindingsHelperTest, CreatesPlatformObjectHierarchy) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_TRUE(RunScriptToBool(context, "typeof cast === 'object'"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));

  // Repeated calls return the same object.
  EXPECT_TRUE(
      GetOrCreateCastPlatformObject(isolate, global)->StrictEquals(platform));
}

TEST_F(NativeBindingsHelperTest, ReusesExistingPlainDataProperties) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context, "globalThis.cast = {__platform__: {marker: 17}};");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));
  EXPECT_TRUE(RunScriptToBool(context, "cast.__platform__.marker === 17"));
}

TEST_F(NativeBindingsHelperTest, OverwritesNonObjectProperties) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context, "globalThis.cast = 42;");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_TRUE(RunScriptToBool(context, "typeof cast === 'object'"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));
}

TEST_F(NativeBindingsHelperTest, OverwritesNonWritableConfigurableProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context,
            "Object.defineProperty(globalThis, 'cast', {"
            "  value: 42,"
            "  writable: false,"
            "  configurable: true,"
            "});");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_TRUE(RunScriptToBool(context, "typeof cast === 'object'"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));
}

TEST_F(NativeBindingsHelperTest, OverwritesFrozenObjects) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context,
            "globalThis.frozenCast = Object.freeze({marker: 1});"
            "globalThis.cast = globalThis.frozenCast;");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_TRUE(RunScriptToBool(context, "cast !== frozenCast"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));

  // Also verify a frozen cast.__platform__ is replaced with an extensible
  // object on which bindings can be installed.
  RunScript(context,
            "globalThis.frozenPlatform = Object.freeze({marker: 2});"
            "globalThis.cast = {__platform__: globalThis.frozenPlatform};");

  v8::Local<v8::Object> platform2 =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform2.IsEmpty());
  EXPECT_TRUE(
      RunScriptToBool(context, "cast.__platform__ !== frozenPlatform"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform2));

  bool called = false;
  InstallBinding(isolate, platform2, "doThing", &SetFlag, &called);
  RunScript(context, "cast.__platform__.doThing();");
  EXPECT_TRUE(called);
}

// Reading or (re)defining the cast object must not run author script, which
// could detach the frame and delete the bindings while they are being
// installed.
TEST_F(NativeBindingsHelperTest, DoesNotInvokeAccessorsOnGlobal) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context,
            "globalThis.accessed = false;"
            "Object.defineProperty(globalThis, 'cast', {"
            "  configurable: true,"
            "  get() { globalThis.accessed = true; return {}; },"
            "  set() { globalThis.accessed = true; },"
            "});");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
  // The accessor has been replaced by a plain data property.
  EXPECT_TRUE(RunScriptToBool(
      context,
      "Object.getOwnPropertyDescriptor(globalThis, 'cast').get === "
      "undefined"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));
  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
}

TEST_F(NativeBindingsHelperTest, DoesNotInvokeAccessorsOnCastObject) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context,
            "globalThis.accessed = false;"
            "globalThis.cast = {};"
            "Object.defineProperty(globalThis.cast, '__platform__', {"
            "  configurable: true,"
            "  get() { globalThis.accessed = true; return {}; },"
            "  set() { globalThis.accessed = true; },"
            "});");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));
  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
}

TEST_F(NativeBindingsHelperTest, DoesNotInvokeProxyTraps) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context,
            "globalThis.trapped = false;"
            "globalThis.proxyRef = new Proxy({}, {"
            "  get() { globalThis.trapped = true; return {}; },"
            "  set() { globalThis.trapped = true; return true; },"
            "  has() { globalThis.trapped = true; return false; },"
            "  getOwnPropertyDescriptor() {"
            "    globalThis.trapped = true;"
            "    return undefined;"
            "  },"
            "  defineProperty() { globalThis.trapped = true; return true; },"
            "});"
            "globalThis.cast = globalThis.proxyRef;");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_FALSE(RunScriptToBool(context, "trapped"));
  // The proxy has been replaced by a plain object.
  EXPECT_TRUE(RunScriptToBool(context, "cast !== proxyRef"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));
  EXPECT_FALSE(RunScriptToBool(context, "trapped"));
}

TEST_F(NativeBindingsHelperTest, DoesNotInvokeAccessorsOnPrototypeChain) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context,
            "globalThis.accessed = false;"
            "Object.defineProperty(Object.prototype, 'cast', {"
            "  configurable: true,"
            "  get() { globalThis.accessed = true; return {}; },"
            "  set() { globalThis.accessed = true; },"
            "});");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
  // An own data property shadows the prototype accessor.
  EXPECT_TRUE(
      RunScriptToBool(context,
                      "Object.getOwnPropertyDescriptor(globalThis, 'cast') !== "
                      "undefined"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));
  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
}

TEST_F(NativeBindingsHelperTest, IgnoresPollutedDescriptorPrototype) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context,
            "globalThis.accessed = false;"
            "Object.defineProperty(globalThis, 'cast', {"
            "  configurable: true,"
            "  get() { globalThis.accessed = true; return {}; },"
            "});"
            "Object.defineProperty(Object.prototype, 'value', {"
            "  configurable: true,"
            "  get() { globalThis.accessed = true; return {}; },"
            "});");

  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
  EXPECT_TRUE(RunScript(context, "cast.__platform__")->StrictEquals(platform));
  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
}

TEST_F(NativeBindingsHelperTest, HandlesNonConfigurableAccessor) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Local<v8::Object> global = context->Global();

  RunScript(context,
            "globalThis.accessed = false;"
            "Object.defineProperty(globalThis, 'cast', {"
            "  configurable: false,"
            "  get() { globalThis.accessed = true; return {}; },"
            "});");

  // The property cannot be redefined, but installation must neither run the
  // accessor nor crash.
  v8::Local<v8::Object> platform =
      GetOrCreateCastPlatformObject(isolate, global);
  ASSERT_FALSE(platform.IsEmpty());

  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
}

TEST_F(NativeBindingsHelperTest, InstallBindingDoesNotInvokeAccessors) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  RunScript(context,
            "globalThis.accessed = false;"
            "globalThis.target = {};"
            "Object.defineProperty(globalThis.target, 'doThing', {"
            "  configurable: true,"
            "  get() { globalThis.accessed = true; },"
            "  set() { globalThis.accessed = true; },"
            "});");
  v8::Local<v8::Object> target = RunScript(context, "target").As<v8::Object>();

  bool called = false;
  InstallBinding(isolate, target, "doThing", &SetFlag, &called);

  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
  EXPECT_TRUE(RunScriptToBool(context,
                              "typeof target.doThing === "
                              "'function'"));
  EXPECT_FALSE(RunScriptToBool(context, "accessed"));
}

TEST_F(NativeBindingsHelperTest, InstallBindingInstallsCallableFunction) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  RunScript(context, "globalThis.target = {};");
  v8::Local<v8::Object> target = RunScript(context, "target").As<v8::Object>();

  bool called = false;
  InstallBinding(isolate, target, "doThing", &SetFlag, &called);
  // Bindings are installed repeatedly on the same object; the second install
  // replaces the first.
  InstallBinding(isolate, target, "doThing", &SetFlag, &called);

  EXPECT_TRUE(RunScriptToBool(context,
                              "typeof target.doThing === "
                              "'function'"));
  RunScript(context, "target.doThing();");
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace chromecast
