// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_manager.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

namespace {

base::Value::Dict CreateCommandMetadata(const WebAppCommand& command) {
  base::Value::Dict dict;
  dict.Set("name", command.name());
  dict.Set("id", command.id());
  if (command.scheduled_location().has_value()) {
    dict.Set("scheduled_location",
             command.scheduled_location().value().ToString());
  }
  return dict;
}

base::Value::Dict CreateLogValue(const WebAppCommand& command,
                                 absl::optional<CommandResult> result) {
  base::Value::Dict dict = CreateCommandMetadata(command);
  base::Value debug_value = command.ToDebugValue();
  bool is_empty_dict = debug_value.is_dict() && debug_value.GetDict().empty();
  if (!debug_value.is_none() && !is_empty_dict) {
    dict.Set("value", std::move(debug_value));
  }
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
  return dict;
}

}  // namespace

WebAppCommandManager::WebAppCommandManager(Profile* profile)
    : profile_(profile) {}
WebAppCommandManager::~WebAppCommandManager() {
  // Make sure that unittests & browsertests correctly shut down the manager.
  // This ensures that all tests also cover shutdown.
  DCHECK(is_in_shutdown_);
}

void WebAppCommandManager::SetProvider(base::PassKey<WebAppProvider>,
                                       WebAppProvider& provider) {
  provider_ = &provider;
  lock_manager_.SetProvider(PassKey(), provider);
}

void WebAppCommandManager::Start() {
  started_ = true;
  std::vector<std::unique_ptr<WebAppCommand>> to_schedule;
  std::swap(commands_waiting_for_start_, to_schedule);

  for (auto& command : to_schedule) {
    ScheduleCommand(std::move(command));
  }
}

void WebAppCommandManager::ScheduleCommand(
    std::unique_ptr<WebAppCommand> command,
    const base::Location& location) {
  DCHECK(command);
  command->SetScheduledLocation(location);
  DVLOG(2) << "Scheduling command: " << CreateCommandMetadata(*command);
  if (!started_) {
    commands_waiting_for_start_.push_back(std::move(command));
    return;
  }
  if (is_in_shutdown_) {
    AddValueToLog(
        base::Value(CreateLogValue(*command, CommandResult::kShutdown)));
    return;
  }
  DCHECK(!base::Contains(commands_, command->id()));
  auto command_id = command->id();
  auto command_it = commands_.try_emplace(command_id, std::move(command)).first;
  command_it->second->RequestLock(
      this, &lock_manager_,
      base::BindOnce(&WebAppCommandManager::OnLockAcquired,
                     weak_ptr_factory_.GetWeakPtr(), command_id),
      location);
}

void WebAppCommandManager::OnLockAcquired(WebAppCommand::Id command_id,
                                          base::OnceClosure start_command) {
  if (is_in_shutdown_)
    return;
  auto command_it = commands_.find(command_id);
  DCHECK(command_it != commands_.end());
  // Start is called in a new task to avoid re-entry issues with started tasks
  // calling back into Enqueue/Destroy. This can especially be an issue if
  // this task is being run in response to a call to
  // NotifySyncSourceRemoved.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebAppCommandManager::StartCommand,
                     weak_ptr_factory_.GetWeakPtr(), command_it->second.get(),
                     std::move(start_command)));
}

void WebAppCommandManager::StartCommand(WebAppCommand* command,
                                        base::OnceClosure start_command) {
  if (is_in_shutdown_)
    return;
#if DCHECK_IS_ON()
  DCHECK(command);
  auto command_it = commands_.find(command->id());
  DCHECK(command_it != commands_.end());
#endif
  if (command->lock_description().IncludesSharedWebContents()) {
    CHECK(shared_web_contents_);
  }
  DVLOG(2) << "Starting command: " << CreateCommandMetadata(*command);
  std::move(start_command).Run();
}

void WebAppCommandManager::Shutdown() {
  // Ignore duplicate shutdowns for unittests.
  if (is_in_shutdown_)
    return;
  is_in_shutdown_ = true;
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

  shared_web_contents_.reset();
}

base::Value WebAppCommandManager::ToDebugValue() {
  base::Value::List command_log;
  for (const auto& command_value : command_debug_log_) {
    command_log.Append(command_value.Clone());
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

void WebAppCommandManager::LogToInstallManager(base::Value::Dict log) {
#if DCHECK_IS_ON()
  // This is wrapped with DCHECK_IS_ON() to prevent calling DebugString() in
  // production builds.
  DVLOG(1) << log.DebugString();
#endif
  provider_->install_manager().TakeCommandErrorLog(PassKey(), std::move(log));
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

void WebAppCommandManager::OnCommandComplete(
    WebAppCommand* running_command,
    CommandResult result,
    base::OnceClosure completion_callback) {
  DCHECK(running_command);
  AddValueToLog(base::Value(CreateLogValue(*running_command, result)));

  auto command_it = commands_.find(running_command->id());
  DCHECK(command_it != commands_.end());
  commands_.erase(command_it);

  if (shared_web_contents_) {
    bool lock_free = lock_manager_.IsSharedWebContentsLockFree();
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
  DCHECK(!value.is_none());
#if DCHECK_IS_ON()
  // This is wrapped with DCHECK_IS_ON() to prevent calling DebugString() in
  // production builds.
  DVLOG(1) << value.DebugString();
#endif
  static const size_t kMaxLogLength =
      base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo) ? 1000
                                                                     : 20;
  command_debug_log_.push_front(std::move(value));
  if (command_debug_log_.size() > kMaxLogLength)
    command_debug_log_.resize(kMaxLogLength);
}

content::WebContents* WebAppCommandManager::EnsureWebContentsCreated(
    base::PassKey<WebAppLockManager>) {
  return EnsureWebContentsCreated();
}

content::WebContents* WebAppCommandManager::EnsureWebContentsCreated() {
  DCHECK(profile_);
  if (!shared_web_contents_)
    shared_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
  web_app::CreateWebAppInstallTabHelpers(shared_web_contents_.get());

  return shared_web_contents_.get();
}

}  // namespace web_app
