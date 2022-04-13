// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_

#include <deque>
#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class WebAppRegistrar;

// The command manager is used to enqueue commands or callbacks to write & read
// from the WebAppProvider system.
// Commands are queued based on a `WebAppCommandQueueId`, and each queue is
// independent. To use, simply call `EnqueueCommand` to enqueue the given
// command or a CallbackCommand with given callback on it's respective queue.
// The queue of a command is determined by `WebAppCommand::queue_id()`.
//
// Commands will be executed (`Start()` will be called) in-order, and the next
// command will not execute until `SignalCompletionAndSelfDestruct()` was called
// by the last command. Commands can specify 'chained' commands on completion
// that can preempt already scheduled commands if they are in the same queue.
class WebAppCommandManager {
 public:
  WebAppCommandManager();
  virtual ~WebAppCommandManager();

  // Enqueues the given command in the queue corresponding to the command's
  // `queue_id()`. `Start()` will always be called asynchronously.
  void EnqueueCommand(std::unique_ptr<WebAppCommand> command);

  // Called on system shutdown. This call is also forwarded to any commands that
  // have been `Start()`ed.
  void Shutdown();

  // Called by the sync integration when a list of apps are going to be deleted
  // from the registry. Any commands that whose `queue_id()`s match an id in
  // `app_id` who have also been `Start()`ed will also be notified.
  void NotifyBeforeSyncUninstalls(const std::vector<AppId>& app_ids);

  // Outputs a debug value of the state of the commands system, including
  // running and queued commands.
  base::Value ToDebugValue();

 protected:
  friend class WebAppCommand;

  void OnCommandComplete(
      WebAppCommand* running_command,
      CommandResult result,
      base::OnceClosure completion_callback,
      std::vector<std::unique_ptr<WebAppCommand>> chained_commands);

 private:
  void MaybeRunNextCommand(const WebAppCommandQueueId& queue_id);

  void StartCommand(WebAppCommand* command);

  void AddValueToLog(base::Value value);

  struct CommandQueueState {
    CommandQueueState();
    ~CommandQueueState();

    base::Value CreateLogValue() const;

    std::unique_ptr<WebAppCommand> running_command;
    std::deque<std::unique_ptr<WebAppCommand>> queued_commands;
  };
  SEQUENCE_CHECKER(command_sequence_checker_);

  std::map<WebAppCommandQueueId, CommandQueueState> commands_queues_;

  bool is_in_shutdown_ = false;
  std::deque<base::Value> command_debug_log_;

  raw_ptr<WebAppRegistrar> registrar_ = nullptr;

  base::WeakPtrFactory<WebAppCommandManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_
