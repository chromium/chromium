// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/mojo_chrome_prompt_ipc.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

MojoChromePromptIPC::MojoChromePromptIPC(
    const std::string& chrome_mojo_pipe_token,
    scoped_refptr<MojoTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      chrome_mojo_pipe_token_(chrome_mojo_pipe_token) {
  // Accesses to |state_| must happen on |task_runner_|'s sequence, which is
  // not the construction sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void MojoChromePromptIPC::Initialize(ErrorHandler* error_handler) {
  DCHECK(!chrome_mojo_pipe_token_.empty());
  DCHECK(task_runner_);

  error_handler_ = error_handler;

  // No need to retain this object, since it will live until the process
  // finishes.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MojoChromePromptIPC::InitializeChromePromptPtr,
                                base::Unretained(this)));
}

MojoChromePromptIPC::~MojoChromePromptIPC() {
  // This object is only destroyed when the cleaner process ends. At this point
  // we don't need to close the IPC, since the broken connection message will
  // be received by Chrome anyway, and we can simply leak the pointer.
  chrome_prompt_service_.release();  // Leaked.
}

void MojoChromePromptIPC::PostPromptUserTask(
    const std::vector<base::FilePath>& files_to_delete,
    const std::vector<std::wstring>& registry_keys,
    const std::vector<std::wstring>& extension_ids,
    PromptUserCallback callback) {
  DCHECK(task_runner_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MojoChromePromptIPC::RunPromptUserTask,
                     base::Unretained(this), files_to_delete, registry_keys,
                     extension_ids, std::move(callback)));
}

void MojoChromePromptIPC::OnChromeResponseReceived(
    PromptUserCallback callback,
    mojom::PromptAcceptance prompt_acceptance) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kWaitingForResponseFromChrome, state_);

  state_ = State::kDoneInteraction;
  std::move(callback).Run(
      static_cast<PromptUserResponse::PromptAcceptance>(prompt_acceptance));
}

void MojoChromePromptIPC::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(State::kUninitialized, state_);

  if (!error_handler_)
    return;

  if (state_ == State::kDoneInteraction) {
    error_handler_->OnConnectionClosedAfterDone();
  } else {
    state_ = State::kDoneInteraction;
    error_handler_->OnConnectionClosed();
  }
}

void MojoChromePromptIPC::InitializeChromePromptPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kUninitialized, state_);

  auto channel_endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess());
  auto incoming_invitation =
      mojo::IncomingInvitation::Accept(std::move(channel_endpoint));

  mojo::ScopedMessagePipeHandle message_pipe_handle =
      incoming_invitation.ExtractMessagePipe(chrome_mojo_pipe_token_);

  mojo::PendingRemote<chrome_cleaner::mojom::ChromePrompt> pending_remote(
      std::move(message_pipe_handle), /*version=*/0);
  chrome_prompt_service_ =
      std::make_unique<mojo::Remote<chrome_cleaner::mojom::ChromePrompt>>(
          std::move(pending_remote));
  // No need to retain this object, since it will live until the process
  // finishes.
  chrome_prompt_service_->set_disconnect_handler(base::BindOnce(
      &MojoChromePromptIPC::OnConnectionError, base::Unretained(this)));
  state_ = State::kWaitingForScanResults;
}

void MojoChromePromptIPC::PromptUserCheckVersion(
    const std::vector<base::FilePath>& files_to_delete,
    const std::vector<std::wstring>& registry_keys,
    const std::vector<std::wstring>& extension_ids,
    mojom::ChromePrompt::PromptUserCallback callback,
    uint32_t version) {
  if (version >= 3) {
    (*chrome_prompt_service_)
        ->PromptUser(std::move(files_to_delete), std::move(registry_keys),
                     std::move(extension_ids), std::move(callback));
  } else {
    // Before version 3 the delete extensions interface wasn't implemented.
    // So we need to not notify the user that there are extensions to be
    // deleted.
    (*chrome_prompt_service_)
        ->PromptUser(std::move(files_to_delete), std::move(registry_keys),
                     absl::nullopt, std::move(callback));
  }
}

void MojoChromePromptIPC::RunPromptUserTask(
    const std::vector<base::FilePath>& files_to_delete,
    const std::vector<std::wstring>& registry_keys,
    const std::vector<std::wstring>& extension_ids,
    PromptUserCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(chrome_prompt_service_);
  DCHECK(state_ != State::kUninitialized);
  DCHECK(state_ != State::kWaitingForResponseFromChrome);

  // This is a corner case, in which we receive the disconnect message on the
  // IPC thread right before this task is posted. In that case, this function
  // will be a no-op.
  if (state_ == State::kDoneInteraction)
    return;

  state_ = State::kWaitingForResponseFromChrome;

  // Mojo will invoke this callback when a response is received. The
  // |prompt_acceptance| parameter is unbound and will be filled in by Mojo.
  mojom::ChromePrompt::PromptUserCallback response_callback =
      base::BindOnce(&MojoChromePromptIPC::OnChromeResponseReceived,
                     base::Unretained(this), std::move(callback));

  (*chrome_prompt_service_)
      .QueryVersion(base::BindOnce(
          &MojoChromePromptIPC::PromptUserCheckVersion, base::Unretained(this),
          std::move(files_to_delete), std::move(registry_keys),
          std::move(extension_ids), std::move(response_callback)));
}

}  // namespace chrome_cleaner
