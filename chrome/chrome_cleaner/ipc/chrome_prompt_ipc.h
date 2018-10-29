// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_IPC_H_
#define CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_IPC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "base/threading/thread.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"

namespace chrome_cleaner {

// Simple wrapper to control lifetime of the ChromePromptPtr object and post
// tasks in the IPC thread kept by the MojoTaskRunner. Once created, this
// object lives until the cleaner process ends.
//
// Simple usage:
//   scoped_refptr<MojoTaskRunner> task_runner = MojoTaskRunner::Create();
//   const std::string chrome_mojo_pipe_token =
//       Settings::GetInstance()->chrome_mojo_pipe_token();
//   ChromePromptIPC* chrome_prompt_ipc =
//       new ChromePromptIPC(chrome_mojo_pipe_token, task_runner);
//   ChromePromptIPC::ErrorHandler error_handler = ...;
//   chrome_prompt_ipc->Initialize(&error_handler);
//   ...
//   std::vector<base::FilePath> files_to_delete = ...;
//   std::vector<base::string16> registry_keys = ...;
//   chrome_prompt_ipc->PostPromptUserTask(
//       files_to_delete, registry_keys,
//       base::BindOnce(&ReceivePromptResult));
//
//   void ReceivePromptResult(mojom::PromptAcceptance prompt_acceptance) {
//     ...
//   }
class ChromePromptIPC {
 public:
  // Interface for connection error handling, which can change depending on the
  // context objects of this class are used, such as in the main cleaner or in
  // tests. Methods of this class will be called on the IPC controller's
  // thread. Clients of this class must ensure operations are posted to the
  // right thread if needed.
  class ErrorHandler {
   public:
    virtual ~ErrorHandler() = default;

    // Invoked if the pipe connection is closed while the communication channel
    // is still required (before receiving response from the parent process).
    virtual void OnConnectionClosed() = 0;

    // Invoked if the pipe connection is closed once the communication channel
    // is no longer required.
    virtual void OnConnectionClosedAfterDone() = 0;
  };

  ChromePromptIPC(const std::string& chrome_mojo_pipe_token,
                  scoped_refptr<MojoTaskRunner> task_runner);

  // Initializes |chrome_prompt_service_| in the IPC controller's thread and
  // sets |error_handler| as the connection error handler. This object doesn't
  // own the error handler pointer.
  virtual void Initialize(ErrorHandler* error_handler);

  // Posts a PromptUser() task to the IPC controller's thread. Internal state
  // must be State:kWaitingForScanResults when the posted task runs. Once the
  // response from Chrome is received, |callback| will run on the IPC
  // controller's thread; clients of this class are responsible for posting
  // response on the right thread.
  virtual void PostPromptUserTask(
      const std::vector<base::FilePath>& files_to_delete,
      const std::vector<base::string16>& registry_keys,
      mojom::ChromePrompt::PromptUserCallback callback);

  // Posts a PromptDisableExtensions() task to the IPC controller's thread.
  // Internal state must be State::kDoneInteraction when the posted task runs.
  virtual void PostDisableExtensionsTask(
      const std::vector<base::string16>& extension_ids,
      mojom::ChromePrompt::DisableExtensionsCallback callback);

 protected:
  // The destructor is only called by tests for doubles of this class. In the
  // cleaner, this object leaks, so we don't bother closing the connection
  // (Chrome will receive the signal anyway once the cleaner process ends).
  virtual ~ChromePromptIPC();

 private:
  enum class State {
    // The IPC has not been initialized.
    kUninitialized,
    // Scan results are not available yet.
    kWaitingForScanResults,
    // Scan results sent to Chrome, waiting for the user's response.
    kWaitingForResponseFromChrome,
    // Response from Chrome received. In this state, there will be no further
    // user interaction.
    kDoneInteraction,
  };

  // Initializes |chrome_prompt_service_| and sets the connection error
  // handler. This must be executed in the IPC controller's thread.
  void InitializeChromePromptPtr();

  // Runs |chrome_prompt_service_->PromptUser()|. Must be called on the IPC
  // thread.
  virtual void RunPromptUserTask(
      const std::vector<base::FilePath>& files_to_delete,
      const std::vector<base::string16>& registry_keys,
      mojom::ChromePrompt::PromptUserCallback callback);

  virtual void RunDisableExtensionsTask(
      const std::vector<base::string16>& extension_ids,
      mojom::ChromePrompt::DisableExtensionsCallback callback);

  // Callback for ChromePrompt::PromptUser, internal state must be
  // State::kWaitingForResponseFromChrome. Invokes callback(prompt_acceptance)
  // and transitions to state State::kDoneInteraction.
  void OnChromeResponseReceived(
      mojom::ChromePrompt::PromptUserCallback callback,
      mojom::PromptAcceptance prompt_acceptance);

  // Callback for ChromePrompt::DisableExtensions, internal state must be
  // State::kDoneInteraction. Invokes callback(extensions_deleted_callback).
  void OnChromeResponseReceivedExtensions(
      mojom::ChromePrompt::DisableExtensionsCallback callback,
      bool extensions_deleted_callback);

  // Connection error handler. Invokes either
  // error_handler_->OnConnectionClosed() or
  // error_handler_->OnConnectionClosedAfterDone(), depending on the internal
  // state.
  void OnConnectionError();

  State state_ = State::kUninitialized;
  scoped_refptr<MojoTaskRunner> task_runner_;
  std::string chrome_mojo_pipe_token_;
  std::unique_ptr<mojom::ChromePromptPtr> chrome_prompt_service_;
  ErrorHandler* error_handler_ = nullptr;

  // Ensures that all accesses to state_ after initialization are done on the
  // same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_IPC_H_
