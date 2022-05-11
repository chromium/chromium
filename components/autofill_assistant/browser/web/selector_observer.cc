// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/selector_observer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/js_filter_builder.h"
#include "components/autofill_assistant/browser/web/selector_observer_script.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/web_contents.h"
#include "js_snippets.h"
#include "selector_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
namespace {
// Javascript code to get the document.
const char kGetDocumentElement[] = "document";
}  // namespace

SelectorObserver::ObservableSelector::ObservableSelector(
    const SelectorId selector_id,
    const SelectorProto& proto,
    bool strict)
    : selector_id(selector_id), proto(proto), strict(strict) {}
SelectorObserver::ObservableSelector::~ObservableSelector() = default;
SelectorObserver::ObservableSelector::ObservableSelector(
    const ObservableSelector&) = default;

SelectorObserver::Update::Update() = default;
SelectorObserver::Update::~Update() = default;
SelectorObserver::Update::Update(const Update&) = default;

SelectorObserver::RequestedElement::RequestedElement(
    const SelectorId& selector_id,
    int element_id)
    : selector_id(selector_id), element_id(element_id) {}
SelectorObserver::RequestedElement::~RequestedElement() = default;
SelectorObserver::RequestedElement::RequestedElement(const RequestedElement&) =
    default;

SelectorObserver::Settings::Settings(const base::TimeDelta& max_wait_time,
                                     const base::TimeDelta& min_check_interval,
                                     const base::TimeDelta& extra_timeout,
                                     const base::TimeDelta& debounce_interval)
    : max_wait_time(max_wait_time),
      min_check_interval(min_check_interval),
      extra_timeout(extra_timeout),
      debounce_interval(debounce_interval) {}
SelectorObserver::Settings::~Settings() = default;
SelectorObserver::Settings::Settings(const Settings&) = default;

SelectorObserver::SelectorObserver(
    const std::vector<ObservableSelector>& selectors,
    const Settings& settings,
    content::WebContents* web_contents,
    DevtoolsClient* devtools_client,
    const UserData* user_data,
    Callback update_callback)
    : settings_(settings),
      devtools_client_(devtools_client),
      web_contents_(web_contents),
      user_data_(user_data),
      update_callback_(update_callback) {
  const DomRoot root(/* frame_id = */ "", DomRoot::kUseMainDoc);
  wait_time_remaining_ms_[root] = settings.max_wait_time.InMilliseconds();
  for (auto& selector : selectors) {
    selectors_.emplace(std::make_pair(selector.selector_id, selector));
    // Every selector starts in the root frame
    dom_roots_.emplace(std::make_pair(selector.selector_id, 0), root);
  }
}

SelectorObserver::~SelectorObserver() {
  Stop();
}

ClientStatus SelectorObserver::Start(base::OnceClosure finished_callback) {
  DCHECK(state_ == State::INITIALIZED);
  finished_callback_ = std::move(finished_callback);
  for (auto& selector : selectors_) {
    auto result =
        user_data::ResolveSelectorUserData(&selector.second.proto, user_data_);
    if (!result.ok()) {
      EnterState(State::ERROR_STATE);
      return result;
    }
  }

  EnterState(State::RUNNING);
  const DomRoot root(/* frame_id = */ "", DomRoot::kUseMainDoc);
  ResolveObjectIdAndInjectFrame(root, 0);

  timeout_timer_ = std::make_unique<base::OneShotTimer>();
  timeout_timer_->Start(FROM_HERE, MaxTimeRemaining() + settings_.extra_timeout,
                        base::BindOnce(&SelectorObserver::OnHardTimeout,
                                       weak_ptr_factory_.GetWeakPtr()));

  return OkClientStatus();
}

