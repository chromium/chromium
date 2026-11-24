// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bidi_mapper/host/bidi_mapper_host_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bidi_mapper/host/bidi_mapper_host.h"
#include "components/bidi_mapper/host/bidi_mapper_ui.h"
#include "components/bidi_mapper/host/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace bidi_mapper {

namespace {

// The name that the mapper expects its DevTools protocol connection to have
// in its document.
constexpr char kCdpBindingName[] = "cdp";

// Wraps the raw `cdp` Runtime.addBinding function in the `{send, onmessage}`
// object that the mapper's WindowCdpTransport expects.
constexpr char kInitCdpScript[] = R"(
  (() => {
    const send = window.cdp;
    delete window.cdp;
    if (typeof send !== 'function') {
      throw new Error('The CDP binding of the BiDi mapper host is missing');
    }
    window.cdp = {
      onmessage: () => {},
      send,
    };
  })();
)";

// What the host and the message filter in the mapper document use to talk to
// each other, see bidi_message_filter.js: the bindings that the filter calls,
// and the function of the filter that takes the messages of the client.
constexpr char kSendMessageBindingName[] = "bidiHostSendMessage";
constexpr char kEndSessionBindingName[] = "bidiHostEndSession";
constexpr char kOnMessageFunctionName[] = "bidiHostOnMessage";

// Returns true if `result` is the result of a Runtime.evaluate command that
// completed without an exception. `result` is null if the command failed.
bool EvaluateSucceeded(const base::DictValue* result) {
  if (!result || !result->FindDict("result")) {
    LOG(ERROR) << "Failed to evaluate in the BiDi mapper document";
    return false;
  }
  const base::DictValue* exception = result->FindDict("exceptionDetails");
  if (!exception) {
    return true;
  }
  const std::string* description =
      exception->FindStringByDottedPath("exception.description");
  if (!description) {
    description = exception->FindString("text");
  }
  LOG(ERROR) << "Evaluation in the BiDi mapper document failed: "
             << (description ? *description : "unknown error");
  return false;
}

}  // namespace

// static
bool BidiMapperHost::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kChromeDriverExtensionApi);
}

// static
std::unique_ptr<BidiMapperHost> BidiMapperHost::Create(
    content::BrowserContext* browser_context,
    int32_t privileged_feature_id,
    Client* client) {
  if (!IsEnabled()) {
    return nullptr;
  }
  auto host = std::make_unique<BidiMapperHostImpl>(browser_context, client);
  host->Start(privileged_feature_id);
  return host;
}

// Client of `browser_host_` backing `window.cdp` in the mapper document.
// Separate from `BidiMapperHostImpl` (which attaches to the mapper's own
// privileged WebContents) so that it can forbid attaching to privileged
// WebContents.
class BidiMapperHostImpl::BrowserClient
    : public content::DevToolsAgentHostClient {
 public:
  explicit BrowserClient(BidiMapperHostImpl* host) : host_(host) {}
  BrowserClient(const BrowserClient&) = delete;
  BrowserClient& operator=(const BrowserClient&) = delete;
  ~BrowserClient() override = default;

  // content::DevToolsAgentHostClient:
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    host_->OnCdpMessageFromBrowser(base::as_string_view(message));
  }

  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {
    // Called e.g. when `content::DevToolsAgentHost::DetachAllClients()`
    // detaches `browser_host_` before `page_host_`.
    host_->OnBrowserAgentHostClosed(agent_host);
  }

  bool MayAttachToRenderFrameHost(
      content::RenderFrameHost* render_frame_host) override {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    return web_contents && !web_contents->IsPrivileged();
  }

 private:
  const raw_ptr<BidiMapperHostImpl> host_;
};

BidiMapperHostImpl::BidiMapperHostImpl(content::BrowserContext* browser_context,
                                       Client* client)
    : browser_context_(browser_context),
      client_(client),
      browser_client_(std::make_unique<BrowserClient>(this)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(browser_context_);
  CHECK(client_);
}

BidiMapperHostImpl::~BidiMapperHostImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  state_ = State::kClosed;
  DetachFromAgentHosts();
  Observe(nullptr);
  DestroyWebContents();
}

