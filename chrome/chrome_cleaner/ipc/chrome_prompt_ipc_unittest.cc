// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/chrome_cleaner/ipc/ipc_test_util.h"
#include "chrome/chrome_cleaner/logging/scoped_logging.h"
#include "chrome/chrome_cleaner/test/test_name_helper.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {
namespace {

using testing::Bool;
using testing::Values;

constexpr char kIncludeUwESwitch[] = "include-uwe";
constexpr char kIncludeUwSSwitch[] = "include-uws";
constexpr char kIncludeRegistryKeysSwitch[] = "include-registry-keys";
constexpr char kExpectedPromptResultSwitch[] = "expected-prompt-result";
constexpr char kExpectedParentDisconnectedSwitch[] =
    "expected-parent-disconnected";

const base::FilePath kBadFilePath(L"/path/to/bad.dll");
const base::string16 kBadRegistryKey(L"HKCU:32\\Software\\ugly-uws\\nasty");
const base::string16 kExtensionId(L"expected-extension-id");

// Possible moments when the parent process can disconnect from the IPC to
// check connection error handling in the child process.
enum class ParentDisconnected {
  // The parent process will not try to disconnect while the child process
  // is running.
  kNone,
  // The parent process will disconnect before the child process sends a
  // message through the pipe.
  kOnStartup,
  // The parent process will disconnect after receiving a message from the
  // child process and before sending out a response.
  kWhileProcessingChildRequest,
  // The parent process will disconnect once no further communication is
  // required in the child process.
  kOnDone,
};

std::ostream& operator<<(std::ostream& stream,
                         ParentDisconnected parent_disconnected) {
  switch (parent_disconnected) {
    case ParentDisconnected::kNone:
      stream << "NotDisconnected";
      break;
    case ParentDisconnected::kOnStartup:
      stream << "DisconnectedOnStartup";
      break;
    case ParentDisconnected::kWhileProcessingChildRequest:
      stream << "DisconnectedWhileProcessingChildRequest";
      break;
    case ParentDisconnected::kOnDone:
      stream << "DisconnectedOnDone";
      break;
  }
  return stream;
}

struct TestConfig {
  bool uws_expected;
  bool uwe_expected;
  bool with_registry_keys;
  mojom::PromptAcceptance expected_prompt_acceptance;
  ParentDisconnected expected_parent_disconnected;
};

// Parent process.
class MockChromePrompt : public mojom::ChromePrompt {
 public:
  MockChromePrompt(TestConfig test_config, mojom::ChromePromptRequest request)
      : test_config_(test_config), binding_(this, std::move(request)) {}

  ~MockChromePrompt() override = default;

  void PromptUser(
      const std::vector<base::FilePath>& files_to_delete,
      const base::Optional<std::vector<base::string16>>& registry_keys,
      const base::Optional<std::vector<base::string16>>& extension_ids,
      mojom::ChromePrompt::PromptUserCallback callback) override {
    EXPECT_NE(test_config_.uws_expected, files_to_delete.empty());
    if (test_config_.uws_expected) {
      EXPECT_EQ(1UL, files_to_delete.size());
      EXPECT_EQ(kBadFilePath, files_to_delete.front());
    }
    if (test_config_.with_registry_keys) {
      EXPECT_EQ(1UL, registry_keys->size());
      EXPECT_EQ(kBadRegistryKey, registry_keys->front());
    }
    CloseConnectionIf(ParentDisconnected::kWhileProcessingChildRequest);
    std::move(callback).Run(test_config_.expected_prompt_acceptance);
    if (!test_config_.uwe_expected) {
      CloseConnectionIf(ParentDisconnected::kOnDone);
    }
  }

  void DisableExtensions(const std::vector<base::string16>& extension_ids,
                         DisableExtensionsCallback callback) override {
    EXPECT_NE(test_config_.uwe_expected, extension_ids.empty());
    if (test_config_.uwe_expected) {
      EXPECT_EQ(kExtensionId, extension_ids.front());
    } else {
      EXPECT_EQ(0UL, extension_ids.size());
    }
    std::move(callback).Run(true);
    CloseConnectionIf(ParentDisconnected::kOnDone);
  }

  // Close the IPC connection on the parent process depending on the value of
  // |parent_disconnected|.
  void CloseConnectionIf(ParentDisconnected parent_disconnected) {
    if (test_config_.expected_parent_disconnected == parent_disconnected)
      binding_.Close();
  }