void SelectorObserver::GetElementsAndStop(
    const std::vector<RequestedElement>& requested_elements,
    base::OnceCallback<void(
        const ClientStatus&,
        const base::flat_map<SelectorId, DomObjectFrameStack>&)> callback) {
  DCHECK(state_ == State::RUNNING);
  timeout_timer_.reset();
  EnterState(State::FETCHING_ELEMENTS);
  get_elements_callback_ = std::move(callback);
  base::flat_map<DomRoot, std::vector<RequestedElement>> elements_by_dom_root;

  for (auto& element : requested_elements) {
    size_t depth = 0;
    DomRoot leaf_dom_root;
    auto it = dom_roots_.find(std::make_pair(element.selector_id, depth++));
    DCHECK(it != dom_roots_.end()) << "Invalid selector_id";
    while (it != dom_roots_.end()) {
      leaf_dom_root = it->second;
      it = dom_roots_.find(std::make_pair(element.selector_id, depth++));
    }
    elements_by_dom_root[leaf_dom_root].push_back(element);
  }

  pending_get_elements_responses_ = elements_by_dom_root.size();
  for (auto& entry : elements_by_dom_root) {
    std::vector<int> element_ids_list;
    for (auto& element : entry.second) {
      element_ids_list.push_back(element.element_id);
    }
    GetElementsByElementId(
        entry.first, element_ids_list,
        base::BindOnce(&SelectorObserver::OnGetElementsResponse,
                       weak_ptr_factory_.GetWeakPtr(), entry.first,
                       entry.second));
  }
  // In case there were no elements requested.
  MaybeFinishedGettingElements();
}

void SelectorObserver::OnGetElementsResponse(
    const DomRoot& dom_root,
    const std::vector<RequestedElement>& elements,
    const base::flat_map<int, std::string>& object_ids) {
  if (state_ != State::FETCHING_ELEMENTS) {
    return;
  }
  for (auto& element : elements) {
    auto element_object_id_entry = object_ids.find(element.element_id);
    if (element_object_id_entry == object_ids.end()) {
      NOTREACHED();
      FailWithError(UnexpectedErrorStatus(__FILE__, __LINE__));
      return;
    }
    DomObjectFrameStack element_dom_object;
    element_dom_object.object_data.object_id = element_object_id_entry->second;
    element_dom_object.object_data.node_frame_id = dom_root.frame_id();

    size_t depth = 1;
    std::string prev_frame_id = "";
    auto it = dom_roots_.find(std::make_pair(element.selector_id, depth++));
    while (it != dom_roots_.end() && it->second != dom_root) {
      auto entry = iframe_object_ids_.find(it->second);
      if (entry != iframe_object_ids_.end()) {
        JsObjectIdentifier frame;
        frame.object_id = entry->second;
        frame.node_frame_id = prev_frame_id;
        element_dom_object.frame_stack.push_back(frame);
      }
      prev_frame_id = it->second.frame_id();
      it = dom_roots_.find(std::make_pair(element.selector_id, depth++));
    }
    get_elements_response_[element.selector_id] = element_dom_object;
  }
  pending_get_elements_responses_--;
  MaybeFinishedGettingElements();
}

void SelectorObserver::MaybeFinishedGettingElements() {
  if (pending_get_elements_responses_ == 0) {
    auto callback = std::move(get_elements_callback_);
    auto response = get_elements_response_;
    Stop();
    // Entering TERMINATED state can destroy this.
    EnterState(State::TERMINATED);
    std::move(callback).Run(OkClientStatus(), response);
  }
}

void SelectorObserver::Stop() {
  while (!script_api_object_ids_.empty()) {
    TerminateDomRoot(script_api_object_ids_.begin()->first);
  }
}

void SelectorObserver::FailWithError(const ClientStatus& status) {
  DCHECK(!status.ok());
  VLOG(1) << "Selector observer failed: " << status;
  if (state_ == State::RUNNING) {
    update_callback_.Run(status, {}, this);
  } else if (state_ == State::FETCHING_ELEMENTS) {
    std::move(get_elements_callback_).Run(status, get_elements_response_);
  }
  timeout_timer_.reset();
  EnterState(State::ERROR_STATE);
}

template <typename T>
bool SelectorObserver::FailIfError(const DevtoolsClient::ReplyStatus& js_status,
                                   const T* result,
                                   const char* file,
                                   int line) {
  if (!js_status.is_ok()) {
    FailWithError(UnexpectedErrorStatus(file, line));
    return false;
  }
  auto status = CheckJavaScriptResult(js_status, result, file, line);
  if (!status.ok()) {
    FailWithError(status);
    return false;
  }
  return true;
}