void BidiMapperHostImpl::Start(int32_t privileged_feature_id) {
  CHECK_EQ(state_, State::kNotStarted);
  state_ = State::kStarting;

  content::WebContents::CreateParams create_params(browser_context_);
  // The mapper has nothing to show.
  create_params.initially_hidden = true;
  // The mapper document has a DevTools protocol connection to the whole
  // browser, so the WebContents is a privileged one. Its renderer process is
  // never shared with an ordinary WebContents, the mappers of other hosts
  // cannot attach to it (see BrowserClient::MayAttachToRenderFrameHost()), and
  // the embedder knows to keep extensions away from it. The mapper uses neither
  // service workers nor shared workers.
  content::WebContents::PrivilegedParams privileged_params;
  privileged_params.feature_id = privileged_feature_id;
  privileged_params.disallow_service_worker_control = true;
  privileged_params.disallow_shared_workers = true;
  create_params.privileged_params = privileged_params;
  web_contents_ = content::WebContents::Create(create_params);

  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(GURL(kBidiMapperUIURL)));
  // The renderer of a hidden WebContents runs at a low priority otherwise.
  web_contents_->BoostBackgroundExecution();
}

void BidiMapperHostImpl::SendBidiMessage(std::string_view message) {
  // The host closes before its client is told, see Close(), so the client may
  // send a message to a host that is closed already. Drop it.
  if (state_ == State::kClosed) {
    return;
  }
  CHECK_EQ(state_, State::kRunning);
  // The message filter takes the message as a string.
  Evaluate(base::StrCat({kOnMessageFunctionName, "(",
                         base::GetQuotedJSONString(message), ")"}),
           /*await_promise=*/false, CommandCallback());
}

void BidiMapperHostImpl::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  CHECK_EQ(agent_host, page_host_.get());
  std::optional<base::DictValue> parsed = base::JSONReader::ReadDict(
      base::as_string_view(message),
      base::JSON_PARSE_RFC | base::JSON_REPLACE_INVALID_CHARACTERS);
  if (!parsed) {
    Fail(FROM_HERE);
    return;
  }

  if (std::optional<int> id = parsed->FindInt("id")) {
    auto it = pending_commands_.find(*id);
    if (it == pending_commands_.end()) {
      Fail(FROM_HERE);
      return;
    }
    CommandCallback callback = std::move(it->second);
    pending_commands_.erase(it);
    if (callback) {
      // A command that failed has an error instead of a result.
      std::move(callback).Run(parsed->FindDict("result"));
    }
    return;
  }

  const std::string* method = parsed->FindString("method");
  if (!method || *method != "Runtime.bindingCalled") {
    return;
  }
  base::DictValue* params = parsed->FindDict("params");
  if (!params) {
    Fail(FROM_HERE);
    return;
  }
  const std::string* name = params->FindString("name");
  if (!name) {
    Fail(FROM_HERE);
    return;
  }
  if (*name == kCdpBindingName) {
    std::string* payload = params->FindString("payload");
    if (!payload) {
      Fail(FROM_HERE);
      return;
    }
    OnCdpMessageFromMapper(*payload);
  } else if (*name == kSendMessageBindingName) {
    std::string* payload = params->FindString("payload");
    if (!payload) {
      Fail(FROM_HERE);
      return;
    }
    OnMessageForClient(std::move(*payload));
  } else if (*name == kEndSessionBindingName) {
    CloseOnSessionEnded();
  } else {
    Fail(FROM_HERE);
  }
}

void BidiMapperHostImpl::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  CHECK_EQ(agent_host, page_host_.get());
  // The agent host of the mapper document is detaching its clients. Do not
  // call DetachClient() on it from Close(), which the agent host does not
  // expect while iterating its clients.
  page_host_.reset();
  Fail(FROM_HERE);
}

void BidiMapperHostImpl::CloseContents(content::WebContents* source) {
  Fail(FROM_HERE);
}

void BidiMapperHostImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle->IsInPrimaryMainFrame());
  CHECK(!navigation_handle->IsSameDocument());
  if (!mapper_document_committed_) {
    if (navigation_handle->HasCommitted() &&
        !navigation_handle->IsErrorPage()) {
      mapper_document_committed_ = true;
    } else {
      LOG(ERROR) << "Failed to load the BiDi mapper document";
      Fail(FROM_HERE);
    }
    return;
  }
  // Another document replaced the one that the mapper runs in. The host closes
  // before that, in ReadyToCommitNavigation(). This is for a document that
  // commits without that notice.
  if (navigation_handle->HasCommitted()) {
    Fail(FROM_HERE);
  }
}