  TestConfig test_config_;
  mojo::Binding<chrome_cleaner::mojom::ChromePrompt> binding_;
};

class ChromePromptIPCParentProcess : public ParentProcess {
 public:
  ChromePromptIPCParentProcess(TestConfig test_config,
                               scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ParentProcess(std::move(mojo_task_runner)), test_config_(test_config) {
    if (test_config.uws_expected)
      AppendSwitch(kIncludeUwSSwitch);
    if (test_config.uwe_expected) {
      AppendSwitch(kIncludeUwESwitch);
    }
    if (test_config.with_registry_keys)
      AppendSwitch(kIncludeRegistryKeysSwitch);
    AppendSwitch(kExpectedPromptResultSwitch,
                 base::IntToString(
                     static_cast<int>(test_config.expected_prompt_acceptance)));
    AppendSwitch(kExpectedParentDisconnectedSwitch,
                 base::IntToString(static_cast<int>(
                     test_config.expected_parent_disconnected)));
  }

 protected:
  void CreateImpl(mojo::ScopedMessagePipeHandle mojo_pipe) override {
    chrome_cleaner::mojom::ChromePromptRequest chrome_prompt_request(
        std::move(mojo_pipe));
    mock_chrome_prompt_ = std::make_unique<MockChromePrompt>(
        test_config_, std::move(chrome_prompt_request));
    // At this point, the child process should be connected.
    mock_chrome_prompt_->CloseConnectionIf(ParentDisconnected::kOnStartup);
  }

  void DestroyImpl() override { mock_chrome_prompt_.reset(); }

 private:
  ~ChromePromptIPCParentProcess() override = default;

  TestConfig test_config_;
  std::unique_ptr<MockChromePrompt> mock_chrome_prompt_;
};

class ChromePromptIPCTestErrorHandler : public ChromePromptIPC::ErrorHandler {
 public:
  ChromePromptIPCTestErrorHandler(base::OnceClosure on_closed,
                                  base::OnceClosure on_closed_after_done)
      : on_closed_(std::move(on_closed)),
        on_closed_after_done_(std::move(on_closed_after_done)) {}

  ~ChromePromptIPCTestErrorHandler() override = default;

  void OnConnectionClosed() override { std::move(on_closed_).Run(); }

  void OnConnectionClosedAfterDone() override {
    std::move(on_closed_after_done_).Run();
  }

