// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_BROWSERTEST_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_BROWSERTEST_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/bindings/bindings_manager_cast.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

namespace {

const base::FilePath::CharType kTestDataPath[] =
    FILE_PATH_LITERAL("chromecast/bindings/testdata");

base::FilePath GetTestDataPath() {
  return base::FilePath(kTestDataPath);
}

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
  return file_path.Append(GetTestDataPath()).AppendASCII(name);
}

// Mojo MessagePort Utils:
mojo::Message MojoMessageFromUtf8(base::StringPiece message_utf8) {
  blink::TransferableMessage transfer_message;
  transfer_message.owned_encoded_message =
      blink::EncodeStringMessage(base::UTF8ToUTF16(message_utf8));
  transfer_message.encoded_message = transfer_message.owned_encoded_message;
  return blink::mojom::TransferableMessage::SerializeAsMessage(
      &transfer_message);
}

base::Optional<std::string> ReadMessagePayloadAsUtf8(mojo::Message message) {
  blink::TransferableMessage transferable_message;
  if (!blink::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(message), &transferable_message)) {
    return base::nullopt;
  }

  if (!transferable_message.ports.empty()) {
    LOG(ERROR) << "TransferableMessage has unexpected ports.";
  }

  base::string16 data_utf16;
  if (!blink::DecodeStringMessage(transferable_message.encoded_message,
                                  &data_utf16)) {
    return base::nullopt;
  }

  std::string output;
  if (!base::UTF16ToUTF8(data_utf16.data(), data_utf16.size(), &output))
    return base::nullopt;

  return base::make_optional<std::string>(output);
}

class TitleChangeObserver : public CastWebContents::Observer {
 public:
  TitleChangeObserver() = default;
  ~TitleChangeObserver() override = default;

