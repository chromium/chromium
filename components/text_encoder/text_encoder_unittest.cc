// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/text_encoder/text_encoder.h"

#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"

namespace text_encoder {
namespace {

static const int kBindingListIndex = 0;

typedef std::vector<std::unique_ptr<TextEncoder>> BindingList;

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Script;
using v8::String;
using v8::V8;
using v8::Value;

void CreateTextEncoderCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  // Check this is a constructor call: JS should always request
  // `new TextEncoder()` rather than just `TextEncoder`.
  DCHECK(info.IsConstructCall());
  std::unique_ptr<TextEncoder> text_encoder;
  Local<Context> context = info.GetIsolate()->GetCurrentContext();
  gin::Handle<TextEncoder> handle = TextEncoder::Create(context, &text_encoder);
  BindingList* bindings = static_cast<BindingList*>(
      context->Global()->GetAlignedPointerFromInternalField(kBindingListIndex));
  bindings->push_back(std::move(text_encoder));
  info.GetReturnValue().Set(handle.ToV8());
}

void AddCallHandlerToTemplate(v8::Isolate* isolate,
                              v8::Local<v8::ObjectTemplate>& object_template,
                              const std::string& name,
                              v8::FunctionCallback callback) {
  v8::Local<v8::FunctionTemplate> fn_template =
      v8::FunctionTemplate::New(isolate);
  fn_template->SetCallHandler(callback);
  object_template->Set(isolate, name.c_str(), fn_template);
}

class TestTextEncoder : public testing::Test {
 public:
  TestTextEncoder() {
    gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                   gin::ArrayBufferAllocator::SharedInstance(),
                                   /*reference_table=*/nullptr, "");
    isolate_holder_ = std::make_unique<gin::IsolateHolder>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        gin::IsolateHolder::kSingleThread,
        gin::IsolateHolder::IsolateType::kUtility);
  }

  ~TestTextEncoder() override = default;

  std::string Run(std::string test) {
    Isolate::Scope isolate_scope(isolate_holder_->isolate());
    // Create a stack-allocated handle scope.
    HandleScope handle_scope(isolate_holder_->isolate());

    v8::Handle<v8::ObjectTemplate> global_template =
        v8::ObjectTemplate::New(isolate_holder_->isolate());
    global_template->SetInternalFieldCount(kBindingListIndex + 1);
    AddCallHandlerToTemplate(isolate_holder_->isolate(), global_template,
                             "TextEncoder", CreateTextEncoderCallback);
    // Create a new context.
    Local<Context> context = Context::New(
        isolate_holder_->isolate(), /*extensions=*/nullptr, global_template);

    BindingList binding_list;
    context->Global()->SetAlignedPointerInInternalField(kBindingListIndex,
                                                        &binding_list);

    // Enter the context for compiling and running the hello world script.
    Context::Scope context_scope(context);
    // Create a string containing the JavaScript source code.
    Local<String> source =
        String::NewFromUtf8(isolate_holder_->isolate(), test.c_str(),
                            NewStringType::kNormal)
            .ToLocalChecked();
    // Compile the source code.
    Local<Script> script = Script::Compile(context, source).ToLocalChecked();
    // Run the script to get the result.
    Local<Value> result = script->Run(context).ToLocalChecked();
    // Convert the result to an UTF8 string and print it.
    String::Utf8Value utf8(isolate_holder_->isolate(), result);
    return *utf8;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
};

TEST_F(TestTextEncoder, BasicTest) {
  // Verify our test infra is working properly.
  EXPECT_EQ("97", Run("\"a\".charCodeAt(0)"));
  // Verify we can create a TextEncoder.
  EXPECT_EQ("97", Run("new TextEncoder();\"a\".charCodeAt(0)"));
  // Verift we can encode a character.
  EXPECT_EQ("97", Run("new TextEncoder().encode(\"a\")[0]"));
  // Verify we can encode two character strings.
  EXPECT_EQ("97", Run("new TextEncoder().encode(\"ab\")[0]"));
  EXPECT_EQ("98", Run("new TextEncoder().encode(\"ab\")[1]"));
  // Verify we can save a TextEncoder in a variable.
  EXPECT_EQ("97", Run("let te = new TextEncoder(); \
                       te.encode(\"a\")[0]"));
  // Verify we can encode two strings.
  EXPECT_EQ("97", Run("let te = new TextEncoder(); \
                       te.encode(\"b\"); \
                       te.encode(\"a\")[0]"));

  // Verify we can encode two strings without modifying first string.
  EXPECT_EQ("98", Run("let te = new TextEncoder(); \
                       let x = te.encode(\"b\"); \
                       te.encode(\"a\"); \
                       x[0]"));
  // Encode a 3 byte UTF character.
  EXPECT_EQ("226,130,172", Run("new TextEncoder().encode('€')"));
  // Encode a 4 byte UTF character.
  EXPECT_EQ("240,157,159,152", Run("new TextEncoder().encode('𝟘')"));
  // Try first 128 characters.
  EXPECT_EQ("true", Run("var r = true; \
                         var te = new TextEncoder(); \
                         for (var i = 0; i < 128; i++) \
                           r &&= i == te.encode(String.fromCharCode(i))"));
  // Verify stringification of args.
  EXPECT_EQ("49", Run("new TextEncoder().encode(1)"));
  EXPECT_EQ("49,44,97", Run("new TextEncoder().encode([1,'a'])"));
  EXPECT_EQ("50", Run("new TextEncoder().encode(new Uint8Array([2]))"));
  EXPECT_EQ("52,57",
            Run("new TextEncoder().encode(new TextEncoder().encode(1))"));
  // Verify no args produces empty array.
  EXPECT_EQ("", Run("new TextEncoder().encode()"));
  // Verify null gets stringified.
  EXPECT_EQ("110,117,108,108", Run("new TextEncoder().encode(null)"));
  // Verify extra args are ignored.
  EXPECT_EQ("49", Run("new TextEncoder().encode(1, 2)"));
  EXPECT_EQ("49", Run("new TextEncoder().encode(1, 'a')"));
  EXPECT_EQ("97", Run("new TextEncoder().encode('a', 1)"));
  EXPECT_EQ("97", Run("new TextEncoder().encode('a', 'b')"));
  EXPECT_EQ("49", Run("new TextEncoder().encode(new Uint8Array([1]), 1)"));
  EXPECT_EQ("49", Run("new TextEncoder().encode(new Uint8Array([1]), 'a')"));
  EXPECT_EQ("97", Run("new TextEncoder().encode('a', new Uint8Array([1]))"));
  EXPECT_EQ("49", Run("new TextEncoder().encode(new Uint8Array([1]), new "
                      "Uint8Array([2]))"));
}

}  // namespace
}  // namespace text_encoder
