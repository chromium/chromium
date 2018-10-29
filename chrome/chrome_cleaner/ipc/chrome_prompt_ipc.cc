// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

ChromePromptIPC::ChromePromptIPC(const std::string& chrome_mojo_pipe_token,
                                 scoped_refptr<MojoTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      chrome_mojo_pipe_token_(chrome_mojo_pipe_token) {
  // Accesses to |state_| must happen on |task_runner_|'s sequence, which is
  // not the construction sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ChromePromptIPC::Initialize(ErrorHandler* error_handler) {
  DCHECK(!chrome_mojo_pipe_token_.empty());
  DCHECK(task_runner_);

  error_handler_ = error_handler;

  // No need to retain this object, since it will live until the process
  // finishes.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ChromePromptIPC::InitializeChromePromptPtr,
                                base::Unretained(this)));
}

ChromePromptIPC::~ChromePromptIPC() {
  // This object is only destroyed when the cleaner process ends. At this point
  // we don't need to close the IPC, since the broken connection message will
  // be received by Chrome anyway, and we can simply leak the pointer.
  chrome_prompt_service_.release();  // Leaked.
}

void ChromePromptIPC::PostPromptUserTask(
    const std::vector<base::FilePath>& files_to_delete,
    const std::vector<base::string16>& registry_keys,
    mojom::ChromePrompt::PromptUserCallback callback) {
  DCHECK(task_runner_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromePromptIPC::RunPromptUserTask, base::Unretained(this),
          files_to_delete, registry_keys,
          base::BindOnce(&ChromePromptIPC::OnChromeResponseReceived,
                         base::Unretained(this), std::move(callback))));
}

void ChromePromptIPC::PostDisableExtensionsTask(
    const std::vector<base::string16>& extension_ids,
    mojom::ChromePrompt::DisableExtensionsCallback callback) {
  DCHECK(task_runner_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromePromptIPC::RunDisableExtensionsTask, base::Unretained(this),
          extension_ids,
          base::BindOnce(&ChromePromptIPC::OnChromeResponseReceivedExtensions,
                         base::Unretained(this), std::move(callback))));
}

void ChromePromptIPC::OnChromeResponseReceived(
    mojom::ChromePrompt::PromptUserCallback callback,
    mojom::PromptAcceptance prompt_acceptance) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kWaitingForResponseFromChrome, state_);

  state_ = State::kDoneInteraction;
  std::move(callback).Run(prompt_acceptance);
}

void ChromePromptIPC::OnChromeResponseReceivedExtensions(
    mojom::ChromePrompt::DisableExtensionsCallback callback,
    bool extensions_disabled_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kDoneInteraction, state_);

  std::move(callback).Run(extensions_disabled_callback);
}

void ChromePromptIPC::OnConnectionError() {
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

void ChromePromptIPC::InitializeChromePromptPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kUninitialized, state_);

  auto channel_endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess());
  auto incoming_invitation =
      mojo::IncomingInvitation::Accept(std::move(channel_endpoint));

  mojo::ScopedMessagePipeHandle message_pipe_handle =
      incoming_invitation.ExtractMessagePipe(chrome_mojo_pipe_token_);

  chrome_prompt_service_.reset(new chrome_cleaner::mojom::ChromePromptPtr);
  chrome_prompt_service_->Bind(chrome_cleaner::mojom::ChromePromptPtrInfo(
      std::move(message_pipe_handle), 0));
  // No need to retain this object, since it will live until the process
  // finishes.
  chrome_prompt_service_->set_connection_error_handler(base::BindOnce(
      &ChromePromptIPC::OnConnectionError, base::Unretained(this)));
  state_ = State::kWaitingForScanResults;
}

void ChromePromptIPC::RunPromptUserTask(
    const std::vector<base::FilePath>& files_to_delete,
    const std::vector<base::string16>& registry_keys,
    mojom::ChromePrompt::PromptUserCallback callback) {
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

  (*chrome_prompt_service_)
      ->PromptUser(std::move(files_to_delete), std::move(registry_keys),
                   base::nullopt, std::move(callback));
}

void ChromePromptIPC::RunDisableExtensionsTask(
    const std::vector<base::string16>& extension_ids,
    mojom::ChromePrompt::DisableExtensionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(chrome_prompt_service_);
  DCHECK(state_ == State::kDoneInteraction);

  (*chrome_prompt_service_)
      ->DisableExtensions(std::move(extension_ids), std::move(callback));
}

}  // namespace chrome_cleaner