void BidiMapperHostImpl::DocumentOnLoadCompletedInPrimaryMainFrame() {
  CHECK_EQ(state_, State::kStarting);
  CHECK(mapper_document_committed_);
  CHECK(!page_host_);
  // The mapper gets a DevTools protocol connection to the whole browser. Only
  // the document that is meant to run it may get one.
  content::RenderFrameHost* frame = web_contents_->GetPrimaryMainFrame();
  if (frame->IsErrorDocument() ||
      frame->GetLastCommittedOrigin() !=
          url::Origin::Create(GURL(kBidiMapperUIURL))) {
    Fail(FROM_HERE);
    return;
  }
  AttachAndAddBindings();
}

void BidiMapperHostImpl::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  Fail(FROM_HERE);
}

void BidiMapperHostImpl::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle->IsInPrimaryMainFrame());
  // Another document is about to replace the one that the mapper runs in.
  // Closing when it has committed would be too late: the document is created
  // before the host hears of the commit. The renderer is asked to commit after
  // this call.
  if (mapper_document_committed_) {
    Fail(FROM_HERE);
  }
}

void BidiMapperHostImpl::AttachAndAddBindings() {
  // The embedder decides whether DevTools clients may attach.
  scoped_refptr<content::DevToolsAgentHost> page_host =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents_.get());
  if (!page_host->AttachClient(this)) {
    FailNotAllowed();
    return;
  }
  page_host_ = std::move(page_host);

  scoped_refptr<content::DevToolsAgentHost> browser_host =
      content::DevToolsAgentHost::CreateForDiscovery();
  if (!browser_host->AttachClient(browser_client_.get())) {
    FailNotAllowed();
    return;
  }
  browser_host_ = std::move(browser_host);

  // The commands are processed in order, and both the CDP initializer and the
  // message filter verify that their bindings exist, so it is enough to wait
  // for the last command. `Runtime.enable` is needed so that the bindings are
  // installed in the empty document, which has no V8 context yet in production.
  SendCommand("Runtime.enable", base::DictValue(), CommandCallback());
  AddBinding(kCdpBindingName, CommandCallback());
  AddBinding(kSendMessageBindingName, CommandCallback());
  AddBinding(
      kEndSessionBindingName,
      base::BindOnce(&BidiMapperHostImpl::StartMapper, base::Unretained(this)));
}

void BidiMapperHostImpl::StartMapper(const base::DictValue* result) {
  if (!result) {
    LOG(ERROR) << "Failed to add bindings to the BiDi mapper document";
    Fail(FROM_HERE);
    return;
  }
  // The message filter has to be in place before the mapper, which it stands
  // in front of. The mapper needs the id of its own target to tell it apart
  // from the targets that it automates.
  Evaluate(base::StrCat(
               {"(async () => {", kInitCdpScript, "await import('./",
                kBidiMessageFilterScriptPath, "'); await import('./",
                kBidiMapperScriptPath, "'); await window.runMapperInstance(",
                base::GetQuotedJSONString(page_host_->GetId()), ");})()"}),
           /*await_promise=*/true,
           base::BindOnce(&BidiMapperHostImpl::OnMapperRunning,
                          base::Unretained(this)));
}

void BidiMapperHostImpl::OnMapperRunning(const base::DictValue* result) {
  if (!EvaluateSucceeded(result)) {
    Fail(FROM_HERE);
    return;
  }
  CHECK_EQ(state_, State::kStarting);
  state_ = State::kRunning;

  PostToClient(base::BindOnce(&Client::OnStarted, base::Unretained(client_)));
}

void BidiMapperHostImpl::SendCommand(std::string_view method,
                                     base::DictValue params,
                                     CommandCallback callback) {
  const int id = ++last_command_id_;
  pending_commands_.emplace(id, std::move(callback));
  base::DictValue command;
  command.Set("id", id);
  command.Set("method", method);
  command.Set("params", std::move(params));
  std::optional<std::string> json = base::WriteJson(command);
  // The commands of the host only hold strings and booleans.
  CHECK(json);
  page_host_->DispatchProtocolMessage(this, base::as_byte_span(*json));
}

void BidiMapperHostImpl::AddBinding(std::string_view name,
                                    CommandCallback callback) {
  base::DictValue params;
  params.Set("name", name);
  // Expose to the default execution context only.
  params.Set("executionContextName", "");
  SendCommand("Runtime.addBinding", std::move(params), std::move(callback));
}