  base::OnceClosure on_closed_;
  base::OnceClosure on_closed_after_done_;
};

// Child process.
class ChromePromptIPCChildProcess : public ChildProcess {
 public:
  explicit ChromePromptIPCChildProcess(
      scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ChildProcess(std::move(mojo_task_runner)) {}

  void SendUwEDataToParentProcess(ChromePromptIPC* chrome_prompt_ipc,
                                  base::OnceClosure done) {
    CHECK(chrome_prompt_ipc);
    std::vector<base::string16> extensions;
    if (uwe_expected()) {
      extensions.push_back(kExtensionId);
    } else {
      std::move(done).Run();
      return;
    }
    chrome_prompt_ipc->PostDisableExtensionsTask(
        std::move(extensions),
        base::BindOnce(
            &ChromePromptIPCChildProcess::ReceiveDisableExtensionsResult,
            base::Unretained(this), base::Passed(&done)));
  }

  void SendUwSDataToParentProcess(ChromePromptIPC* chrome_prompt_ipc,
                                  base::OnceClosure done) {
    CHECK(chrome_prompt_ipc);

    std::vector<base::FilePath> files_to_delete;
    std::vector<base::string16> registry_keys;
    if (uws_expected()) {
      files_to_delete.push_back(kBadFilePath);
      if (with_registry_keys())
        registry_keys.push_back(kBadRegistryKey);
    }

    chrome_prompt_ipc->PostPromptUserTask(
        std::move(files_to_delete), std::move(registry_keys),
        base::BindOnce(&ChromePromptIPCChildProcess::ReceivePromptResult,
                       base::Unretained(this), base::Passed(&done)));
  }

  ParentDisconnected expected_parent_disconnected() const {
    int val = -1;
    CHECK(base::StringToInt(
        command_line().GetSwitchValueASCII(kExpectedParentDisconnectedSwitch),
        &val));
    return static_cast<ParentDisconnected>(val);
  }

 private:
  ~ChromePromptIPCChildProcess() override = default;

  void ReceivePromptResult(base::OnceClosure done,
                           mojom::PromptAcceptance prompt_acceptance) {
    CHECK_EQ(expected_prompt_acceptance(), prompt_acceptance);
    // Unblocks the main thread.
    std::move(done).Run();
  }

  void ReceiveDisableExtensionsResult(base::OnceClosure done, bool completed) {
    CHECK(completed);
    std::move(done).Run();
  }

  bool uws_expected() const {
    return command_line().HasSwitch(kIncludeUwSSwitch);
  }

  bool uwe_expected() const {
    return command_line().HasSwitch(kIncludeUwESwitch);
  }

  bool with_registry_keys() const {
    return uws_expected() &&
           command_line().HasSwitch(kIncludeRegistryKeysSwitch);
  }

  mojom::PromptAcceptance expected_prompt_acceptance() const {
    int val = -1;
    CHECK(base::StringToInt(
        command_line().GetSwitchValueASCII(kExpectedPromptResultSwitch), &val));
    return static_cast<mojom::PromptAcceptance>(val);
  }
};

constexpr int kEarlyDisconnectionExitCode = 100;
constexpr int kSuccessExitCode = 0;

MULTIPROCESS_TEST_MAIN(ChromePromptIPCClientMain) {
  static constexpr int kInternalTestFailureExitCode = -1;

  base::MessageLoop message_loop;

  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  auto child_process =
      base::MakeRefCounted<ChromePromptIPCChildProcess>(mojo_task_runner);
  base::RunLoop on_done_run_loop;
  // The parent process can disconnect while the pipe is required or after it's
  // no longer needed. In the former case, the child process will immediately
  // exit; in the latter, it will break |on_done_run_loop|, which will be
  // blocking its main thread.
  ChromePromptIPCTestErrorHandler error_handler(
      base::BindOnce([] {
        exit(::testing::Test::HasFailure() ? kInternalTestFailureExitCode
                                           : kEarlyDisconnectionExitCode);
      }),
      on_done_run_loop.QuitClosure());

  ChromePromptIPC* chrome_prompt_ipc =
      new ChromePromptIPC(child_process->mojo_pipe_token(), mojo_task_runner);
  chrome_prompt_ipc->Initialize(&error_handler);

  if (child_process->expected_parent_disconnected() ==
      ParentDisconnected::kOnStartup) {
    // If a failure on startup is expected, the child process will wait until
    // the pipe gets disconnected (which will terminate the process), or
    // eventually timeout if the disconnection is never received.
    base::RunLoop().Run();
  }

  // After the response from the parent process is received, this will post a
  // task to unblock the child process's main thread. Not blocking the main
  // thread can lead to race condition on exit.
  base::RunLoop prompt_user_run_loop;
  base::RunLoop extension_run_loop;
  child_process->SendUwSDataToParentProcess(chrome_prompt_ipc,
                                            prompt_user_run_loop.QuitClosure());
  prompt_user_run_loop.Run();
  child_process->SendUwEDataToParentProcess(chrome_prompt_ipc,
                                            extension_run_loop.QuitClosure());
  extension_run_loop.Run();

  if (child_process->expected_parent_disconnected() ==
      ParentDisconnected::kOnDone) {
    // Only block the main thread at this point if the parent process is
    // expected to disconnect once communication is over.
    on_done_run_loop.Run();
  }

  return ::testing::Test::HasFailure() ? kInternalTestFailureExitCode
                                       : kSuccessExitCode;
}

class ChromePromptIPCTest
    : public ::testing::TestWithParam<std::tuple<bool,
                                                 bool,
                                                 bool,
                                                 mojom::PromptAcceptance,
                                                 ParentDisconnected>> {
 public:
  void SetUp() override { mojo_task_runner_ = MojoTaskRunner::Create(); }

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  scoped_refptr<ChromePromptIPCParentProcess> parent_process_;
};

TEST_P(ChromePromptIPCTest, Communication) {
  TestConfig test_config;
  std::tie(test_config.uws_expected, test_config.uwe_expected,
           test_config.with_registry_keys,
           test_config.expected_prompt_acceptance,
           test_config.expected_parent_disconnected) = GetParam();
  if (test_config.with_registry_keys)
    ASSERT_TRUE(test_config.uws_expected);

  parent_process_ = base::MakeRefCounted<ChromePromptIPCParentProcess>(
      test_config, mojo_task_runner_);

  int32_t exit_code = -1;
  EXPECT_TRUE(parent_process_->LaunchConnectedChildProcess(
      "ChromePromptIPCClientMain", &exit_code));

  int32_t expected_exit_code =
      test_config.expected_parent_disconnected == ParentDisconnected::kNone ||
              test_config.expected_parent_disconnected ==
                  ParentDisconnected::kOnDone
          ? kSuccessExitCode
          : kEarlyDisconnectionExitCode;
  EXPECT_EQ(expected_exit_code, exit_code);
}

INSTANTIATE_TEST_CASE_P(NoUwSPresent,
                        ChromePromptIPCTest,
                        testing::Combine(
                            /*uws_expected=*/Values(false),
                            /*uwe_expected=*/Values(false),
                            /*with_registry_keys=*/Values(false),
                            Values(mojom::PromptAcceptance::DENIED),
                            Values(ParentDisconnected::kNone,
                                   ParentDisconnected::kOnStartup)),
                        GetParamNameForTest());

INSTANTIATE_TEST_CASE_P(
    UwSPresent,
    ChromePromptIPCTest,
    testing::Combine(
        /*uws_expected=*/Values(true),
        /*uwe_expected=*/Bool(),
        /*with_registry_keys=*/Bool(),
        Values(mojom::PromptAcceptance::ACCEPTED_WITH_LOGS,
               mojom::PromptAcceptance::ACCEPTED_WITHOUT_LOGS,
               mojom::PromptAcceptance::DENIED),
        Values(ParentDisconnected::kNone,
               ParentDisconnected::kOnStartup,
               ParentDisconnected::kWhileProcessingChildRequest,
               ParentDisconnected::kOnDone)),
    GetParamNameForTest());

}  // namespace
}  // namespace chrome_cleaner
