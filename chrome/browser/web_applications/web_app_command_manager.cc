// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_manager.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

namespace {

// Simple adapter to allow a user to specify two callbacks instead of a command
// class.
class CommandCallbackAdapter : public WebAppCommand {
 public:
  CommandCallbackAdapter(absl::optional<AppId> app_id,
                         WebAppCommandManager::CallbackCommand command,
                         WebAppCommandManager::CommandResultCallback complete)
      : WebAppCommand(app_id),
        command_(std::move(command)),
        complete_(std::move(complete)) {}
  ~CommandCallbackAdapter() override = default;

  void Start() override {
    DCHECK(command_);
    DCHECK(complete_);
    CommandResult result = std::move(command_).Run();
    SignalCompletionAndSelfDestruct(
        result, base::BindOnce(std::move(complete_), result), {});
  }

  void OnBeforeForcedUninstallFromSync() override {}

  void OnShutdown() override {}

  base::Value ToDebugValue() const override {
    return base::Value("CommandCallbackAdapter");
  }

 private:
  WebAppCommandManager::CallbackCommand command_;
  WebAppCommandManager::CommandResultCallback complete_;
};

base::Value CreateLogValue(const WebAppCommand& command,
                           absl::optional<CommandResult> result) {
  base::Value::Dict dict;
  dict.Set("id", command.id());
  dict.Set("started", command.IsStarted());
  if (command.parent_id())
    dict.Set("parent_id", command.parent_id().value());
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

WebAppCommandManager::CommandQueueState::CommandQueueState() = default;
WebAppCommandManager::CommandQueueState::~CommandQueueState() = default;

base::Value WebAppCommandManager::CommandQueueState::CreateLogValue() const {
  base::Value::List queued;
  for (const auto& queued_command : queued_commands) {
    queued.Append(::web_app::CreateLogValue(*queued_command, absl::nullopt));
  }
  base::Value::Dict dict;
  dict.Set("running_command",
           ::web_app::CreateLogValue(*running_command, absl::nullopt));
  dict.Set("queued_commands", std::move(queued));
  return base::Value(std::move(dict));
}

WebAppCommandManager::WebAppCommandManager() = default;
WebAppCommandManager::~WebAppCommandManager() {
  // Make sure that unittests & browsertests correctly shut down the manager.
  // This ensures that all tests also cover shutdown.
  DCHECK(is_in_shutdown_);
}

void WebAppCommandManager::EnqueueCommand(
    std::unique_ptr<WebAppCommand> command) {
  if (is_in_shutdown_) {
    AddValueToLog(CreateLogValue(*command, CommandResult::kShutdown));
    return;
  }
  WebAppCommandQueueId queue_id = command->queue_id();
  auto queue_it = commands_queues_.find(queue_id);
  if (queue_it == commands_queues_.end()) {
    queue_it = commands_queues_
                   .emplace(std::piecewise_construct, std::make_tuple(queue_id),
                            std::make_tuple())
                   .first;
  }
  CommandQueueState& queue = queue_it->second;
  queue.queued_commands.push_back(std::move(command));
  MaybeRunNextCommand(queue_id);
}

void WebAppCommandManager::EnqueueCallback(
    const absl::optional<AppId>& queue_id,
    CallbackCommand command,
    CommandResultCallback complete) {
  auto command_adapter = std::make_unique<CommandCallbackAdapter>(
      queue_id, std::move(command), std::move(complete));
  if (is_in_shutdown_) {
    AddValueToLog(CreateLogValue(*command_adapter, CommandResult::kShutdown));
    return;
  }
  EnqueueCommand(std::move(command_adapter));
}

void WebAppCommandManager::Shutdown() {
  DCHECK(!is_in_shutdown_);
  is_in_shutdown_ = true;
  AddValueToLog(base::Value("Shutdown has begun"));
  for (const auto& [id, state] : commands_queues_) {
    if (!state.running_command)
      continue;
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        state.running_command->command_sequence_checker_);
    if (state.running_command->IsStarted())
      state.running_command->OnShutdown();
  }
  commands_queues_.clear();
}

void WebAppCommandManager::NotifyBeforeSyncUninstalls(
    std::vector<AppId> app_ids) {
  if (is_in_shutdown_)
    return;

  // To prevent map modification-during-iteration, make a copy of relevant
  // commands. The main complications that can occur are a command calling
  // `CompleteAndDestruct` or `EnqueueCommand` inside of the
  // `OnBeforeForcedUninstallFromSync` call. Because all commands are
  // `Start()`ed asynchronously, we will never have to notify any commands that
  // are newly scheduled. So at most one command needs to be notified per queue,
  // and that command can be destroyed before we notify it.
  std::map<WebAppCommandQueueId, base::WeakPtr<WebAppCommand>>
      commands_to_notify;
  for (const AppId& app_id : app_ids) {
    auto queue_it = commands_queues_.find(app_id);
    if (queue_it == commands_queues_.end())
      continue;
    if (!queue_it->second.running_command)
      continue;
    if (!queue_it->second.running_command->IsStarted())
      continue;
    commands_to_notify[queue_it->first] =
        queue_it->second.running_command->AsWeakPtr();
  }

  for (const auto& [queue_id, command_ptr] : commands_to_notify) {
    if (!command_ptr)
      continue;
    command_ptr->OnBeforeForcedUninstallFromSync();
  }
}

base::Value WebAppCommandManager::ToDebugValue() {
  base::Value::List command_log;
  for (auto& command_value : command_debug_log_) {
    command_log.Append(std ::move(command_value));
  }

  base::Value::Dict running_state;
  for (const auto& [queue_id, queue] : commands_queues_) {
    running_state.Set(queue_id.value_or("global"), queue.CreateLogValue());
  }

  base::Value::Dict state;
  state.Set("command_log", std::move(command_log));
  state.Set("command_queue", base::Value(std::move(running_state)));
  return base::Value(std::move(state));
}

void WebAppCommandManager::OnCommandComplete(
    WebAppCommand* running_command,
    CommandResult result,
    base::OnceClosure completion_callback,
    std::vector<std::unique_ptr<WebAppCommand>> chained_commands) {
  DCHECK(running_command);
  AddValueToLog(CreateLogValue(*running_command, result));

  auto queue_state_it = commands_queues_.find(running_command->queue_id());
  DCHECK(queue_state_it != commands_queues_.end());
  CommandQueueState& queue_state = queue_state_it->second;
  DCHECK(queue_state.running_command.get() == running_command);

  if (is_in_shutdown_) {
    queue_state.running_command.reset();
    std::move(completion_callback).Run();
    return;
  }

  // Add the chained commands.
  for (auto it = chained_commands.rbegin(); it != chained_commands.rend();
       ++it) {
    std::unique_ptr<WebAppCommand>& command = *it;
    if (command.get() == nullptr)
      continue;
    command->parent_id_ = running_command->id();
    if (command->queue_id() == running_command->queue_id()) {
      queue_state.queued_commands.push_front(std::move(command));
    } else {
      EnqueueCommand(std::move(command));
    }
  }

  queue_state.running_command.reset();
  std::move(completion_callback).Run();
  MaybeRunNextCommand(queue_state_it->first);
}

void WebAppCommandManager::MaybeRunNextCommand(
    const WebAppCommandQueueId& queue_id) {
  DCHECK(!is_in_shutdown_);
  auto queue_state_it = commands_queues_.find(queue_id);
  DCHECK(queue_state_it != commands_queues_.end());
  CommandQueueState& queue_state = queue_state_it->second;

  if (queue_state.running_command)
    return;
  if (queue_state.queued_commands.empty())
    return;

  queue_state.running_command = std::move(queue_state.queued_commands.front());
  queue_state.queued_commands.pop_front();
  base::WeakPtr<WebAppCommand> command_ptr =
      queue_state.running_command->weak_factory_.GetWeakPtr();
  // Start is called in a new task to avoid re-entry issues with started tasks
  // calling back into Enqueue/Destroy. This can especially be an issue if this
  // task is being run in response to a call to NotifyBeforeSyncUninstalls.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&WebAppCommandManager::StartCommand,
                                weak_ptr_factory_.GetWeakPtr(),
                                queue_state.running_command.get()));
}

void WebAppCommandManager::StartCommand(WebAppCommand* command) {
  if (is_in_shutdown_)
    return;
#if DCHECK_IS_ON()
  DCHECK(command);
  auto queue_state_it = commands_queues_.find(command->queue_id());
  DCHECK(queue_state_it != commands_queues_.end());
  DCHECK(queue_state_it->second.running_command.get() == command);
  DCHECK(!command->IsStarted());
#endif
  command->Start(this);
}

void WebAppCommandManager::AddValueToLog(base::Value value) {
  static constexpr const int kMaxLogLength = 20;
  command_debug_log_.push_front(std::move(value));
  if (command_debug_log_.size() > kMaxLogLength)
    command_debug_log_.resize(kMaxLogLength);
}

}  // namespace web_app