void BidiMapperHostImpl::Evaluate(std::string expression,
                                  bool await_promise,
                                  CommandCallback callback) {
  base::DictValue params;
  params.Set("expression", std::move(expression));
  if (await_promise) {
    params.Set("awaitPromise", true);
  }
  // The mapper document does not allow code generation from strings. An
  // evaluation lifts that restriction while it runs unless told otherwise,
  // which would let the mapper find out that it can generate code, and fail
  // when it does so later.
  params.Set("allowUnsafeEvalBlockedByCSP", false);
  SendCommand("Runtime.evaluate", std::move(params), std::move(callback));
}

void BidiMapperHostImpl::OnCdpMessageFromMapper(std::string_view payload) {
  if (state_ == State::kClosed || !browser_host_) {
    return;
  }
  browser_host_->DispatchProtocolMessage(browser_client_.get(),
                                         base::as_byte_span(payload));
}

void BidiMapperHostImpl::OnCdpMessageFromBrowser(std::string_view message) {
  if (state_ == State::kClosed || !page_host_) {
    return;
  }
  Evaluate(base::StrCat({"try { window.", kCdpBindingName, ".onmessage(",
                         base::GetQuotedJSONString(message),
                         "); } catch (e) { console.error(e); }"}),
           /*await_promise=*/false, CommandCallback());
}

void BidiMapperHostImpl::OnBrowserAgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  CHECK_EQ(agent_host, browser_host_.get());
  // The browser agent host is detaching its clients (e.g. from
  // `content::DevToolsAgentHost::DetachAllClients()`). Do not call
  // DetachClient() on it from Close().
  browser_host_.reset();
  Fail(FROM_HERE);
}

void BidiMapperHostImpl::OnMessageForClient(std::string message) {
  if (state_ != State::kRunning) {
    Fail(FROM_HERE);
    return;
  }
  PostToClient(base::BindOnce(&Client::OnMessage, base::Unretained(client_),
                              std::move(message)));
}

void BidiMapperHostImpl::CloseOnSessionEnded() {
  if (state_ != State::kRunning) {
    Fail(FROM_HERE);
    return;
  }
  Close(std::nullopt);
}

void BidiMapperHostImpl::FailNotAllowed() {
  CHECK_EQ(state_, State::kStarting);
  Close(FailureReason::kNotAllowed);
}

void BidiMapperHostImpl::Fail(base::Location location) {
  if (state_ == State::kClosed) {
    return;
  }
  LOG(ERROR) << "Closing BiDi mapper host from " << location.ToString();
  Close(FailureReason::kFailed);
}

void BidiMapperHostImpl::Close(std::optional<FailureReason> failure_reason) {
  if (state_ == State::kClosed) {
    return;
  }
  const State previous_state = std::exchange(state_, State::kClosed);
  pending_commands_.clear();
  DetachFromAgentHosts();
  Observe(nullptr);

  // This may be called while the WebContents notifies its observers, when it
  // cannot be destroyed. Keep ownership in `web_contents_` so that destroying
  // the host before the task runs still destroys the WebContents before
  // `browser_context_`.
  if (web_contents_) {
    web_contents_->SetDelegate(nullptr);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&BidiMapperHostImpl::DestroyWebContents,
                                  weak_factory_.GetWeakPtr()));
  }

  if (previous_state == State::kStarting) {
    PostToClient(base::BindOnce(&Client::OnFailed, base::Unretained(client_),
                                failure_reason.value()));
  } else if (previous_state == State::kRunning) {
    PostToClient(base::BindOnce(&Client::OnClosed, base::Unretained(client_)));
  }
}

void BidiMapperHostImpl::DetachFromAgentHosts() {
  if (browser_host_) {
    browser_host_->DetachClient(browser_client_.get());
    browser_host_.reset();
  }
  if (page_host_) {
    page_host_->DetachClient(this);
    page_host_.reset();
  }
}

void BidiMapperHostImpl::DestroyWebContents() {
  if (web_contents_) {
    web_contents_->SetDelegate(nullptr);
    web_contents_.reset();
  }
}

void BidiMapperHostImpl::PostToClient(base::OnceClosure task) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BidiMapperHostImpl::RunClientTask,
                                weak_factory_.GetWeakPtr(), std::move(task)));
}

void BidiMapperHostImpl::RunClientTask(base::OnceClosure task) {
  std::move(task).Run();
  // The client may have destroyed the host.
}

}  // namespace bidi_mapper
