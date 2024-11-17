// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_manager.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

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
  url_loader_ = provider_->web_contents_manager().CreateUrlLoader();
}

void WebAppCommandManager::Start() {
  started_ = true;
  // Profile manager can be null in unit tests.
  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(profile_manager);
  }
  std::vector<std::pair<std::unique_ptr<internal::CommandBase>, base::Location>>
      to_schedule;
  std::swap(commands_waiting_for_start_, to_schedule);

  for (auto& [command_ptr, location] : to_schedule) {
    ScheduleCommand(std::move(command_ptr), location);
  }
}

void WebAppCommandManager::ScheduleCommand(
    std::unique_ptr<internal::CommandBase> command,
    const base::Location& location) {
  CHECK(command);

  command->SetScheduledLocation(base::PassKey<WebAppCommandManager>(),
                                location);
  command->SetCommandManager(base::PassKey<WebAppCommandManager>(), this);
  internal::CommandBase::Id command_id = command->id();
  CHECK(!base::Contains(commands_, command_id));

  if (!started_) {
    commands_waiting_for_start_.emplace_back(std::move(command), location);
    return;
  }

  DVLOG(2) << "Scheduling command: " << command->GetDebugValue();

  if (is_in_shutdown_) {
    base::OnceClosure callback = command->TakeCallbackWithShutdownArgs(
        base::PassKey<WebAppCommandManager>());
    CHECK(!callback.is_null());
    // Add the log value taking the callback because that will log the callback
    // args.
    AddCommandToLog(*command);
    command->OnShutdown(base::PassKey<WebAppCommandManager>());
    std::move(callback).Run();
    return;
  }

  base::WeakPtr<internal::CommandBase> command_ptr =
      command->GetBaseCommandWeakPtr();
  commands_.try_emplace(command_id, std::move(command));

  // Note: `StartCommand` is guaranteed to be called async.
  command_ptr->RequestLock(
      base::PassKey<WebAppCommandManager>(), &lock_manager_,
      base::BindOnce(&WebAppCommandManager::StartCommand,
                     weak_ptr_factory_reset_on_shutdown_.GetWeakPtr(),
                     command_ptr),
      location);
}

void WebAppCommandManager::StartCommand(
    base::WeakPtr<internal::CommandBase> command,
    base::OnceClosure start_command) {
  // Note: Lock acquisition is always async, so this function is never called
  // synchronously.
  if (!command) {
    // Commands can be destroyed via `CompleteAndSelfDestruct` or
    // shutdown before being started.
    return;
  }

  // This is impossible because this function is always called async & bound to
  // `weak_ptr_factory_reset_on_shutdown_`, and that is invalidated in
  // `OnShutdown()` when `is_in_shutdown_` is set to true.
  CHECK(!is_in_shutdown_);

  DVLOG(2) << "Starting command: " << command->GetDebugValue();
  if (command->ShouldPrepareWebContentsBeforeStart(
          base::PassKey<WebAppCommandManager>())) {
    CHECK(shared_web_contents_);
    url_loader_->PrepareForLoad(shared_web_contents_.get(),
                                std::move(start_command));
  } else {
    std::move(start_command).Run();
  }
}

void WebAppCommandManager::Shutdown() {
  // Ignore duplicate shutdowns for unittests.
  if (is_in_shutdown_) {
    return;
  }
  is_in_shutdown_ = true;
  profile_manager_observation_.Reset();
  weak_ptr_factory_reset_on_shutdown_.InvalidateWeakPtrs();
  AddValueToLog(base::Value("Shutdown has begun"));

  std::vector<base::OnceClosure> callbacks;
  for (const auto& [id, command] : commands_) {
    base::OnceClosure callback =
        command->TakeCallbackWithShutdownArgs(PassKey());
    CHECK(!callback.is_null());
    // Add the log value taking the callback because that will log the callback
    // args.
    AddCommandToLog(*command);
    callbacks.push_back(std::move(callback));
  }
  commands_.clear();
  shared_web_contents_.reset();

  // Call all callbacks AFTER the commands are destroyed, to prevent any sort of
  // re-entry.
  for (base::OnceClosure& callback : callbacks) {
    std::move(callback).Run();
  }
}

