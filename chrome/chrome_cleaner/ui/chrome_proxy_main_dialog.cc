// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ui/chrome_proxy_main_dialog.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
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
      std::vector<base::FilePath>(), std::vector<base::string16>(),
      std::vector<base::string16>(),
      base::BindOnce(
          &ChromeProxyMainDialog::PostCloseAfterReceivingResponseTask,
          base::Unretained(this), base::SequencedTaskRunnerHandle::Get()));
}

void ChromeProxyMainDialog::ConfirmCleanup(
    const std::vector<UwSId>& found_pups,
    const FilePathSet& files,
    const std::vector<base::string16>& registry_keys) {
  std::vector<base::FilePath> files_out = files.ToVector();
  std::vector<base::string16> registry_keys_out = registry_keys;
  std::vector<base::string16> extension_ids;
  for (const UwSId& pup_id : found_pups) {
    if (!PUPData::IsKnownPUP(pup_id)) {
      continue;
    }
    PUPData::PUP* pup = PUPData::GetPUP(pup_id);
    for (const ForceInstalledExtension& matched_extension :
         pup->matched_extensions) {
      extension_ids.push_back(
          base::UTF8ToUTF16(matched_extension.id.AsString()));
    }
  }
  chrome_prompt_ipc_->PostPromptUserTask(
      std::move(files_out), std::move(registry_keys_out),
      std::move(extension_ids),
      base::BindOnce(&ChromeProxyMainDialog::PostPromptResultReceivedTask,
                     base::Unretained(this),
                     base::SequencedTaskRunnerHandle::Get()));
}

void ChromeProxyMainDialog::CleanupDone(ResultCode cleanup_result) {
  delegate()->OnClose();
}

void ChromeProxyMainDialog::Close() {
  delegate()->OnClose();
}

void ChromeProxyMainDialog::DisableExtensions(
    const std::vector<base::string16>& extensions,
    base::OnceCallback<void(bool)> on_disable) {
  chrome_prompt_ipc_->PostDisableExtensionsTask(extensions,
                                                std::move(on_disable));
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