bool SelectorObserver::GetObjectId(const runtime::RemoteObject* result,
                                   std::string* out,
                                   const char* file,
                                   int line) {
  if (!SafeGetObjectId(result, out)) {
    VLOG(1) << "Failed to get Object ID.";
    FailWithError(UnexpectedErrorStatus(file, line));
    return false;
  }
  return true;
}

void SelectorObserver::EnterState(State new_state) {
  if (state_ == new_state)
    return;
  if (new_state < state_) {
    NOTREACHED();
    return;
  }
  VLOG(2) << " status " << static_cast<int>(state_) << " -> "
          << static_cast<int>(new_state);
  state_ = new_state;
  if ((state_ == State::TERMINATED || state_ == State::ERROR_STATE) &&
      finished_callback_) {
    std::move(finished_callback_).Run();
  }
}

ClientStatus SelectorObserver::CallSelectorObserverScriptApi(
    const DomRoot& dom_root,
    const std::string& function,
    runtime::CallFunctionOnParams::CallFunctionOnParamsBuilder<0>&&
        param_builder,
    base::OnceCallback<void(const MessageDispatcher::ReplyStatus&,
                            std::unique_ptr<runtime::CallFunctionOnResult>)>
        callback) {
  auto entry = script_api_object_ids_.find(dom_root);
  if (entry == script_api_object_ids_.end()) {
    NOTREACHED();
    return UnexpectedErrorStatus(__FILE__, __LINE__);
  }
  devtools_client_->GetRuntime()->CallFunctionOn(
      param_builder.SetObjectId(entry->second)
          .SetFunctionDeclaration(base::StrCat(
              {"(function(...args) { return this.", function, "(...args); })"}))
          .Build(),
      dom_root.frame_id(), std::move(callback));
  return OkClientStatus();
}

void SelectorObserver::TerminateDomRoot(const DomRoot& dom_root) {
  CallSelectorObserverScriptApi(
      dom_root, "terminate", runtime::CallFunctionOnParams::Builder(),
      base::BindOnce(&SelectorObserver::OnTerminateDone,
                     weak_ptr_factory_.GetWeakPtr()));
  script_api_object_ids_.erase(dom_root);
}

void SelectorObserver::OnTerminateDone(
    const MessageDispatcher::ReplyStatus& js_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!VLOG_IS_ON(1))
    return;

  if (!js_status.is_ok()) {
    VLOG(1) << "Failed to terminate: " << js_status.error_code;
  }
  auto status =
      CheckJavaScriptResult(js_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << "Failed to terminate: " << status;
  }
}

void SelectorObserver::Continue() {
  DCHECK(state_ == State::RUNNING);
  for (auto& entry : script_api_object_ids_) {
    AwaitChanges(entry.first);
  }
}

void SelectorObserver::InjectOrAddSelectorsByParent(
    const DomRoot& parent,
    const std::string& object_id,
    const std::vector<SelectorId>& selector_ids) {
  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
      parent.frame_id(),
      base::BindOnce(&SelectorObserver::OnDescribeNodeDone,
                     weak_ptr_factory_.GetWeakPtr(), parent, object_id,
                     selector_ids));
}

