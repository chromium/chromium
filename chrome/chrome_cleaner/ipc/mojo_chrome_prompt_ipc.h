// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_CHROME_CLEANER_IPC_MOJO_CHROME_PROMPT_IPC_H_
#define CHROME_CHROME_CLEANER_IPC_MOJO_CHROME_PROMPT_IPC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/chrome_prompt.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chrome_cleaner {

// Simple wrapper to control lifetime of the mojo::Remote<ChromePrompt> object
// and post tasks in the IPC thread kept by the MojoTaskRunner. Once created,
// this object lives until the cleaner process ends.
//
// Sample usage:
//   scoped_refptr<MojoTaskRunner> task_runner = MojoTaskRunner::Create();
//   const std::string chrome_mojo_pipe_token =
//       Settings::GetInstance()->chrome_mojo_pipe_token();
//   ChromePromptIPC* chrome_prompt_ipc =
//       new MojoChromePromptIPC(chrome_mojo_pipe_token, task_runner);
//   ChromePromptIPC::ErrorHandler error_handler = ...;
//   chrome_prompt_ipc->Initialize(&error_handler);
//   ...
//   std::vector<base::FilePath> files_to_delete = ...;
//   std::vector<std::wstring> registry_keys = ...;
//   chrome_prompt_ipc->PostPromptUserTask(
//       files_to_delete, registry_keys,
//       base::BindOnce(&ReceivePromptResult));
//
//   void ReceivePromptResult(PromptAcceptance prompt_acceptance) {
//     ...
//   }
class MojoChromePromptIPC : public ChromePromptIPC {
 public:
  MojoChromePromptIPC(const std::string& chrome_mojo_pipe_token,
                      scoped_refptr<MojoTaskRunner> task_runner);

  // Initializes |chrome_prompt_service_| in the IPC controller's thread and
  // sets |error_handler| as the connection error handler. This object doesn't
  // own the error handler pointer.
  void Initialize(ChromePromptIPC::ErrorHandler* error_handler) override;

  // Posts a PromptUser() task to the IPC controller's thread. Internal state
  // must be State:kWaitingForScanResults when the posted task runs. Once the
  // response from Chrome is received, |callback| will run on the IPC
  // controller's thread; clients of this class are responsible for posting
  // response on the right thread.
  void PostPromptUserTask(const std::vector<base::FilePath>& files_to_delete,
                          const std::vector<std::wstring>& registry_keys,
                          const std::vector<std::wstring>& extension_ids,
                          PromptUserCallback callback) override;

 protected:
  // The destructor is only called by tests for doubles of this class. In the
  // cleaner, this object leaks, so we don't bother closing the connection
  // (Chrome will receive the signal anyway once the cleaner process ends).
  ~MojoChromePromptIPC() override;

  // Initializes |chrome_prompt_service_| and sets the connection error
  // handler. This must be executed in the IPC controller's thread.
  void InitializeChromePromptPtr();

  // Runs |chrome_prompt_service_->PromptUser()|. Must be called on the IPC
  // thread.
  void RunPromptUserTask(const std::vector<base::FilePath>& files_to_delete,
                         const std::vector<std::wstring>& registry_keys,
                         const std::vector<std::wstring>& extension_ids,
                         PromptUserCallback callback);

  // Callback for mojom::ChromePrompt::PromptUser, internal state must be
  // State::kWaitingForResponseFromChrome. Invokes callback(prompt_acceptance)
  // and transitions to state State::kDoneInteraction.
  void OnChromeResponseReceived(PromptUserCallback callback,
                                mojom::PromptAcceptance prompt_acceptance);

  // Connection error handler. Invokes either
  // error_handler_->OnConnectionClosed() or
  // error_handler_->OnConnectionClosedAfterDone(), depending on the internal
  // state.
  void OnConnectionError();

  void PromptUserCheckVersion(
      const std::vector<base::FilePath>& files_to_delete,
      const std::vector<std::wstring>& registry_keys,
      const std::vector<std::wstring>& extension_ids,
      mojom::ChromePrompt::PromptUserCallback callback,
      uint32_t version);

 private:
  scoped_refptr<MojoTaskRunner> task_runner_;
  std::string chrome_mojo_pipe_token_;
  std::unique_ptr<mojo::Remote<mojom::ChromePrompt>> chrome_prompt_service_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_MOJO_CHROME_PROMPT_IPC_H_