  // Spins a Runloop until the title of the page matches the |expected_title|
  // that have been set.
  void RunUntilTitleEquals(base::StringPiece expected_title) {
    expected_title_ = expected_title.as_string();
    // Spin the runloop until the expected conditions are met.
    if (current_title_ != expected_title_) {
      expected_title_ = expected_title.as_string();
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  // CastWebContents::Observer implementation:
  void UpdateTitle(const base::string16& title) override {
    // Resumes execution of RunUntilTitleEquals() if |title| matches
    // expectations.
    std::string title_utf8 = base::UTF16ToUTF8(title);
    current_title_ = title_utf8;
    if (!quit_closure_.is_null() && current_title_ == expected_title_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  std::string current_title_;
  std::string expected_title_;

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TitleChangeObserver);
};

// Test class for communicating with connector.html.
class TestBindingBackend : public mojo::MessageReceiver {
 public:
  TestBindingBackend(bindings::BindingsManager* bindings_manager)
      : bindings_manager_(bindings_manager) {
    constexpr char kPortName[] = "hello";
    bindings_manager_->RegisterPortHandler(
        kPortName, base::BindRepeating(&TestBindingBackend::OnPortConnected,
                                       base::Unretained(this)));
  }

  ~TestBindingBackend() override {
    connector_.reset();
    constexpr char kPortName[] = "hello";
    bindings_manager_->UnregisterPortHandler(kPortName);
  }

  // Start the RunLoop until OnPortConnected.
  void RunUntilPortConnected() {
    if (connector_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Run the callback if there is at least one available message
  // cached.
  void ReceiveMessage(base::OnceCallback<void(std::string)> callback) {
    ASSERT_TRUE(message_received_callback_.is_null())
        << "Only one waiting event is allowed.";

    // Run the callback immediately if we have cached some messages.
    if (!message_queue_.empty()) {
      std::move(message_received_callback_).Run(message_queue_.front());
      message_queue_.pop_front();
      return;
    }

    message_received_callback_ = std::move(callback);
  }

  void SendMessageToPage(base::StringPiece message) {
    if (!connector_)
      return;

    DCHECK(!message.empty());

    mojo::Message mojo_message = MojoMessageFromUtf8(message);
    connector_->Accept(&mojo_message);
  }

  void SetPortDisconnectedCallback(
      base::OnceClosure port_disconnected_callback) {
    DCHECK(!port_disconnected_callback_);
    port_disconnected_callback_ = std::move(port_disconnected_callback);
  }

 private:
  // Called when a port was received from the page.
  void OnPortConnected(mojo::ScopedMessagePipeHandle port) {
    if (!quit_closure_.is_null()) {
      std::move(quit_closure_).Run();
    }
    connector_ = std::make_unique<mojo::Connector>(
        std::move(port), mojo::Connector::SINGLE_THREADED_SEND,
        base::ThreadTaskRunnerHandle::Get());
    connector_->set_connection_error_handler(base::BindOnce(
        &TestBindingBackend::OnPortDisconnected, base::Unretained(this)));
    connector_->set_incoming_receiver(this);
  }

  // Called when the peer disconnected the port.
  void OnPortDisconnected() {
    LOG(INFO) << "TestBindingBackend port disconnected.";
    connector_.reset();
    if (port_disconnected_callback_) {
      std::move(port_disconnected_callback_).Run();
    }
  }

  // mojo::MessageReceiver implementation:
  bool Accept(mojo::Message* message) override {
    base::Optional<std::string> message_json =
        ReadMessagePayloadAsUtf8(std::move(*message));
    if (!message_json)
      return false;

    if (message_received_callback_) {
      // Number of cached messages must be zero in this case.
      DCHECK(message_queue_.empty());
      std::move(message_received_callback_).Run(message_json.value());
      return true;
    }
    // Cache received message until external caller access it
    // via TestBindingBackend::ReceiveMessage
    message_queue_.emplace_back(message_json.value());
    return true;
  }

  base::OnceClosure quit_closure_;

  bindings::BindingsManager* const bindings_manager_;

  // Used for sending and receiving messages over the MessagePort.
  std::unique_ptr<mojo::Connector> connector_;

  base::circular_deque<std::string> message_queue_;
  base::OnceCallback<void(std::string)> message_received_callback_;
  base::OnceClosure port_disconnected_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestBindingBackend);
};

// =============================================================================
// Mocks
// =============================================================================
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD1(CloseContents, void(content::WebContents* source));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebContentsDelegate);
};

class MockCastWebContentsDelegate
    : public base::SupportsWeakPtr<MockCastWebContentsDelegate>,
      public CastWebContents::Delegate {
 public:
  MockCastWebContentsDelegate() {}
  ~MockCastWebContentsDelegate() override = default;

  MOCK_METHOD2(InnerContentsCreated,
               void(CastWebContents* inner_contents,
                    CastWebContents* outer_contents));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastWebContentsDelegate);
};

}  // namespace

// =============================================================================
// Test class
// =============================================================================
class BindingsManagerCastBrowserTest : public content::BrowserTestBase {
 protected:
  BindingsManagerCastBrowserTest() = default;
  ~BindingsManagerCastBrowserTest() override = default;

  void SetUp() final {
    SetUpCommandLine(base::CommandLine::ForCurrentProcess());
    BrowserTestBase::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) final {
    command_line->AppendSwitch(switches::kNoWifi);
    command_line->AppendSwitchASCII(switches::kTestType, "browser");
  }

  void PreRunTestOnMainThread() override {
    // Pump startup related events.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::RunLoop().RunUntilIdle();

    metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
    content::WebContents::CreateParams create_params(
        shell::CastBrowserProcess::GetInstance()->browser_context(), nullptr);
    web_contents_ = content::WebContents::Create(create_params);
    web_contents_->SetDelegate(&mock_wc_delegate_);

    // CastWebContents::Delegate must be set for receiving PageStateChanged
    // event.
    CastWebContents::InitParams init_params;
    init_params.delegate = mock_cast_wc_delegate_.AsWeakPtr();
    init_params.is_root_window = true;

    cast_web_contents_ =
        std::make_unique<CastWebContentsImpl>(web_contents_.get(), init_params);
    title_change_observer_.Observe(cast_web_contents_.get());
    bindings_manager_ = std::make_unique<bindings::BindingsManagerCast>();
  }

