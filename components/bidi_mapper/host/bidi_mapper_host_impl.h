// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_HOST_IMPL_H_
#define COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_HOST_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/values.h"
#include "components/bidi_mapper/host/bidi_mapper_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class BrowserContext;
class DevToolsAgentHost;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace bidi_mapper {

// Runs the BiDi mapper in a hidden WebContents that it owns, and drives it
// over the DevTools protocol the way ChromeDriver does:
//  - the mapper is loaded in an empty document;
//  - `window.cdp` in that document is a browser-wide DevTools protocol
//    connection relayed directly by the host to a browser DevTools agent host;
//  - BiDi messages are passed to the document by evaluating a function there,
//    and are received from a binding.
//
// The mapper is the same one that ChromeDriver uses, unmodified. What a host
// needs on top of it is done by a script that the host loads in the document
// before the mapper, see bidi_message_filter.js. The host itself passes BiDi
// messages on as they are.
//
// The WebContents is a privileged one, see
// content::WebContents::PrivilegedParams, because of the DevTools protocol
// connection that the mapper document has. Its renderer process is never
// shared with an ordinary WebContents, and the host's browser DevTools client
// forbids attaching to privileged WebContents.
//
// The DevTools protocol connection is meant for the mapper document only. The
// host therefore closes before another document commits in the WebContents,
// detaching both its page and browser DevTools clients immediately without
// waiting for the WebContents to be destroyed.
class BidiMapperHostImpl : public BidiMapperHost,
                           public content::DevToolsAgentHostClient,
                           public content::WebContentsDelegate,
                           public content::WebContentsObserver {
 public:
  BidiMapperHostImpl(content::BrowserContext* browser_context, Client* client);
  BidiMapperHostImpl(const BidiMapperHostImpl&) = delete;
  BidiMapperHostImpl& operator=(const BidiMapperHostImpl&) = delete;
  ~BidiMapperHostImpl() override;

  void Start(int32_t privileged_feature_id);

  // BidiMapperHost:
  void SendBidiMessage(std::string_view message) override;

 private:
  enum class State {
    kNotStarted,
    kStarting,
    kRunning,
    kClosed,
  };

  class BrowserClient;

  // Receives the result of a DevTools protocol command, or null if the command
  // failed.
  using CommandCallback =
      base::OnceCallback<void(const base::DictValue* result)>;

  // content::DevToolsAgentHostClient (for `page_host_`):
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  // The steps that start the mapper once its document is loaded, in order.
  void AttachAndAddBindings();
  void StartMapper(const base::DictValue* result);
  void OnMapperRunning(const base::DictValue* result);

  // Sends a DevTools protocol command to `page_host_`. `callback` may be null.
  void SendCommand(std::string_view method,
                   base::DictValue params,
                   CommandCallback callback);

  // Adds a binding with the given name to the mapper document. `callback` may
  // be null.
  void AddBinding(std::string_view name, CommandCallback callback);

  // Evaluates `expression` in the mapper document. `callback` may be null.
  void Evaluate(std::string expression,
                bool await_promise,
                CommandCallback callback);

  // Relays DevTools protocol messages between `window.cdp` in the mapper
  // document and `browser_host_`.
  void OnCdpMessageFromMapper(std::string_view payload);
  void OnCdpMessageFromBrowser(std::string_view message);
  void OnBrowserAgentHostClosed(content::DevToolsAgentHost* agent_host);

  // Called when the mapper document calls `bidiHostSendMessage`.
  void OnMessageForClient(std::string message);

  // Stops the mapper and notifies the client: with OnFailed() if the host is
  // starting, or with OnClosed() if it is running. Detaches both DevTools
  // clients right away, taking the DevTools protocol connection away from the
  // mapper document immediately. The WebContents is destroyed in a later task,
  // or when the host is destroyed.
  void CloseOnSessionEnded();
  void FailNotAllowed();
  void Fail(base::Location location);
  void Close(std::optional<FailureReason> failure_reason);

  void DetachFromAgentHosts();
  void DestroyWebContents();

  // Runs `task`, which invokes a method of the client, asynchronously and
  // only if the host still exists.
  void PostToClient(base::OnceClosure task);
  void RunClientTask(base::OnceClosure task);

  const raw_ptr<content::BrowserContext> browser_context_;
  const raw_ptr<Client> client_;

  State state_ = State::kNotStarted;

  // The WebContents that runs the mapper.
  std::unique_ptr<content::WebContents> web_contents_;

  // True once the document that the mapper runs in is committed. Any later
  // navigation replaces the mapper.
  bool mapper_document_committed_ = false;

  // Attached while the mapper is starting or running.
  scoped_refptr<content::DevToolsAgentHost> page_host_;

  // Client and browser-wide agent host backing `window.cdp` in the mapper
  // document. Attached while the mapper is starting or running.
  const std::unique_ptr<BrowserClient> browser_client_;
  scoped_refptr<content::DevToolsAgentHost> browser_host_;

  int last_command_id_ = 0;
  std::map<int, CommandCallback> pending_commands_;

  base::WeakPtrFactory<BidiMapperHostImpl> weak_factory_{this};
};

}  // namespace bidi_mapper

#endif  // COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_HOST_IMPL_H_
