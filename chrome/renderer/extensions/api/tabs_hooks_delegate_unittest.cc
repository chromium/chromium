// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/tabs_hooks_delegate.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/api/messaging/send_message_tester.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

namespace {

void CallAPIAndExpectError(v8::Local<v8::Context> context,
                           const char* method_name,
                           const char* args) {
  SCOPED_TRACE(base::StringPrintf("Args: `%s`", args));
  constexpr char kTemplate[] = "(function() { chrome.tabs.%s(%s); })";

  v8::Isolate* isolate = context->GetIsolate();

  // Just verify some error was thrown. Expecting the exact error message
  // tends to rely too much on our argument spec code, which is tested
  // separately.
  v8::Local<v8::Function> function = FunctionFromString(
      context, base::StringPrintf(kTemplate, method_name, args));
  v8::TryCatch try_catch(isolate);
  v8::MaybeLocal<v8::Value> result =
      function->Call(context, v8::Undefined(isolate), 0, nullptr);
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
}

}  // namespace

class TabsHooksDelegateTest : public NativeExtensionBindingsSystemUnittest {
 public:
  TabsHooksDelegateTest() {}

  TabsHooksDelegateTest(const TabsHooksDelegateTest&) = delete;
  TabsHooksDelegateTest& operator=(const TabsHooksDelegateTest&) = delete;

  ~TabsHooksDelegateTest() override {}

  // NativeExtensionBindingsSystemUnittest:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();
    messaging_service_ =
        std::make_unique<NativeRendererMessagingService>(bindings_system());

    bindings_system()->api_system()->RegisterHooksDelegate(
        "tabs", std::make_unique<TabsHooksDelegate>(messaging_service_.get()));

    scoped_refptr<const Extension> mutable_extension = BuildExtension();
    RegisterExtension(mutable_extension);
    extension_ = mutable_extension;

    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();

    script_context_ =
        CreateScriptContext(context, mutable_extension.get(),
                            mojom::ContextType::kPrivilegedExtension);
    script_context_->set_url(extension_->url());
    bindings_system()->UpdateBindingsForContext(script_context_);
  }
  void TearDown() override {
    script_context_ = nullptr;
    extension_ = nullptr;
    messaging_service_.reset();
    NativeExtensionBindingsSystemUnittest::TearDown();
  }
  bool UseStrictIPCMessageSender() override { return true; }

  virtual scoped_refptr<const Extension> BuildExtension() {
    // TODO(https://crbug.com/40804030): Update this to use MV3.
    // SendMessageTester needs to be updated since runtime.sendMessage() now
    // returns a promise.
    return ExtensionBuilder("foo").SetManifestVersion(2).Build();
  }

  NativeRendererMessagingService* messaging_service() {
    return messaging_service_.get();
  }
  ScriptContext* script_context() { return script_context_; }
  const Extension* extension() { return extension_.get(); }

 private:
  std::unique_ptr<NativeRendererMessagingService> messaging_service_;

  raw_ptr<ScriptContext> script_context_ = nullptr;
  scoped_refptr<const Extension> extension_;
};

TEST_F(TabsHooksDelegateTest, Connect) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  SendMessageTester tester(ipc_message_sender(), script_context(), 0, "tabs");

  tester.TestConnect("1", "",
                     MessageTarget::ForTab(1, messaging_util::kNoFrameId));
  tester.TestConnect("-0", "",
                     MessageTarget::ForTab(0, messaging_util::kNoFrameId));
  tester.TestConnect("4, {name: 'channel'}", "channel",
                     MessageTarget::ForTab(4, messaging_util::kNoFrameId));
  tester.TestConnect("9, {frameId: null}", "",
                     MessageTarget::ForTab(9, messaging_util::kNoFrameId));
  tester.TestConnect("9, {frameId: 16}", "", MessageTarget::ForTab(9, 16));
  tester.TestConnect("25, {}", "",
                     MessageTarget::ForTab(25, messaging_util::kNoFrameId));

  CallAPIAndExpectError(context, "connect", "36, {includeTlsChannelId: true}");
}