void SelectorObserver::OnDescribeNodeDone(
    const DomRoot& parent,
    const std::string& parent_object_id,
    const std::vector<SelectorId>& selector_ids,
    const MessageDispatcher::ReplyStatus& status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!status.is_ok()) {
    // TODO(b/205676462): It's possible .
    FailWithError(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }
  if (state_ != State::RUNNING) {
    return;
  }

  int frame_depth = frame_depth_.at(parent) + 1;
  auto* node = result->GetNode();
  std::vector<int> backend_ids;
  if (node->GetNodeName() == "IFRAME") {
    // See: b/206647825
    if (!node->HasFrameId()) {
      NOTREACHED() << "Frame without ID";  // Ensure all frames have an id.
      FailWithError(UnexpectedErrorStatus(__FILE__, __LINE__));
      return;
    }

    DomRoot dom_root;
    if (node->HasContentDocument()) {
      // If the frame has a ContentDocument it's considered a local frame.
      // In this case frame_id doesn't change.
      dom_root = DomRoot(parent.frame_id(),
                         node->GetContentDocument()->GetBackendNodeId());
    } else {
      // OOP frame.
      dom_root = DomRoot(node->GetFrameId(), DomRoot::kUseMainDoc);
    }
    iframe_object_ids_.emplace(dom_root, parent_object_id);
    InjectOrAddSelectorsToDomRoot(dom_root, frame_depth, selector_ids);
  } else if (node->HasShadowRoots()) {
    // We aren't entering a frame but a shadow dom.
    DomRoot dom_root(parent.frame_id(),
                     node->GetShadowRoots()->front()->GetBackendNodeId());
    InjectOrAddSelectorsToDomRoot(dom_root, frame_depth, selector_ids);
  }
}

void SelectorObserver::InjectOrAddSelectorsToDomRoot(
    const DomRoot& dom_root,
    size_t frame_depth,
    const std::vector<SelectorId>& selector_ids) {
  std::vector<SelectorId> new_selector_ids;
  --pending_frame_injects_;

  for (const auto& selector_id : selector_ids) {
    auto key = std::make_pair(selector_id, frame_depth);
    auto existing_entry = dom_roots_.find(key);
    if (existing_entry != dom_roots_.end() &&
        existing_entry->second == dom_root) {
      continue;
    }
    new_selector_ids.push_back(selector_id);
    dom_roots_[key] = dom_root;
    // If we have changed a frame, all following frames in the chain are
    // invalidated.
    InvalidateDeeperFrames(selector_id, frame_depth + 1);
  }
  if (new_selector_ids.empty())
    return;
  TerminateUnneededDomRoots();

  if (script_api_object_ids_.contains(dom_root)) {
    // Frame known and we are injected into.
    AddSelectorsToDomRoot(dom_root, new_selector_ids);
  } else if (!frame_depth_.contains(dom_root)) {
    // We haven't injected yet
    wait_time_remaining_ms_[dom_root] =
        std::max(1, static_cast<int>(MaxTimeRemaining().InMilliseconds()));
    VLOG(2) << "Injecting into new frame";
    ResolveObjectIdAndInjectFrame(dom_root, frame_depth);
  }
}

void SelectorObserver::ResolveObjectIdAndInjectFrame(const DomRoot& dom_root,
                                                     size_t frame_depth) {
  DCHECK(!frame_depth_.contains(dom_root));

  frame_depth_.insert({dom_root, frame_depth});
  if (dom_root.should_use_main_doc()) {
    devtools_client_->GetRuntime()->Evaluate(
        std::string(kGetDocumentElement), dom_root.frame_id(),
        base::BindOnce(&SelectorObserver::OnGetDocumentElement,
                       weak_ptr_factory_.GetWeakPtr(), dom_root));
  } else {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetBackendNodeId(dom_root.root_backend_node_id())
            .Build(),
        dom_root.frame_id(),
        base::BindOnce(&SelectorObserver::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr(), dom_root));
  }
}

void SelectorObserver::OnGetDocumentElement(
    const DomRoot& dom_root,
    const DevtoolsClient::ReplyStatus& status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  if (state_ != State::RUNNING) {
    return;
  }
  // TODO(b/205676462): Investigate if this can fail if we try to get a frame's
  // document while the page is loading.
  if (!FailIfError<runtime::EvaluateResult>(status, result.get(), __FILE__,
                                            __LINE__)) {
    return;
  }
  std::string object_id;
  if (!GetObjectId(result->GetResult(), &object_id, __FILE__, __LINE__)) {
    return;
  }
  InjectFrame(dom_root, object_id);
}

