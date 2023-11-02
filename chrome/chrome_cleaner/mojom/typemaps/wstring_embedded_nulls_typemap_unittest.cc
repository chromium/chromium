// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/chrome_cleaner/ipc/ipc_test_util.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/test_wstring_embedded_nulls.mojom.h"
#include "chrome/chrome_cleaner/mojom/wstring_embedded_nulls.mojom.h"
#include "chrome/chrome_cleaner/strings/string_test_helpers.h"
#include "chrome/chrome_cleaner/strings/wstring_embedded_nulls.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

using base::WaitableEvent;

class TestWStringEmbeddedNullsImpl : public mojom::TestWStringEmbeddedNulls {
 public:
  explicit TestWStringEmbeddedNullsImpl(
      mojo::PendingReceiver<mojom::TestWStringEmbeddedNulls> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Echo(const WStringEmbeddedNulls& path, EchoCallback callback) override {
    std::move(callback).Run(path);
  }

 private:
  mojo::Receiver<mojom::TestWStringEmbeddedNulls> receiver_;
};

class SandboxParentProcess : public chrome_cleaner::ParentProcess {
 public:
  explicit SandboxParentProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ParentProcess(std::move(mojo_task_runner)) {}

 protected:
  void CreateImpl(mojo::ScopedMessagePipeHandle mojo_pipe) override {
    mojo::PendingReceiver<mojom::TestWStringEmbeddedNulls> receiver(
        std::move(mojo_pipe));
    impl_ = std::make_unique<TestWStringEmbeddedNullsImpl>(std::move(receiver));
  }

  void DestroyImpl() override { impl_.reset(); }

 private:
  ~SandboxParentProcess() override = default;

  std::unique_ptr<TestWStringEmbeddedNullsImpl> impl_;
};

class SandboxChildProcess : public chrome_cleaner::ChildProcess {
 public:
  explicit SandboxChildProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ChildProcess(mojo_task_runner),
        remote_(
            std::make_unique<mojo::Remote<mojom::TestWStringEmbeddedNulls>>()) {
  }

  void BindToPipe(mojo::ScopedMessagePipeHandle mojo_pipe,
                  WaitableEvent* event) {
    remote_->Bind(
        mojo::PendingRemote<chrome_cleaner::mojom::TestWStringEmbeddedNulls>(
            std::move(mojo_pipe), 0));
    event->Signal();
  }

  bool SuccessfulEcho(const WStringEmbeddedNulls& input) {
    WStringEmbeddedNulls output;
    WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                        WaitableEvent::InitialState::NOT_SIGNALED);
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SandboxChildProcess::RunEcho, base::Unretained(this),
                       input, GetSaveAndSignalCallback(&output, &event)));
    event.Wait();

    return input == output;
  }

 private:
  ~SandboxChildProcess() override {
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<mojo::Remote<mojom::TestWStringEmbeddedNulls>>
                   remote) { remote.reset(); },
            std::move(remote_)));
  }

  template <typename EchoedType>
  base::OnceCallback<void(const EchoedType&)> GetSaveAndSignalCallback(
      EchoedType* output,
      WaitableEvent* event) {
    return base::BindOnce(
        [](EchoedType* value_holder, WaitableEvent* event,
           const EchoedType& value) {
          *value_holder = value;
          event->Signal();
        },
        output, event);
  }

  void RunEcho(const WStringEmbeddedNulls& input,
               mojom::TestWStringEmbeddedNulls::EchoCallback callback) {
    (*remote_)->Echo(input, std::move(callback));
  }

  std::unique_ptr<mojo::Remote<mojom::TestWStringEmbeddedNulls>> remote_;
};

scoped_refptr<SandboxChildProcess> InitChildProcess() {
  auto mojo_task_runner = MojoTaskRunner::Create();
  auto child_process =
      base::MakeRefCounted<SandboxChildProcess>(mojo_task_runner);
  auto message_pipe_handle = child_process->CreateMessagePipeFromCommandLine();

  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  mojo_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&SandboxChildProcess::BindToPipe, child_process,
                                std::move(message_pipe_handle), &event));
  event.Wait();

  return child_process;
}

MULTIPROCESS_TEST_MAIN(EchoMain) {
  scoped_refptr<SandboxChildProcess> child_process = InitChildProcess();

  // Empty string.
  EXPECT_TRUE(child_process->SuccessfulEcho(WStringEmbeddedNulls()));
  EXPECT_TRUE(child_process->SuccessfulEcho(WStringEmbeddedNulls(nullptr)));
  EXPECT_TRUE(child_process->SuccessfulEcho(WStringEmbeddedNulls(nullptr, 0)));
  EXPECT_TRUE(child_process->SuccessfulEcho(WStringEmbeddedNulls(nullptr, 1)));
  EXPECT_TRUE(child_process->SuccessfulEcho(WStringEmbeddedNulls(L"", 0)));
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(std::vector<wchar_t>{})));
  EXPECT_TRUE(
      child_process->SuccessfulEcho(WStringEmbeddedNulls(std::wstring())));
  EXPECT_TRUE(
      child_process->SuccessfulEcho(WStringEmbeddedNulls(std::wstring(L""))));
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(base::WStringPiece())));
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(base::WStringPiece(L""))));

  // Null-terminated strings. Zeroes will be replaced with null characters.
  constexpr wchar_t kStringWithNulls[] = L"string0with0nulls";
  const std::vector<wchar_t> vec1 = CreateVectorWithNulls(kStringWithNulls);
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(vec1.data(), vec1.size())));
  EXPECT_TRUE(child_process->SuccessfulEcho(WStringEmbeddedNulls(vec1)));
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(std::wstring(vec1.data(), vec1.size()))));
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(base::WStringPiece(vec1.data(), vec1.size()))));

  // Non null-terminated strings.
  const std::vector<wchar_t> vec2(vec1.begin(), vec1.end() - 1);
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(vec2.data(), vec2.size())));
  EXPECT_TRUE(child_process->SuccessfulEcho(WStringEmbeddedNulls(vec2)));
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(std::wstring(vec2.data(), vec2.size()))));
  EXPECT_TRUE(child_process->SuccessfulEcho(
      WStringEmbeddedNulls(base::WStringPiece(vec2.data(), vec2.size()))));

  return ::testing::Test::HasNonfatalFailure();
}

class WStringEmbeddedNullsTypemapTest : public ::testing::Test {
 public:
  void SetUp() override {
    mojo_task_runner_ = MojoTaskRunner::Create();
    parent_process_ =
        base::MakeRefCounted<SandboxParentProcess>(mojo_task_runner_);
  }

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  scoped_refptr<SandboxParentProcess> parent_process_;
};

TEST_F(WStringEmbeddedNullsTypemapTest, Echo) {
  int32_t exit_code = -1;
  EXPECT_TRUE(
      parent_process_->LaunchConnectedChildProcess("EchoMain", &exit_code));
  EXPECT_EQ(0, exit_code);
}

}  // namespace

}  // namespace chrome_cleaner