base::Value WebAppCommandManager::ToDebugValue() {
  base::Value::List command_log;
  for (const auto& command_value : command_debug_log_) {
    command_log.Append(command_value.Clone());
  }

  base::Value::List queued;
  for (const auto& [command, location] : commands_waiting_for_start_) {
    queued.Append(command->GetDebugValue().Clone());
  }
  for (const auto& [id, command] : commands_) {
    queued.Append(command->GetDebugValue().Clone());
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
    if (command->GetInstallingWebContents(
            base::PassKey<WebAppCommandManager>()) == web_contents) {
      return true;
    }
  }
  return false;
}

std::size_t WebAppCommandManager::GetCommandCountForTesting() {
  return commands_.size() + commands_waiting_for_start_.size();
}

int WebAppCommandManager::GetStartedCommandCountForTesting() {
  std::size_t num = 0;
  for (const auto& [id, command] : commands_) {
    if (command->IsStarted()) {
      ++num;
    }
  }
  return num;
}

std::size_t
WebAppCommandManager::GetCommandsInstallingForWebContentsForTesting() {
  std::size_t num = 0;
  for (const auto& [id, command] : commands_) {
    if (command->GetInstallingWebContents(
            base::PassKey<WebAppCommandManager>()) != nullptr) {
      ++num;
    }
  }
  return num;
}

void WebAppCommandManager::AwaitAllCommandsCompleteForTesting() {
  if (commands_.empty()) {
    return;
  }

  if (!run_loop_for_testing_) {
    run_loop_for_testing_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
  }
  run_loop_for_testing_->Run();
  run_loop_for_testing_.reset();
}

void WebAppCommandManager::SetOnWebContentsCreatedCallbackForTesting(
    base::OnceClosure on_web_contents_created) {
  CHECK_IS_TEST();
  if (shared_web_contents_) {
    std::move(on_web_contents_created).Run();
    return;
  }
  CHECK(!on_web_contents_created_for_testing_);
  on_web_contents_created_for_testing_ = std::move(on_web_contents_created);
}

void WebAppCommandManager::OnCommandComplete(
    base::PassKey<internal::CommandBase>,
    internal::CommandBase* command,
    CommandResult result,
    base::OnceClosure completion_callback) {
  // Note: Calling this function does NOT mean that the command has been
  // started, as commands can call `CompleteAndSelfDestruct` at any time
  // after being scheduled.
  CHECK(command);
  AddCommandToLog(*command);

  auto command_it = commands_.find(command->id());
  // Commands are immediately added to the map when scheduled, and never have a
  // lifetime in this class without being owned by the map.
  CHECK(command_it != commands_.end());
  commands_.erase(command_it);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebAppCommandManager::ClearSharedWebContentsIfUnused,
                     weak_ptr_factory_reset_on_shutdown_.GetWeakPtr()));

  std::move(completion_callback).Run();

  if (commands_.empty() && run_loop_for_testing_) {
    run_loop_for_testing_->Quit();
  }
}

void WebAppCommandManager::OnProfileMarkedForPermanentDeletion(
    Profile* profile_to_be_deleted) {
  if (profile_ != profile_to_be_deleted) {
    return;
  }
  Shutdown();
}

void WebAppCommandManager::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void WebAppCommandManager::ClearSharedWebContentsIfUnused() {
  if (!shared_web_contents_) {
    return;
  }

  bool lock_free = lock_manager_.IsSharedWebContentsLockFree();
  if (!lock_free) {
    return;
  }

  AddValueToLog(base::Value("Destroying the shared web contents."));
  shared_web_contents_.reset();
}

void WebAppCommandManager::AddCommandToLog(
    const internal::CommandBase& command) {
  AddValueToLog(base::Value(command.GetDebugValue().Clone()));
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
  if (command_debug_log_.size() > kMaxLogLength) {
    command_debug_log_.resize(kMaxLogLength);
  }
}

content::WebContents* WebAppCommandManager::EnsureWebContentsCreated(
    base::PassKey<WebAppLockManager>) {
  return EnsureWebContentsCreated();
}

content::WebContents* WebAppCommandManager::EnsureWebContentsCreated() {
  DCHECK(profile_);
  if (!shared_web_contents_) {
    shared_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_app::CreateWebAppInstallTabHelpers(shared_web_contents_.get());
    if (on_web_contents_created_for_testing_) {
      std::move(on_web_contents_created_for_testing_).Run();
    }
  }

  return shared_web_contents_.get();
}

}  // namespace web_app