void SelectorObserver::OnResolveNode(
    const DomRoot& dom_root,
    const DevtoolsClient::ReplyStatus& status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (state_ != State::RUNNING) {
    return;
  }
  if (!status.is_ok()) {
    FailWithError(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }
  std::string object_id;
  if (!GetObjectId(result->GetObject(), &object_id, __FILE__, __LINE__)) {
    return;
  }
  InjectFrame(dom_root, object_id);
}

void SelectorObserver::AddSelectorsToDomRoot(
    const DomRoot& dom_root,
    const std::vector<SelectorId>& selector_ids) {
  auto entry = script_api_object_ids_.find(dom_root);
  if (entry == script_api_object_ids_.end()) {
    FailWithError(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }

  std::string expr = BuildUpdateExpression(dom_root, selector_ids);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetFunctionDeclaration(expr)
          .SetReturnByValue(false)
          .SetObjectId(entry->second)
          .Build(),
      /* optional_node_frame_id= */ dom_root.frame_id(), base::DoNothing());
}

void SelectorObserver::InjectFrame(const DomRoot& dom_root,
                                   const std::string& object_id) {
  DCHECK(!script_api_object_ids_.contains(dom_root));
  DCHECK(frame_depth_.contains(dom_root));

  auto expr = BuildExpression(dom_root);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetFunctionDeclaration(expr)
          .SetReturnByValue(false)
          .SetObjectId(object_id)
          .Build(),
      /* optional_node_frame_id= */ dom_root.frame_id(),
      base::BindOnce(&SelectorObserver::OnInjectFrame,
                     weak_ptr_factory_.GetWeakPtr(), dom_root));
}

void SelectorObserver::OnInjectFrame(
    const DomRoot& dom_root,
    const MessageDispatcher::ReplyStatus& status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::string object_id;
  if (state_ != State::RUNNING) {
    return;
  }
  if (!FailIfError<runtime::CallFunctionOnResult>(status, result.get(),
                                                  __FILE__, __LINE__)) {
    return;
  }
  if (!GetObjectId(result->GetResult(), &object_id, __FILE__, __LINE__)) {
    return;
  }
  script_api_object_ids_.emplace(dom_root, object_id);

  AwaitChanges(dom_root);
}

void SelectorObserver::AwaitChanges(const DomRoot& dom_root) {
  auto status = CallSelectorObserverScriptApi(
      dom_root, "getChanges",
      std::move(runtime::CallFunctionOnParams::Builder()
                    .SetAwaitPromise(true)
                    .SetReturnByValue(true)),
      base::BindOnce(&SelectorObserver::OnHasChanges,
                     weak_ptr_factory_.GetWeakPtr(), dom_root));
  if (!status.ok()) {
    FailWithError(status);
  }
}

void SelectorObserver::OnHasChanges(
    const DomRoot& dom_root,
    const MessageDispatcher::ReplyStatus& status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (state_ != State::RUNNING) {
    return;
  }
  if (!FailIfError<runtime::CallFunctionOnResult>(status, result.get(),
                                                  __FILE__, __LINE__)) {
    return;
  }
  DCHECK(result->GetResult()->HasValue());

  const base::Value* value = result->GetResult()->GetValue();
  VLOG(2) << "OnHasChanges: " << value->DebugString();
  DCHECK(value->is_dict());
  std::string response_status = value->FindKey("status")->GetString();
  if (response_status == "listenerSuperseded") {
    return;
  } else if (response_status == "pageUnload") {
    OnFrameUnloaded(dom_root);
    return;
  } else if (response_status == "timeout") {
    wait_time_remaining_ms_[dom_root] = 0;
    CheckTimeout();
    return;
  }
  DCHECK_EQ(response_status, "update");

  int wait_time_remaining = value->FindKey("waitTimeRemaining")->GetInt();
  wait_time_remaining_ms_[dom_root] = wait_time_remaining;
  const base::Value* updates_val = value->FindKey("updates");
  DCHECK(updates_val->is_list());
  auto update_list = updates_val->GetListDeprecated();
  if (update_list.size() == 0) {
    AwaitChanges(dom_root);
    return;
  }

  bool invalidated_frames = false;
  std::vector<Update> updates;
  base::flat_map<int, std::vector<SelectorId>> frames_to_inject;
  for (const auto& entry : update_list) {
    SelectorId selector_id(entry.FindKey("selectorId")->GetInt());
    auto element_id = entry.FindKey("elementId")->GetIfInt();
    bool match = element_id.has_value();
    bool is_leaf_frame = entry.FindKey("isLeafFrame")->GetBool();
    if (is_leaf_frame || !match) {
      Update& update = updates.emplace_back();
      update.selector_id = selector_id;
      update.element_id = element_id.has_value() ? element_id.value() : -1;
      update.match = match;
      if (!is_leaf_frame) {  // No match in a non-leaf frame
        InvalidateDeeperFrames(selector_id, dom_root);
        invalidated_frames = true;
      }
    } else {  // Match in a non-leaf frame
      frames_to_inject[element_id.value()].emplace_back(selector_id);
    }
  }
  if (invalidated_frames) {
    TerminateUnneededDomRoots();
  }
  if (frames_to_inject.size()) {
    std::vector<int> element_ids;
    for (const auto& entry : frames_to_inject) {
      element_ids.push_back(entry.first);
    }
    pending_frame_injects_ += element_ids.size();
    GetElementsByElementId(
        dom_root, element_ids,
        base::BindOnce(&SelectorObserver::OnGetFramesObjectIds,
                       weak_ptr_factory_.GetWeakPtr(), dom_root,
                       frames_to_inject));
  }
  if (updates.size()) {
    // Callbacks can delete `this`. The callback should call Continue() if
    // needed.
    update_callback_.Run(OkClientStatus(), updates, this);
  } else {
    AwaitChanges(dom_root);
  }
}

