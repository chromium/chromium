// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_UI_CHROME_PROXY_MAIN_DIALOG_H_
#define CHROME_CHROME_CLEANER_UI_CHROME_PROXY_MAIN_DIALOG_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ui/main_dialog_api.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"

namespace chrome_cleaner {

// Implementation of MainDialogAPI that communicates with Chrome over IPC. The
// user interacts with the tool via Chrome UI.
class ChromeProxyMainDialog : public MainDialogAPI {
 public:
  // The given delegate must outlive the ChromeProxyMainDialog.
  ChromeProxyMainDialog(MainDialogDelegate* delegate,
                        ChromePromptIPC* chrome_prompt_ipc);
  ~ChromeProxyMainDialog() override;

  // MainDialogAPI overrides.
  bool Create() override;
  void NoPUPsFound() override;
  void CleanupDone(ResultCode cleanup_result) override;
  void Close() override;
  void DisableExtensions(const std::vector<base::string16>& extensions,
                         base::OnceCallback<void(bool)> on_disable) override;

 protected:
  void ConfirmCleanup(
      const std::vector<UwSId>& found_pups,
      const FilePathSet& files_to_remove,
      const std::vector<base::string16>& registry_keys) override;

 private:
  // Callback for the Mojo IPC that posts PromptResultReceived() on the UI
  // thread.
  void PostPromptResultReceivedTask(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      PromptUserResponse::PromptAcceptance prompt_acceptance);

  // Handles the prompt acceptance result received from Chrome. This should
  // only be called by PostPromptResultReceivedTask(), that will handle posting
  // it to the right thread.
  void PromptResultReceived(
      PromptUserResponse::PromptAcceptance prompt_acceptance);

  // Callback for the Mojo IPC that posts CloseAfterReceivingResponse() on the
  // UI thread.
  void PostCloseAfterReceivingResponseTask(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      PromptUserResponse::PromptAcceptance prompt_acceptance);

  // Closes the dialog after receiving a response from Chrome when no UwS is
  // found in the system.
  void CloseAfterReceivingResponse(
      PromptUserResponse::PromptAcceptance prompt_acceptance);

  // Pointer to the wrapper for the Mojo IPC to send scan results to Chrome.
  ChromePromptIPC* chrome_prompt_ipc_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_UI_CHROME_PROXY_MAIN_DIALOG_H_
