// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_manager.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

namespace {

base::Value CreateLogValue(const WebAppCommand& command,
                           absl::optional<CommandResult> result) {
  base::Value::Dict dict;
  dict.Set("id", command.id());
  dict.Set("started", command.IsStarted());
  dict.Set("value", command.ToDebugValue());
  if (result) {
    switch (result.value()) {
      case CommandResult::kSuccess:
        dict.Set("result", "kSuccess");
        break;
      case CommandResult::kFailure:
        dict.Set("result", "kFailure");
        break;
      case CommandResult::kShutdown:
        dict.Set("result", "kShutdown");
        break;
    }
  }
  return base::Value(std::move(dict));
}

}  // namespace

WebAppCommandManager::WebAppCommandManager(Profile* profile)
    : profile_(profile),
      url_loader_(std::make_unique<WebAppUrlLoader>()),
      lock_manager_(std::make_unique<WebAppLockManager>()) {}
WebAppCommandManager::~WebAppCommandManager() {
  // Make sure that unittests & browsertests correctly shut down the manager.
  // This ensures that all tests also cover shutdown.
  DCHECK(is_in_shutdown_);
}

void WebAppCommandManager::ScheduleCommand(
    std::unique_ptr<WebAppCommand> command) {
  DCHECK(command);
  if (is_in_shutdown_) {
    AddValueToLog(CreateLogValue(*command, CommandResult::kShutdown));
    return;
  }
  DCHECK(!base::Contains(commands_, command->id()));
  auto command_id = command->id();
  auto command_it = commands_.try_emplace(command_id, std::move(command)).first;
  lock_manager_->AcquireLock(
      command_it->second->lock(),
      base::BindOnce(&WebAppCommandManager::OnLockAcquired,
                     weak_ptr_factory_.GetWeakPtr(), command_id));
}

void WebAppCommandManager::OnLockAcquired(WebAppCommand::Id command_id) {
  if (is_in_shutdown_)
    return;
  auto command_it = commands_.find(command_id);
  DCHECK(command_it != commands_.end());
  // Start is called in a new task to avoid re-entry issues with started tasks
  // calling back into Enqueue/Destroy. This can especially be an issue if
  // this task is being run in response to a call to
  // NotifySyncSourceRemoved.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebAppCommandManager::StartCommandOrPrepareForLoad,
                     weak_ptr_factory_.GetWeakPtr(), command_it->second.get()));
}

void WebAppCommandManager::StartCommandOrPrepareForLoad(
    WebAppCommand* command) {
  if (is_in_shutdown_)
    return;
#if DCHECK_IS_ON()
  DCHECK(command);
  auto command_it = commands_.find(command->id());
  DCHECK(command_it != commands_.end());
  DCHECK(!command->IsStarted());
#endif
  if (command->lock().IncludesSharedWebContents()) {
    command->shared_web_contents_ = EnsureWebContentsCreated();
    url_loader_->PrepareForLoad(
        command->shared_web_contents(),
        base::BindOnce(&WebAppCommandManager::OnAboutBlankLoadedForCommandStart,
                       weak_ptr_factory_.GetWeakPtr(), command));
    return;
  }
  command->Start(this);
}

void WebAppCommandManager::OnAboutBlankLoadedForCommandStart(
    WebAppCommand* command,
    WebAppUrlLoader::Result result) {
  if (!shared_web_contents_) {
    DCHECK(is_in_shutdown_);
    return;
  }

  // about:blank must always be loaded.
  DCHECK_EQ(WebAppUrlLoader::Result::kUrlLoaded, result);
  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    base::Value url_loader_error(base::Value::Type::DICTIONARY);
    url_loader_error.SetStringKey("WebAppUrlLoader::Result",
                                  ConvertUrlLoaderResultToString(result));
    if (command->lock().app_ids().size() == 1) {
      url_loader_error.SetStringKey("task.app_id_to_expect",
                                    *command->lock().app_ids().begin());
    }
    url_loader_error.SetStringKey("!stage", "OnWebContentsReady");
    install_manager_->TakeCommandErrorLog(PassKey(),
                                          std::move(url_loader_error));
  }

  command->Start(this);
}