void SelectorObserver::OnGetFramesObjectIds(
    const DomRoot& dom_root,
    const base::flat_map<int, std::vector<SelectorId>>& frames_to_inject,
    const base::flat_map<int, std::string>& element_object_ids) {
  if (state_ != State::RUNNING) {
    return;
  }
  for (auto& entry : frames_to_inject) {
    auto object_id_it = element_object_ids.find(entry.first);
    if (object_id_it == element_object_ids.end()) {
      NOTREACHED();
      FailWithError(UnexpectedErrorStatus(__FILE__, __LINE__));
      return;
    }
    InjectOrAddSelectorsByParent(/*parent*/ dom_root, object_id_it->second,
                                 entry.second);
  }
}

void SelectorObserver::GetElementsByElementId(
    const DomRoot& dom_root,
    const std::vector<int>& element_ids,
    base::OnceCallback<void(const base::flat_map<int, std::string>&)>
        callback) {
  auto element_ids_list =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  for (int id : element_ids) {
    DCHECK(id >= 0);
    element_ids_list->Append(id);
  }
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(std::move(element_ids_list))
                             .Build());
  auto status = CallSelectorObserverScriptApi(
      dom_root, "getElements",
      std::move(runtime::CallFunctionOnParams::Builder()
                    .SetArguments(std::move(arguments))
                    .SetReturnByValue(false)),
      base::BindOnce(&SelectorObserver::OnGetElementsByElementIdResult,
                     weak_ptr_factory_.GetWeakPtr(), dom_root,
                     std::move(callback)));
  if (!status.ok()) {
    FailWithError(status);
  }
}

