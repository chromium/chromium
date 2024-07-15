// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/extension_hooks_delegate.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "content/public/common/content_constants.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/api/messaging/send_message_tester.h"
#include "extensions/renderer/api/runtime_hooks_delegate.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"

namespace extensions {

class ExtensionHooksDelegateTest
    : public NativeExtensionBindingsSystemUnittest {
 public:
  ExtensionHooksDelegateTest() {}

  ExtensionHooksDelegateTest(const ExtensionHooksDelegateTest&) = delete;
  ExtensionHooksDelegateTest& operator=(const ExtensionHooksDelegateTest&) =
      delete;

  ~ExtensionHooksDelegateTest() override {}

  // NativeExtensionBindingsSystemUnittest:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();
    messaging_service_ =
        std::make_unique<NativeRendererMessagingService>(bindings_system());

    bindings_system()->api_system()->RegisterHooksDelegate(
        "extension",
        std::make_unique<ExtensionHooksDelegate>(messaging_service_.get()));
    bindings_system()->api_system()->RegisterHooksDelegate(
        "runtime",
        std::make_unique<RuntimeHooksDelegate>(messaging_service_.get()));

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
    // Some extension module methods only exist in MV2.
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

// Test chrome.extension messaging methods. Many of these are just aliased to
// chrome.runtime counterparts, but some others (like sendRequest) are
// implemented as hooks.
TEST_F(ExtensionHooksDelegateTest, MessagingSanityChecks) {
  v8::HandleScope handle_scope(isolate());

  MessageTarget self_target = MessageTarget::ForExtension(extension()->id());
  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "extension");

  tester.TestConnect("", "", self_target);

  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  tester.TestSendMessage("{data: 'hello'}", kStandardMessage, self_target,
                         SendMessageTester::CLOSED);
  tester.TestSendMessage("{data: 'hello'}, function() {}", kStandardMessage,
                         self_target, SendMessageTester::OPEN);

  tester.TestSendRequest("{data: 'hello'}", kStandardMessage, self_target,
                         SendMessageTester::CLOSED);
  tester.TestSendRequest("{data: 'hello'}, function() {}", kStandardMessage,
                         self_target, SendMessageTester::OPEN);

  // Ambiguous argument case ('hi' could be an id or a message); we massage it
  // into being the message because that's a required argument.
  tester.TestSendRequest("'hi', function() {}", "\"hi\"", self_target,
                         SendMessageTester::OPEN);
}

TEST_F(ExtensionHooksDelegateTest, SendRequestDisabled) {
  // Construct an extension for which sendRequest is disabled (unpacked
  // extension with an event page).
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .SetManifestVersion(2)
          .SetBackgroundContext(ExtensionBuilder::BackgroundContext::EVENT_PAGE)
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = AddContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context);
  ASSERT_TRUE(messaging_util::IsSendRequestDisabled(script_context));

  enum AccessBehavior {
    THROWS,
    DOESNT_THROW,
  };

  auto check_access = [context](const char* object, AccessBehavior behavior) {
    SCOPED_TRACE(object);
    constexpr char kExpectedError[] =
        "Uncaught Error: extension.sendRequest, extension.onRequest, and "
        "extension.onRequestExternal are deprecated. Please use "
        "runtime.sendMessage, runtime.onMessage, and runtime.onMessageExternal "
        "instead.";
    v8::Local<v8::Function> function = FunctionFromString(
        context, base::StringPrintf("(function() {%s;})", object));
    if (behavior == THROWS)
      RunFunctionAndExpectError(function, context, 0, nullptr, kExpectedError);
    else
      RunFunction(function, context, 0, nullptr);
  };

  check_access("chrome.extension.sendRequest", THROWS);
  check_access("chrome.extension.onRequest", THROWS);
  check_access("chrome.extension.onRequestExternal", THROWS);
  check_access("chrome.extension.sendMessage", DOESNT_THROW);
  check_access("chrome.extension.onMessage", DOESNT_THROW);
  check_access("chrome.extension.onMessageExternal", DOESNT_THROW);
}

