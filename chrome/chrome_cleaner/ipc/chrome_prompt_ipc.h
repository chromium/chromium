// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_IPC_H_
#define CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_IPC_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"

namespace chrome_cleaner {

// Defines the interface necessary to connect the "ui" and Chrome. The choice of
// IPC mechanism is left to the child classes.
class ChromePromptIPC {
 public:
  ChromePromptIPC();

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

  // If legacy Mojo IPC is in use this callback will be invoked by
  // mojom::ChromePrompt::PromptUserCallback. Otherwise it will be invoked when
  // a PromptUserResponse proto is received.
  using PromptUserCallback =
      base::OnceCallback<void(PromptUserResponse::PromptAcceptance)>;

  // If legacy Mojo IPC is in use this callback will be invoked by
  // mojom::ChromePrompt::PromptUserCallback. Otherwise it is unused since
  // DisableExtensions is unimplemented.
  using DisableExtensionsCallback = base::OnceCallback<void(bool)>;

  // Sets |error_handler| as the connection error handler and completes whatever
  // initialization that needs to be done separately from construction. This
  // object doesn't own the error handler pointer.
  virtual void Initialize(ErrorHandler* error_handler) = 0;

  // Posts a PromptUser() task to the IPC controller's thread. Internal state
  // must be State:kWaitingForScanResults when the posted task runs. Once the
  // response from Chrome is received, |callback| will run on the IPC
  // controller's thread; clients of this class are responsible for posting
  // response on the right thread.
  virtual void PostPromptUserTask(
      const std::vector<base::FilePath>& files_to_delete,
      const std::vector<base::string16>& registry_keys,
      const std::vector<base::string16>& extension_ids,
      PromptUserCallback callback) = 0;

  // Posts a PromptDisableExtensions() task to the IPC controller's thread.
  // Internal state must be State::kDoneInteraction when the posted task runs.
  virtual void PostDisableExtensionsTask(
      const std::vector<base::string16>& extension_ids,
      DisableExtensionsCallback callback) = 0;

  // Calls |delete_allowed_callback| if the IPC version supports deleting
  // extensions, |delete_not_allowed_callback| otherwise.
  virtual void TryDeleteExtensions(
      base::OnceClosure delete_allowed_callback,
      base::OnceClosure delete_not_allowed_callback) = 0;

 protected:
  virtual ~ChromePromptIPC();

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

  State state_ = State::kUninitialized;
  ErrorHandler* error_handler_ = nullptr;

  // Ensures that all accesses to state_ after initialization are done on the
  // same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_IPC_H_