  void PostRunTestOnMainThread() override {
    cast_web_contents_.reset();
    web_contents_.reset();
    bindings_manager_.reset();
  }

  void StartTestServer() {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  NiceMock<MockCastWebContentsDelegate> mock_cast_wc_delegate_;
  NiceMock<MockWebContentsDelegate> mock_wc_delegate_;
  TitleChangeObserver title_change_observer_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<CastWebContentsImpl> cast_web_contents_;

  std::unique_ptr<bindings::BindingsManagerCast> bindings_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BindingsManagerCastBrowserTest);
};

// =============================================================================
// Test cases
// =============================================================================
IN_PROC_BROWSER_TEST_F(BindingsManagerCastBrowserTest, EndToEnd) {
  // ===========================================================================
  // Test: Load BindingsManagerCast, ensure binding backend can receive a port
  // via BindingsManagerCast and the port is good to use.
  // Step 1: Create a TestBindingBackend object. TestBindingBackend will
  // register a PortHandler to BindingsManagerCast.
  // Step 2: Attach |bindings_manager_cast_| to |cast_web_contents_|, port
  // connector binding should be injected into |cast_web_contents_|.
  // Step 3: Load the test page, expected behaviours include:
  //  - BindingsManagerCast posts one end of MessagePort to the page.
  //    NamedMessagePort binding should be able to forward ports to native.
  //  - BindingManagerCast should successfully route a connected MessagePort to
  //    TestBindingBackend. This port is created by test page "connector.html".
  // Step 4: Verify that messages that are sent through the port are cached
  // before the port is not routed to native. And make sure TestBindingBackend
  // could use the |bindings_manager_cast_| provided port to send & receive
  // messages. Note: Messages should arrive in order.
  // ===========================================================================
  GURL test_url =
      content::GetFileUrlWithQuery(GetTestDataFilePath("connector.html"), "");

  // Register test port handler.
  auto test_binding_backend =
      std::make_unique<TestBindingBackend>(bindings_manager_.get());
  // TestBindingBackend test_binding_backend(bindings_manager_.get());

  // Attach BindingsManager to the page.
  bindings_manager_->AttachToPage(cast_web_contents_.get());

  // Load test page.
  constexpr char kTestPageTitle[] = "bindings";
  cast_web_contents_->LoadUrl(test_url);
  title_change_observer_.RunUntilTitleEquals(kTestPageTitle);

  // Start RunLoop until TestBindingBackend receives MessagePort.
  test_binding_backend->RunUntilPortConnected();

  // Send ping message to the test page.
  test_binding_backend->SendMessageToPage("ping");

  // Test that message are received in order.
  std::vector<std::string> test_messages = {"early 1", "early 2", "ack ping"};
  for (auto test_message : test_messages) {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auto received_message_callback = base::BindOnce(
        [](base::OnceClosure loop_quit_closure, std::string expected_msg,
           std::string port_msg) {
          EXPECT_EQ(expected_msg, port_msg);
          std::move(loop_quit_closure).Run();
        },
        std::move(quit_closure), test_message);
    test_binding_backend->ReceiveMessage(std::move(received_message_callback));
    run_loop.Run();
  }

  // Ensure that the MessagePort is dropped when navigating away.
  {
    base::RunLoop run_loop;

    auto port_disconnected_callback = base::BindOnce(
        [](base::OnceClosure loop_quit_closure) {
          std::move(loop_quit_closure).Run();
        },
        run_loop.QuitClosure());
    test_binding_backend->SetPortDisconnectedCallback(
        std::move(port_disconnected_callback));

    cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));

    run_loop.Run();
  }

  // Destruct the binding backend to unregister itself from BindingsManagerCast
  test_binding_backend.reset();
}

}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_BROWSERTEST_H_
