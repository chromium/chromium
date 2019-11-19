// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ui/chrome_proxy_main_dialog.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/chrome_cleaner/ipc/mock_chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/ui/mock_main_dialog_delegate.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::SizeIs;
using ::testing::StrictMock;

class ChromeProxyMainDialogTest : public ::testing::Test {
 public:
  void SetUp() override {
    // StrictMock will ensure that the cleaner will not send an IPC to Chrome
    // when the functions tested by this fixture get called.
    dialog_ = std::make_unique<ChromeProxyMainDialog>(&delegate_,
                                                      &chrome_prompt_ipc_);
  }

  StrictMock<MockChromePromptIPC> chrome_prompt_ipc_;
  StrictMock<MockMainDialogDelegate> delegate_;
  std::unique_ptr<ChromeProxyMainDialog> dialog_;
};

TEST_F(ChromeProxyMainDialogTest, Create) {
  dialog_->Create();
}

TEST_F(ChromeProxyMainDialogTest, NoPUPsFound) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  base::RunLoop run_loop;
  EXPECT_CALL(delegate_, OnClose())
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(chrome_prompt_ipc_, MockPostPromptUserTask(_, _, _, _))
      .WillOnce(Invoke([](const std::vector<base::FilePath>& files_to_delete,
                          const std::vector<base::string16>& registry_keys,
                          const std::vector<base::string16>& extension_ids,
                          ChromePromptIPC::PromptUserCallback* callback) {
        std::move(*callback).Run(PromptUserResponse::DENIED);
      }));

  dialog_->NoPUPsFound();
  run_loop.Run();
}

TEST_F(ChromeProxyMainDialogTest, CleanupDone) {
  EXPECT_CALL(delegate_, OnClose()).Times(1);
  dialog_->CleanupDone(RESULT_CODE_SUCCESS);
}

TEST_F(ChromeProxyMainDialogTest, Close) {
  EXPECT_CALL(delegate_, OnClose()).Times(1);
  dialog_->Close();
}

class ConfirmCleanupChromeProxyMainDialogTest
    : public ::testing::TestWithParam<PromptUserResponse::PromptAcceptance> {
 public:
  void SetUp() override { Settings::SetInstanceForTesting(&mock_settings_); }

  void TearDown() override { Settings::SetInstanceForTesting(nullptr); }

  StrictMock<MockSettings> mock_settings_;
};

TEST_P(ConfirmCleanupChromeProxyMainDialogTest, ConfirmCleanup) {
  constexpr UwSId kFakePupId = 1024;
  PromptUserResponse::PromptAcceptance prompt_acceptance = GetParam();
  bool accept_cleanup =
      prompt_acceptance == PromptUserResponse::ACCEPTED_WITH_LOGS ||
      prompt_acceptance == PromptUserResponse::ACCEPTED_WITHOUT_LOGS;
  bool logs_allowed =
      prompt_acceptance == PromptUserResponse::ACCEPTED_WITH_LOGS;

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  EXPECT_CALL(mock_settings_,
              set_logs_allowed_in_cleanup_mode(Eq(logs_allowed)))
      .Times(1);

  // Add a PUP and some disk footprints. Footprints should be passed along via
  // the IPC.
  TestPUPData test_pup_data;
  test_pup_data.AddPUP(kFakePupId, PUPData::FLAGS_ACTION_REMOVE, "",
                       PUPData::kMaxFilesToRemoveSmallUwS);
  PUPData::PUP* pup = PUPData::GetPUP(kFakePupId);
  EXPECT_TRUE(pup->AddDiskFootprint(
      base::FilePath(FILE_PATH_LITERAL("c:\\file1.exe"))));
  EXPECT_TRUE(pup->AddDiskFootprint(
      base::FilePath(FILE_PATH_LITERAL("c:\\file2.exe"))));
  EXPECT_TRUE(pup->AddDiskFootprint(
      base::FilePath(FILE_PATH_LITERAL("c:\\file3.txt"))));

  // This file is recognized by the DigestVerifier and should not be sent.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath known_file = temp_dir.GetPath().Append(L"known_file.exe");
  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      known_file.DirName(), known_file.BaseName().value()));
  auto digest_verifier =
      chrome_cleaner::DigestVerifier::CreateFromFile(known_file);
  EXPECT_TRUE(pup->AddDiskFootprint(known_file));

  pup->expanded_registry_footprints.push_back(PUPData::RegistryFootprint(
      RegKeyPath(HKEY_USERS, L"Software\\bad-software\\bad-key"),
      base::string16(), base::string16(), REGISTRY_VALUE_MATCH_KEY));

  std::vector<UwSId> found_pups{kFakePupId};

  StrictMock<MockChromePromptIPC> chrome_prompt_ipc;
  EXPECT_CALL(chrome_prompt_ipc,
              MockPostPromptUserTask(/*files_to_delete*/ SizeIs(3),
                                     /*registry_keys*/ SizeIs(1),
                                     /*extensions*/ SizeIs(0), _))
      .WillOnce(Invoke([prompt_acceptance](
                           const std::vector<base::FilePath>& files_to_delete,
                           const std::vector<base::string16>& registry_keys,
                           const std::vector<base::string16>& extension_ids,
                           ChromePromptIPC::PromptUserCallback* callback) {
        std::move(*callback).Run(prompt_acceptance);
      }));

  StrictMock<MockMainDialogDelegate> delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, AcceptedCleanup(Eq(accept_cleanup)))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  ChromeProxyMainDialog dialog(&delegate, &chrome_prompt_ipc);
  dialog.ConfirmCleanupIfNeeded(found_pups, digest_verifier);
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ConfirmCleanupChromeProxyMainDialogTest,
    testing::Values(PromptUserResponse::ACCEPTED_WITH_LOGS,
                    PromptUserResponse::ACCEPTED_WITHOUT_LOGS,
                    PromptUserResponse::DENIED));

}  // namespace
}  // namespace chrome_cleaner
