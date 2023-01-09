// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ui/chrome_proxy_main_dialog.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/settings/settings.h"

namespace chrome_cleaner {

ChromeProxyMainDialog::ChromeProxyMainDialog(MainDialogDelegate* delegate,
                                             ChromePromptIPC* chrome_prompt_ipc)
    : MainDialogAPI(delegate), chrome_prompt_ipc_(chrome_prompt_ipc) {
  DCHECK(delegate);
  DCHECK(chrome_prompt_ipc_);
}

ChromeProxyMainDialog::~ChromeProxyMainDialog() {}

bool ChromeProxyMainDialog::Create() {
  return true;
}

void ChromeProxyMainDialog::NoPUPsFound() {
  chrome_prompt_ipc_->PostPromptUserTask(
      std::vector<base::FilePath>(), std::vector<std::wstring>(),
      std::vector<std::wstring>(),
      base::BindOnce(
          &ChromeProxyMainDialog::PostCloseAfterReceivingResponseTask,
          base::Unretained(this),
          base::SequencedTaskRunner::GetCurrentDefault()));
}

void ChromeProxyMainDialog::ConfirmCleanup(
    const std::vector<UwSId>& found_pups,
    const FilePathSet& files,
    const std::vector<std::wstring>& registry_keys) {
  std::vector<base::FilePath> files_out = files.ToVector();
  std::vector<std::wstring> registry_keys_out = registry_keys;
  // TODO(crbug.com/981388): Remove the extension_ids field from the IPC.
  chrome_prompt_ipc_->PostPromptUserTask(
      std::move(files_out), std::move(registry_keys_out),
      /*extension_ids=*/{},
      base::BindOnce(&ChromeProxyMainDialog::PostPromptResultReceivedTask,
                     base::Unretained(this),
                     base::SequencedTaskRunner::GetCurrentDefault()));
}

void ChromeProxyMainDialog::CleanupDone(ResultCode cleanup_result) {
  delegate()->OnClose();
}

void ChromeProxyMainDialog::Close() {
  delegate()->OnClose();
}

void ChromeProxyMainDialog::PostPromptResultReceivedTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    PromptUserResponse::PromptAcceptance prompt_acceptance) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ChromeProxyMainDialog::PromptResultReceived,
                                base::Unretained(this), prompt_acceptance));
}

void ChromeProxyMainDialog::PromptResultReceived(
    PromptUserResponse::PromptAcceptance prompt_acceptance) {
  Settings::GetInstance()->set_logs_allowed_in_cleanup_mode(
      prompt_acceptance == PromptUserResponse::ACCEPTED_WITH_LOGS);
  delegate()->AcceptedCleanup(
      prompt_acceptance == PromptUserResponse::ACCEPTED_WITH_LOGS ||
      prompt_acceptance == PromptUserResponse::ACCEPTED_WITHOUT_LOGS);
}

void ChromeProxyMainDialog::PostCloseAfterReceivingResponseTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    PromptUserResponse::PromptAcceptance prompt_acceptance) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeProxyMainDialog::CloseAfterReceivingResponse,
                     base::Unretained(this), prompt_acceptance));
}

void ChromeProxyMainDialog::CloseAfterReceivingResponse(
    PromptUserResponse::PromptAcceptance /*prompt_acceptance*/) {
  delegate()->OnClose();
}

}  // namespace chrome_cleaner