void SelectorObserver::OnGetElementsByElementIdResult(
    const DomRoot& dom_root,
    base::OnceCallback<void(const base::flat_map<int, std::string>&)> callback,
    const MessageDispatcher::ReplyStatus& status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (state_ != State::RUNNING && state_ != State::FETCHING_ELEMENTS) {
    return;
  }
  if (!FailIfError<runtime::CallFunctionOnResult>(status, result.get(),
                                                  __FILE__, __LINE__)) {
    return;
  }
  std::string object_id;
  if (!GetObjectId(result->GetResult(), &object_id, __FILE__, __LINE__)) {
    return;
  }
  devtools_client_->GetRuntime()->GetProperties(
      runtime::GetPropertiesParams::Builder()
          .SetOwnProperties(true)
          .SetObjectId(object_id)
          .Build(),
      dom_root.frame_id(),
      base::BindOnce(&SelectorObserver::CallGetElementsByIdCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SelectorObserver::CallGetElementsByIdCallback(
    base::OnceCallback<void(const base::flat_map<int, std::string>&)> callback,
    const MessageDispatcher::ReplyStatus& status,
    std::unique_ptr<runtime::GetPropertiesResult> result) {
  if (state_ != State::RUNNING && state_ != State::FETCHING_ELEMENTS) {
    return;
  }
  if (!FailIfError<runtime::GetPropertiesResult>(status, result.get(), __FILE__,
                                                 __LINE__)) {
    return;
  }
  std::vector<std::pair<int, std::string>> object_ids_vector;
  for (auto& prop : *result->GetResult()) {
    std::string prop_object_id;
    std::string name = prop->GetName();
    if (!GetObjectId(prop->GetValue(), &prop_object_id, __FILE__, __LINE__)) {
      return;
    }
    size_t id;
    if (base::StringToSizeT(name, &id)) {
      object_ids_vector.emplace_back(id, prop_object_id);
    } else {
      NOTREACHED();
    }
  }
  base::flat_map<int, std::string> element_object_ids(object_ids_vector);
  std::move(callback).Run(element_object_ids);
}

void SelectorObserver::OnFrameUnloaded(const DomRoot& dom_root) {
  auto frame_depth_entry = frame_depth_.find(dom_root);
  if (frame_depth_entry == frame_depth_.end())
    return;
  size_t frame_depth = frame_depth_entry->second;

  std::vector<SelectorId> affected_selector_ids;
  for (auto& entry : selectors_) {
    const SelectorId& selector_id = entry.first;
    auto dom_root_entry =
        dom_roots_.find(std::make_pair(selector_id, frame_depth));
    if (dom_root_entry == dom_roots_.end() ||
        dom_root_entry->second != dom_root)
      continue;
    // Remove the unloaded frame and it's descendants
    InvalidateDeeperFrames(selector_id, frame_depth);
    affected_selector_ids.push_back(selector_id);
  }

  if (frame_depth >= 1 && affected_selector_ids.size() > 0) {
    auto parent_it = dom_roots_.find(
        std::make_pair(affected_selector_ids[0], frame_depth - 1));
    if (parent_it == dom_roots_.end()) {
      // This can happen if a frame unloaded had frames of it's own.
      return;
    }
    // Re-add the parent frame selector to the parent frame. This causes us to
    // receive an update from the frame element.
    AddSelectorsToDomRoot(parent_it->second, affected_selector_ids);
  }
  TerminateUnneededDomRoots();
}

void SelectorObserver::InvalidateDeeperFrames(const SelectorId& selector_id,
                                              const DomRoot& dom_root) {
  auto frame_depth_entry = frame_depth_.find(dom_root);
  if (frame_depth_entry == frame_depth_.end())
    return;
  size_t frame_depth = frame_depth_entry->second;
  InvalidateDeeperFrames(selector_id, frame_depth + 1);
}

void SelectorObserver::InvalidateDeeperFrames(const SelectorId& selector_id,
                                              size_t frame_depth) {
  while (dom_roots_.erase(std::make_pair(selector_id, frame_depth)) == 1) {
    ++frame_depth;
  }
}

void SelectorObserver::TerminateUnneededDomRoots() {
  std::set<DomRoot> unused_dom_roots;
  for (const auto& entry : script_api_object_ids_) {
    unused_dom_roots.insert(entry.first);
  }
  for (const auto& entry : dom_roots_) {
    unused_dom_roots.erase(entry.second);
  }
  for (const DomRoot& dom_root : unused_dom_roots) {
    TerminateDomRoot(dom_root);
  }
}

void SelectorObserver::OnHardTimeout() {
  // This timeout is unexpected, means something went wrong along the way. End
  // with an error.
  FailWithError(ClientStatus(TIMED_OUT));
}

base::TimeDelta SelectorObserver::MaxTimeRemaining() const {
  int max = 0;
  for (const auto& entry : wait_time_remaining_ms_) {
    max = std::max(max, entry.second);
  }
  return base::Milliseconds(max);
}

void SelectorObserver::CheckTimeout() {
  if (pending_frame_injects_ == 0 && MaxTimeRemaining().is_zero()) {
    // We didn't didn't match the required condition in the allotted time. It
    // could be expected from the script perspective.
    FailWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  }
}

std::string SelectorObserver::BuildExpression(const DomRoot& dom_root) const {
  JsSnippet snippet;
  snippet.AddLine("(function selectorObserver() {");
  snippet.AddLine(
      {"const pollInterval = ",
       base::NumberToString(settings_.min_check_interval.InMilliseconds()),
       ";"});
  int max_wait_time = wait_time_remaining_ms_.at(dom_root);
  snippet.AddLine(
      {"const maxRuntime = ",
       base::NumberToString(base::saturated_cast<int>(
           (base::Milliseconds(max_wait_time) + settings_.extra_timeout)
               .InMilliseconds())),
       ";"});
  snippet.AddLine(
      {"const maxWaitTime = ", base::NumberToString(max_wait_time), ";"});
  snippet.AddLine(
      {"const debounceInterval = ",
       base::NumberToString(settings_.debounce_interval.InMilliseconds()),
       ";"});
  snippet.AddLine("const selectors = [");

  size_t depth = frame_depth_.at(dom_root);
  for (const auto& entry : selectors_) {
    auto frame_id_entry = dom_roots_.find(std::make_pair(entry.first, depth));
    if (frame_id_entry == dom_roots_.end() ||
        frame_id_entry->second != dom_root) {
      continue;
    }
    SelectorObserver::SerializeSelector(entry.second.proto, entry.first,
                                        entry.second.strict, depth, snippet);
  }
  snippet.AddLine("];");  // const selectors = [
  snippet.AddLine(selector_observer_script::kWaitForChangeScript);
  snippet.AddLine("})");  // (function selectorObserver() {

  return snippet.ToString();
}

std::string SelectorObserver::BuildUpdateExpression(
    const DomRoot& dom_root,
    const std::vector<SelectorId>& selector_ids) const {
  JsSnippet snippet;
  snippet.AddLine("(function selectorObserverUpdate() {");
  snippet.AddLine("const selectors = [");

  size_t depth = frame_depth_.at(dom_root);
  for (const auto& selector_id : selector_ids) {
    auto frame_id_entry = dom_roots_.find(std::make_pair(selector_id, depth));
    if (frame_id_entry == dom_roots_.end() ||
        frame_id_entry->second != dom_root) {
      NOTREACHED();
      continue;
    }
    auto entry = selectors_.at(selector_id);
    SelectorObserver::SerializeSelector(entry.proto, selector_id, entry.strict,
                                        depth, snippet);
  }
  snippet.AddLine("];");  // const selectors = [
  snippet.AddLine("this.addSelectors(selectors);");
  snippet.AddLine("})");  // (function selectorObserverUpdate() {

  return snippet.ToString();
}

void SelectorObserver::SerializeSelector(const SelectorProto& selector,
                                         const SelectorId& selector_id,
                                         bool strict,
                                         size_t frame_depth,
                                         JsSnippet& snippet) {
  JsFilterBuilder builder;
  int depth = frame_depth;
  for (const auto& filter : selector.filters()) {
    if (filter.filter_case() == SelectorProto::Filter::kEnterFrame) {
      depth--;
    } else if (depth == 0) {
      builder.AddFilter(filter);
    } else if (depth < 0) {
      break;
    }
  }
  bool is_leaf_frame = depth == 0;
  if (strict && is_leaf_frame) {
    builder.ClearResultsIfMoreThanOneResult();
  }

  snippet.AddLine("[");
  snippet.AddLine(builder.BuildFunction());

  base::Value metadata(base::Value::Type::DICTIONARY);
  metadata.SetIntKey("selectorId", selector_id.value());
  metadata.SetKey("args", std::move(*builder.BuildArgumentArray()));
  metadata.SetBoolKey("isLeafFrame", is_leaf_frame);
  std::string serialized_meta;
  base::JSONWriter::Write(metadata, &serialized_meta);
  snippet.AddLine({",", serialized_meta, "],"});
}
}  // namespace autofill_assistant