void WebAppCommandManager::Shutdown() {
  // Ignore duplicate shutdowns for unittests.
  if (is_in_shutdown_)
    return;
  is_in_shutdown_ = true;
  shared_web_contents_.reset();
  AddValueToLog(base::Value("Shutdown has begun"));

  // Create a copy of commands to call `OnShutdown` because commands can call
  // `CallSignalCompletionAndSelfDestruct` during `OnShutdown`, which removes
  // the command from the `commands_` map.
  std::vector<base::WeakPtr<WebAppCommand>> commands_to_shutdown;
  for (const auto& [id, command] : commands_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(command->command_sequence_checker_);
    if (command->IsStarted()) {
      commands_to_shutdown.push_back(command->AsWeakPtr());
    }
  }
  for (const auto& command_ptr : commands_to_shutdown) {
    if (!command_ptr)
      continue;
    command_ptr->OnShutdown();
  }
  commands_.clear();
}

void WebAppCommandManager::NotifySyncSourceRemoved(
    const std::vector<AppId>& app_ids) {
  if (is_in_shutdown_)
    return;

  // To prevent map modification-during-iteration, make a copy of relevant
  // commands. The main complications that can occur are a command calling
  // `CompleteAndDestruct` or `ScheduleCommand` inside of the
  // `OnSyncSourceRemoved` call. Because all commands are
  // `Start()`ed asynchronously, we will never have to notify any commands that
  // are newly scheduled. So at most one command needs to be notified per queue,
  // and that command can be destroyed before we notify it.
  std::vector<base::WeakPtr<WebAppCommand>> commands_to_notify;
  for (const AppId& app_id : app_ids) {
    for (const auto& [id, command] : commands_) {
      if (base::Contains(command->lock().app_ids(), app_id)) {
        if (command->IsStarted()) {
          commands_to_notify.push_back(command->AsWeakPtr());
        }
      }
    }
  }
  for (const auto& command_ptr : commands_to_notify) {
    if (!command_ptr)
      continue;
    command_ptr->OnSyncSourceRemoved();
  }
}

base::Value WebAppCommandManager::ToDebugValue() {
  base::Value::List command_log;
  for (auto& command_value : command_debug_log_) {
    command_log.Append(std ::move(command_value));
  }

  base::Value::List queued;
  for (const auto& [id, command] : commands_) {
    queued.Append(::web_app::CreateLogValue(*command, absl::nullopt));
  }

  base::Value::Dict state;
  state.Set("command_log", std::move(command_log));
  state.Set("command_queue", base::Value(std::move(queued)));
  return base::Value(std::move(state));
}

void WebAppCommandManager::SetSubsystems(
    WebAppInstallManager* install_manager) {
  install_manager_ = install_manager;
}

void WebAppCommandManager::LogToInstallManager(base::Value log) {
  install_manager_->TakeCommandErrorLog(PassKey(), std::move(log));
}

bool WebAppCommandManager::IsInstallingForWebContents(
    const content::WebContents* web_contents) const {
  for (const auto& [id, command] : commands_) {
    if (command->GetInstallingWebContents() == web_contents) {
      return true;
    }
  }
  return false;
}

void WebAppCommandManager::AwaitAllCommandsCompleteForTesting() {
  if (commands_.empty())
    return;

  if (!run_loop_for_testing_)
    run_loop_for_testing_ = std::make_unique<base::RunLoop>();
  run_loop_for_testing_->Run();
  run_loop_for_testing_.reset();
}

void WebAppCommandManager::SetUrlLoaderForTesting(
    std::unique_ptr<WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

void WebAppCommandManager::OnCommandComplete(
    WebAppCommand* running_command,
    CommandResult result,
    base::OnceClosure completion_callback) {
  DCHECK(running_command);
  AddValueToLog(CreateLogValue(*running_command, result));

  auto command_it = commands_.find(running_command->id());
  DCHECK(command_it != commands_.end());
  commands_.erase(command_it);

  if (shared_web_contents_) {
    bool lock_free = lock_manager_->IsSharedWebContentsLockFree();
    if (lock_free) {
      AddValueToLog(base::Value("Destroying the shared web contents."));
      shared_web_contents_.reset();
    }
  }

  std::move(completion_callback).Run();

  if (commands_.empty() && run_loop_for_testing_)
    run_loop_for_testing_->Quit();
}

void WebAppCommandManager::AddValueToLog(base::Value value) {
  static constexpr const int kMaxLogLength = 20;
  command_debug_log_.push_front(std::move(value));
  if (command_debug_log_.size() > kMaxLogLength)
    command_debug_log_.resize(kMaxLogLength);
}

content::WebContents* WebAppCommandManager::EnsureWebContentsCreated() {
  DCHECK(profile_);
  if (!shared_web_contents_)
    shared_web_contents_ = WebAppInstallTask::CreateWebContents(profile_);
  return shared_web_contents_.get();
}

}  // namespace web_app
