// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied and modified from
// https://chromium.googlesource.com/chromium/src/+/a3f9d4fac81fc86065d867ab08fa4912ddf662c7/headless/public/internal/headless_devtools_client_impl.h
// Modifications include namespace, path, simplifying and removing unnecessary
// codes.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVTOOLS_DEVTOOLS_CLIENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVTOOLS_DEVTOOLS_CLIENT_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/input.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/network.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/target.h"
#include "components/autofill_assistant/browser/devtools/message_dispatcher.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace autofill_assistant {

class DevtoolsClient : public MessageDispatcher,
                       public content::DevToolsAgentHostClient {
 public:
  explicit DevtoolsClient(scoped_refptr<content::DevToolsAgentHost> agent_host);
  ~DevtoolsClient() override;

  input::Domain* GetInput();
  dom::Domain* GetDOM();
  runtime::Domain* GetRuntime();
  network::Domain* GetNetwork();
  target::ExperimentalDomain* GetTarget();

  // MessageDispatcher implementation:
  void SendMessage(
      const char* method,
      std::unique_ptr<base::Value> params,
      const std::string& optional_node_frame_id,
      base::OnceCallback<void(const ReplyStatus&, const base::Value&)> callback)
      override;
  void SendMessage(const char* method,
                   std::unique_ptr<base::Value> params,
                   const std::string& optional_node_frame_id,
                   base::OnceClosure callback) override;
  void RegisterEventHandler(
      const char* method,
      base::RepeatingCallback<void(const base::Value&)> callback) override;
  void UnregisterEventHandler(const char* method) override;

  // content::DevToolsAgentHostClient overrides:
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override;

 private:
  // Contains a callback with or without a result parameter depending on the
  // message type.
  struct Callback {
    Callback();
    Callback(Callback&& other);
    explicit Callback(base::OnceClosure callback);
    explicit Callback(base::OnceCallback<void(const ReplyStatus&,
                                              const base::Value&)> callback);
    ~Callback();

    Callback& operator=(Callback&& other);

    base::OnceClosure callback;
    base::OnceCallback<void(const ReplyStatus&, const base::Value&)>
        callback_with_result;
  };

  // Manages a map to retrieve a session id from a given frame id. Registers
  // itself to client events.
  class FrameTracker {
   public:
    // Register the event handlers and start tracking new targets. |client|
    // must outlive this frame tracker.
    FrameTracker(DevtoolsClient* client);
    ~FrameTracker();

    void Start();
    void Stop();

    // Returns empty string if there is no session for the given |frame_id|.
    std::string GetSessionIdForFrame(std::string frame_id) const;

   private:
    void OnSetAutoAttach(const DevtoolsClient::ReplyStatus& reply_status,
                         std::unique_ptr<target::SetAutoAttachResult> result);
    void OnAttachedToTarget(const base::Value& value);
    void OnDetachedFromTarget(const base::Value& value);

    // Save find of targetInfo.targetId in the given value. Returns
    // empty string if nothing is found.
    std::string FindTargetId(const base::Value& value);

    // Find sessionId in the given value. Returns empty string if nothing is
    // found.
    std::string FindSessionId(const base::Value& value);

    DevtoolsClient* client_;
    bool started_ = false;

    // Holds the mappings from frame id to session id.
    std::unordered_map<std::string, std::string> sessions_map_;

    base::WeakPtrFactory<FrameTracker> weak_ptr_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(FrameTracker);
  };

  // If the frame is known to devtools, return the session id for it.
  std::string GetSessionIdForFrame(const std::string& frame_id) const;

  template <typename CallbackType>
  void SendMessageWithParams(const char* method,
                             std::unique_ptr<base::Value> params,
                             const std::string& optional_node_frame_id,
                             CallbackType callback);
  bool DispatchMessageReply(std::unique_ptr<base::Value> owning_message,
                            const base::DictionaryValue& message_dict);
  void DispatchMessageReplyWithResultTask(
      std::unique_ptr<base::Value> owning_message,
      base::OnceCallback<void(const ReplyStatus&, const base::Value&)> callback,
      const ReplyStatus& reply_status,
      const base::Value* result_dict);
  bool DispatchEvent(std::unique_ptr<base::Value> owning_message,
                     const base::DictionaryValue& message_dict);

  using EventHandler = base::RepeatingCallback<void(const base::Value&)>;
  using EventHandlerMap = std::unordered_map<std::string, EventHandler>;
  void DispatchEventTask(std::unique_ptr<base::Value> owning_message,
                         const EventHandler* event_handler,
                         const base::DictionaryValue* result_dict);
  void FillReplyStatusFromErrorDict(ReplyStatus* status,
                                    const base::DictionaryValue& error_dict);

  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  scoped_refptr<base::SequencedTaskRunner> browser_main_thread_;

  input::ExperimentalDomain input_domain_;
  dom::ExperimentalDomain dom_domain_;
  runtime::ExperimentalDomain runtime_domain_;
  network::ExperimentalDomain network_domain_;
  target::ExperimentalDomain target_domain_;
  std::unordered_map<int, Callback> pending_messages_;
  EventHandlerMap event_handlers_;
  bool renderer_crashed_;
  int next_message_id_;
  FrameTracker frame_tracker_;

  base::WeakPtrFactory<DevtoolsClient> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DevtoolsClient);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVTOOLS_DEVTOOLS_CLIENT_H_