TEST_F(TabsHooksDelegateTest, SendMessage) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  SendMessageTester tester(ipc_message_sender(), script_context(), 0, "tabs");

  tester.TestSendMessage("1, ''", R"("")",
                         MessageTarget::ForTab(1, messaging_util::kNoFrameId),
                         SendMessageTester::CLOSED);

  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  tester.TestSendMessage("1, {data: 'hello'}", kStandardMessage,
                         MessageTarget::ForTab(1, messaging_util::kNoFrameId),
                         SendMessageTester::CLOSED);
  tester.TestSendMessage("-0, {data: 'hello'}", kStandardMessage,
                         MessageTarget::ForTab(0, messaging_util::kNoFrameId),
                         SendMessageTester::CLOSED);
  tester.TestSendMessage("1, {data: 'hello'}, function() {}", kStandardMessage,
                         MessageTarget::ForTab(1, messaging_util::kNoFrameId),
                         SendMessageTester::OPEN);
  tester.TestSendMessage("1, {data: 'hello'}, {frameId: null}",
                         kStandardMessage,
                         MessageTarget::ForTab(1, messaging_util::kNoFrameId),
                         SendMessageTester::CLOSED);
  tester.TestSendMessage("1, {data: 'hello'}, {frameId: 10}", kStandardMessage,
                         MessageTarget::ForTab(1, 10),
                         SendMessageTester::CLOSED);
  tester.TestSendMessage("1, {data: 'hello'}, {frameId: 10}, function() {}",
                         kStandardMessage, MessageTarget::ForTab(1, 10),
                         SendMessageTester::OPEN);

  CallAPIAndExpectError(context, "sendMessage",
                        "1, 'hello', {includeTlsChannelId: true}");
}

TEST_F(TabsHooksDelegateTest, SendRequest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  SendMessageTester tester(ipc_message_sender(), script_context(), 0, "tabs");

  tester.TestSendRequest("1, ''", R"("")",
                         MessageTarget::ForTab(1, messaging_util::kNoFrameId),
                         SendMessageTester::CLOSED);

  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  tester.TestSendRequest("1, {data: 'hello'}", kStandardMessage,
                         MessageTarget::ForTab(1, messaging_util::kNoFrameId),
                         SendMessageTester::CLOSED);
  tester.TestSendRequest("1, {data: 'hello'}, function() {}", kStandardMessage,
                         MessageTarget::ForTab(1, messaging_util::kNoFrameId),
                         SendMessageTester::OPEN);

  CallAPIAndExpectError(context, "sendRequest", "1, 'hello', {frameId: 10}");
}

class TabsHooksDelegateMV3Test : public TabsHooksDelegateTest {
 public:
  TabsHooksDelegateMV3Test() = default;
  ~TabsHooksDelegateMV3Test() override = default;

  scoped_refptr<const Extension> BuildExtension() override {
    return ExtensionBuilder("foo")
        .SetManifestKey("manifest_version", 3)
        .Build();
  }
};

TEST_F(TabsHooksDelegateMV3Test, SendMessageUsingPromise) {
  v8::HandleScope handle_scope(isolate());

  SendMessageTester tester(ipc_message_sender(), script_context(), 0, "tabs");

  // The port remains open here after the call because in MV3 we return a
  // promise if the callback parameter is omitted, so we can't use the presence/
  // lack of the callback to determine if the caller is/isn't going to handle
  // the response.
  MessageTarget self_target = MessageTarget::ForExtension(extension()->id());
  tester.TestSendMessage("1, ''", R"("")",
                         MessageTarget::ForTab(1, messaging_util::kNoFrameId),
                         SendMessageTester::OPEN);

  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  {
    // Calling sendMessage with a callback should result in no value returned.
    v8::Local<v8::Value> result = tester.TestSendMessage(
        "1, {data: 'hello'}, function() {}", kStandardMessage,
        MessageTarget::ForTab(1, messaging_util::kNoFrameId),
        SendMessageTester::OPEN);
    EXPECT_TRUE(result->IsUndefined());
  }

  {
    // Calling sendMessage without a callback should result in a promise
    // returned.
    v8::Local<v8::Value> result = tester.TestSendMessage(
        "1, {data: 'hello'}", kStandardMessage,
        MessageTarget::ForTab(1, messaging_util::kNoFrameId),
        SendMessageTester::OPEN);
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());
  }
}

}  // namespace extensions