// Ensure that the extension.sendRequest() method doesn't close the channel if
// the listener does not reply and also does not return `true` (unlike the
// runtime.sendMessage() method, which will).
TEST_F(ExtensionHooksDelegateTest, SendRequestChannelLeftOpenToReplyAsync) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kRegisterListener[] =
      "(function() {\n"
      "  chrome.extension.onRequest.addListener(\n"
      "      function(message, sender, reply) {});\n"
      "})";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kRegisterListener);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  const std::string kChannel = "chrome.extension.sendRequest";
  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false,
                       mojom::SerializationFormat::kJson);

  NativeRendererMessagingService::TabConnectionInfo tab_connection_info;
  NativeRendererMessagingService::ExternalConnectionInfo
      external_connection_info;
  tab_connection_info.frame_id = 0;
  GURL source_url("http://example.com");
  // We'd normally also have a tab here (stored in `tab_connection_info.tab`),
  // but then we need a very large JSON object for it to comply with our
  // schema. Just pretend it's not there.
  external_connection_info.target_id = extension()->id();
  external_connection_info.source_endpoint =
      MessagingEndpoint::ForExtension(extension()->id());
  external_connection_info.source_url = source_url;
  external_connection_info.guest_process_id =
      content::kInvalidChildProcessUniqueId;
  external_connection_info.guest_render_frame_routing_id = 0;

  // Open a receiver for the message.
  mojo::PendingAssociatedRemote<mojom::MessagePortHost> port_host_remote;
  auto port_host_receiver =
      port_host_remote.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedReceiver<mojom::MessagePort> port_receiver;
  auto port_remote = port_receiver.InitWithNewEndpointAndPassRemote();

  bool port_opened = false;
  messaging_service()->DispatchOnConnect(
      script_context_set(), port_id, mojom::ChannelType::kSendRequest, kChannel,
      tab_connection_info, external_connection_info, std::move(port_receiver),
      std::move(port_host_remote), nullptr,
      base::BindLambdaForTesting(
          [&port_opened](bool success) { port_opened = success; }));
  port_host_receiver.EnableUnassociatedUsage();
  port_remote.EnableUnassociatedUsage();
  EXPECT_TRUE(port_opened);
  EXPECT_TRUE(
      messaging_service()->HasPortForTesting(script_context(), port_id));

  // Post the message to the receiver. Since the receiver doesn't respond, the
  // channel should remain open.
  messaging_service()->DeliverMessage(
      script_context_set(), port_id,
      Message("\"message\"", mojom::SerializationFormat::kJson, false),
      nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_TRUE(
      messaging_service()->HasPortForTesting(script_context(), port_id));
}

// Tests that overriding the runtime equivalents of chrome.extension methods
// with accessors that throw does not cause a crash on access. Regression test
// for https://crbug.com/949170.
TEST_F(ExtensionHooksDelegateTest, RuntimeAliasesCorrupted) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  // Set a trap on chrome.runtime.sendMessage.
  constexpr char kMutateChromeRuntime[] =
      R"((function() {
           Object.defineProperty(
               chrome.runtime, 'sendMessage',
               { get() { throw new Error('haha'); } });
         }))";
  RunFunctionOnGlobal(FunctionFromString(context, kMutateChromeRuntime),
                      context, 0, nullptr);

  // Touch chrome.extension.sendMessage, which is aliased to the runtime
  // version. Though an error is thrown, we shouldn't crash.
  constexpr char kTouchExtensionSendMessage[] =
      "(function() { chrome.extension.sendMessage; })";
  RunFunctionOnGlobal(FunctionFromString(context, kTouchExtensionSendMessage),
                      context, 0, nullptr);
}

// Ensure that HandleGetURL allows extension URLs and doesn't allow arbitrary
// non-extension URLs. Very similar to RuntimeHooksDeligateTest that tests a
// similar function.
TEST_F(ExtensionHooksDelegateTest, GetURL) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto get_url = [this, context](const char* args, const GURL& expected_url) {
    SCOPED_TRACE(base::StringPrintf("Args: `%s`", args));
    constexpr char kGetUrlTemplate[] =
        "(function() { return chrome.extension.getURL(%s); })";
    v8::Local<v8::Function> get_url =
        FunctionFromString(context, base::StringPrintf(kGetUrlTemplate, args));
    v8::Local<v8::Value> url = RunFunction(get_url, context, 0, nullptr);
    ASSERT_FALSE(url.IsEmpty());
    ASSERT_TRUE(url->IsString());
    EXPECT_EQ(expected_url.spec(), gin::V8ToString(isolate(), url));
  };

  get_url("''", extension()->url());
  get_url("'foo'", extension()->GetResourceURL("foo"));
  get_url("'/foo'", extension()->GetResourceURL("foo"));
  get_url("'https://www.google.com'",
          GURL(extension()->url().spec() + "https://www.google.com"));
}

class ExtensionHooksDelegateMV3Test : public ExtensionHooksDelegateTest {
 public:
  ExtensionHooksDelegateMV3Test() = default;
  ~ExtensionHooksDelegateMV3Test() override = default;

  scoped_refptr<const Extension> BuildExtension() override {
    return ExtensionBuilder("foo").SetManifestVersion(3).Build();
  }
};

TEST_F(ExtensionHooksDelegateMV3Test, AliasesArentAvailableInMV3) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto script_to_value = [context](std::string_view source) {
    return V8ToString(V8ValueFromScriptSource(context, source), context);
  };

  EXPECT_EQ("undefined", script_to_value("chrome.extension.connect"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.connectNative"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.getURL"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.onConnect"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.onConnectExternal"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.onMessage"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.onMessageExternal"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.onRequest"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.onRequestExternal"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.sendNativeMessage"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.sendMessage"));
  EXPECT_EQ("undefined", script_to_value("chrome.extension.sendRequest"));
}

}  // namespace extensions
