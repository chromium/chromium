// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SELECTOR_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SELECTOR_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/strong_alias.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/devtools/message_dispatcher.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"
#include "content/public/browser/web_contents.h"
#include "js_snippets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Class to observe selectors. It received a list of selectors and a callback
// that gets notified every time the match status of a selector changes. If a
// selector matches, the callback also receives an identifier that can be
// used to retrieve the matching DOM nodes the caller is interested in (Using
// GetElementsAndStop).
//
// It works by splitting the Selectors into subsequences of filters separated
// by EnterFrame filters and injecting a JavaScript script into the frames to
// observe (using MutationObserver) the status of this subsequences. When the
// match status or the matching element of the subsequences change, they send an
// to OnHasChanges(). OnHasChanges() then sends the updates to the callback if
// necessary.
//
// TODO(b/205676462): Implement pseudo elements.
class SelectorObserver : public WebControllerWorker {
 public:
  using SelectorId = base::StrongAlias<class SelectorIdTag, size_t>;
  // Used to request selectors to be observed.
  struct ObservableSelector {
    ObservableSelector(SelectorId selector_id,
                       const SelectorProto& proto,
                       bool strict);
    ~ObservableSelector();
    ObservableSelector(const ObservableSelector&);

    // Id to reference this selector in the updates. It's chosen by the  callers
    // and can be any string as long as it's unique.
    SelectorId selector_id;
    SelectorProto proto;
    // If true, match will fail if more that one element matches the selector.
    bool strict;
  };

  // An update to the match status of a selector.
  struct Update {
    Update();
    ~Update();
    Update(const Update&);

    // Selector identifier, same as the one in |ObservableSelector|.
    SelectorId selector_id;
    bool match;
    // Identifier for the specific DOM node that matched. This can be used to
    // fetch the element later.
    int element_id;
  };

  struct RequestedElement {
    RequestedElement(const SelectorId& selector_id, int element_id);
    ~RequestedElement();
    RequestedElement(const RequestedElement&);

    // The id of the selector passed to |ObservableSelector|.
    SelectorId selector_id;
    // An identifier for a matching DOM node. Callers get it from
    // an |Update| and copy it here if they want to fetch the element at the
    // end.
    int element_id;
  };

  // Settings to configure the selector observer.
  struct Settings {
    Settings(const base::TimeDelta& max_wait_time,
             const base::TimeDelta& min_check_interval,
             const base::TimeDelta& extra_timeout,
             const base::TimeDelta& debounce_interval);
    ~Settings();
    Settings(const Settings&);
    // Maximum amount of time it will wait for an element.
    const base::TimeDelta max_wait_time;
    // Selector checks will run at least this often, even if no DOM changes are
    // detected.
    const base::TimeDelta min_check_interval;
    // Extra wait time before assuming something has failed and giving up.
    const base::TimeDelta extra_timeout;
    // Wait until no DOM changes are received for this amount of time to check
    // the selectors. An interval of 0 effectively disables debouncing.
    const base::TimeDelta debounce_interval;
  };

  using Callback = base::RepeatingCallback<
      void(const ClientStatus&, const std::vector<Update>&, SelectorObserver*)>;

  // |content::WebContents| and |DevtoolsClient| need to outlive this instance.
  // |UserData| needs to exist until Start() is called.
  explicit SelectorObserver(const std::vector<ObservableSelector>& selectors,
                            const Settings& settings,
                            content::WebContents* web_contents,
                            DevtoolsClient* devtools_client,
                            const UserData* user_data,
                            Callback update_callback);

  ~SelectorObserver() override;

  // Calls the callbacks when the conditions (that can be tested using
  // javascript) match, or after |timeout_ms|. The DevtoolsClient needs to
  // outlive this instance.
  ClientStatus Start(base::OnceClosure finished_callback);

  // Continue watching for changes. Callbacks should call either Continue() or
  // GetElementsAndStop().
  void Continue();

  // Convert |element_ids| to DomObjectFrameStacks.
  void GetElementsAndStop(
      const std::vector<RequestedElement>& element_ids,
      base::OnceCallback<void(
          const ClientStatus&,
          const base::flat_map<SelectorId, DomObjectFrameStack>&)> callback);

 private:
  // A DomRoot is a root element of a document or a shadow DOM that we should
  // observe. frame_id is the devtools frame id (empty string to use the main
  // frame), root_backend_node_id is the backend_node_id of the root element to
  // observe. In the case of the main frame or a OOP frame, root_backend_node_id
  // can be set to kDomRootUseMainDoc, indicating to observe the document's root
  // element.
  struct DomRoot : public std::pair<std::string, int> {
    constexpr static int kUseMainDoc = -1;

    using std::pair<std::string, int>::pair;
    const std::string& frame_id() const { return first; }
    bool should_use_main_doc() const { return second == kUseMainDoc; }
    int root_backend_node_id() const {
      DCHECK(second != kUseMainDoc);
      return second;
    }
  };

  // Contains different identifiers about the frames we are observing.
  struct FrameIds {
    // The devtools object id of the iframe node that contains the frame. This
    // devtools id therefore belongs to the parent frame.
    std::string devtools_id;
    content::GlobalRenderFrameHostId global_frame_id;
  };
  enum class State {
    INITIALIZED = 0,
    RUNNING = 1,
    FETCHING_ELEMENTS = 2,
    TERMINATED = 3,
    ERROR_STATE = 4,
  };
  State state_ = State::INITIALIZED;
  const Settings settings_;
  base::TimeTicks started_;
  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  base::flat_map<SelectorId, ObservableSelector> selectors_;
  raw_ptr<DevtoolsClient> devtools_client_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<const UserData> user_data_;
  Callback update_callback_;
  base::OnceClosure finished_callback_;

  base::OnceCallback<void(
      const ClientStatus&,
      const base::flat_map<SelectorId, DomObjectFrameStack>&)>
      get_elements_callback_;
  int pending_frame_injects_ = 0;
  int pending_get_elements_responses_;
  base::flat_map<SelectorId, DomObjectFrameStack> get_elements_response_;

  // Selector observer script api object id for each DomRoot
  base::flat_map<DomRoot, std::string> script_api_object_ids_;

  // Devtools Object id's and render host global id's of containing iframes.
  base::flat_map<DomRoot, FrameIds> frame_ids_;

  // Dom root for each selector's stretch: {selector_id, frame_depth} -> DomRoot
  base::flat_map<std::pair<SelectorId, size_t>, DomRoot> dom_roots_;

  // How deep is a frame (root = 0). frame_id -> depth
  base::flat_map<DomRoot, size_t> frame_depth_;

  base::flat_map<DomRoot, int> wait_time_remaining_ms_;

  // Stop watching and free held resources.
  void Stop();

  void OnGetElementsResponse(
      const DomRoot&,
      const std::vector<RequestedElement>& elements,
      const base::flat_map<int, std::string>& object_ids);
  void MaybeFinishedGettingElements();

  void FailWithError(const ClientStatus&);
  template <typename T>
  bool FailIfError(const DevtoolsClient::ReplyStatus& status,
                   const T* result,
                   const char* file,
                   int line);
  bool GetObjectId(const runtime::RemoteObject* result,
                   std::string* out,
                   const char* file,
                   int line);
  void EnterState(State status);

  ClientStatus CallSelectorObserverScriptApi(
      const DomRoot&,
      const std::string& function,
      runtime::CallFunctionOnParams::CallFunctionOnParamsBuilder<0>&&
          param_builder,
      base::OnceCallback<void(const MessageDispatcher::ReplyStatus&,
                              std::unique_ptr<runtime::CallFunctionOnResult>)>
          callback);

  // Remove observers from this DomRoot
  void TerminateDomRoot(const DomRoot&);
  void OnTerminateDone(const MessageDispatcher::ReplyStatus& status,
                       std::unique_ptr<runtime::CallFunctionOnResult> result);

  void InjectFrame(const DomRoot&, const std::string& object_id);
  void OnInjectFrame(const DomRoot&,
                     const MessageDispatcher::ReplyStatus&,
                     std::unique_ptr<runtime::CallFunctionOnResult>);

  void InjectOrAddSelectorsByParent(
      const DomRoot& parent,
      const std::string& node_object_id,
      const std::vector<SelectorId>& selector_ids);
  void OnDescribeNodeDone(const DomRoot& parent,
                          const std::string& parent_object_id,
                          const std::vector<SelectorId>& selector_ids,
                          const MessageDispatcher::ReplyStatus&,
                          std::unique_ptr<dom::DescribeNodeResult> result);
  void InjectOrAddSelectorsToDomRoot(
      const DomRoot&,
      size_t frame_depth,
      const std::vector<SelectorId>& selector_ids);
  void ResolveObjectIdAndInjectFrame(const DomRoot&, size_t frame_depth);
  void InjectOrAddSelectorsToBackendId(
      const DomRoot& parent,
      int backend_id,
      const std::vector<SelectorId>& selector_ids);

  void OnGetDocumentElement(const DomRoot&,
                            const DevtoolsClient::ReplyStatus& status,
                            std::unique_ptr<runtime::EvaluateResult> result);

  void OnResolveNode(const DomRoot&,
                     const DevtoolsClient::ReplyStatus& status,
                     std::unique_ptr<dom::ResolveNodeResult> result);
  void OnResolveBackendId(const DevtoolsClient::ReplyStatus& reply_status,
                          std::unique_ptr<dom::ResolveNodeResult> result);
  void AddSelectorsToDomRoot(const DomRoot&,
                             const std::vector<SelectorId>& selector_ids);

  void AwaitChanges(const DomRoot&);

  // Receives and processes changes form JavacScript. Updates have the format
  // [{
  //    selectorId: string,
  //    isLeafFrame: bool,
  //    elementId: int
  // }]
  // |elementId| is unique within this DomRoot.
  // |isLeafFrame| is true if this is the last frame of the selector, i.e., if
  // true the referenced element is the element the selector is looking for,
  // otherwise it's a frame we need to inject into in order to find the final
  // element.
  void OnHasChanges(const DomRoot&,
                    const MessageDispatcher::ReplyStatus&,
                    std::unique_ptr<runtime::CallFunctionOnResult>);

  void OnGetFramesObjectIds(
      const DomRoot&,
      const base::flat_map<int, std::vector<SelectorId>>& frames_to_inject,
      const base::flat_map<int, std::string>& element_object_ids);
  void OnGetElements(
      const DomRoot&,
      const std::map<int, std::vector<std::string>> frames_to_inject,
      const MessageDispatcher::ReplyStatus&,
      std::unique_ptr<runtime::CallFunctionOnResult>);

  void GetElementsByElementId(
      const DomRoot&,
      const std::vector<int>& element_ids,
      base::OnceCallback<void(const base::flat_map<int, std::string>&)>
          callback);
  void OnGetElementsByElementIdResult(
      const DomRoot&,
      base::OnceCallback<void(const base::flat_map<int, std::string>&)>
          callback,
      const MessageDispatcher::ReplyStatus& status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void CallGetElementsByIdCallback(
      base::OnceCallback<void(const base::flat_map<int, std::string>&)>
          callback,
      const MessageDispatcher::ReplyStatus& status,
      std::unique_ptr<runtime::GetPropertiesResult> result);

  void OnFrameUnloaded(const DomRoot&);
  // Invalidates frames at `frame_depth` and deeper.
  void InvalidateDeeperFrames(const SelectorId& selector_id,
                              size_t frame_depth);
  // Invalidates frames after DomRoot, not including DomRoot.
  void InvalidateDeeperFrames(const SelectorId& selector_id, const DomRoot&);

  void TerminateUnneededDomRoots();
  void OnHardTimeout();
  void CheckTimeout();
  base::TimeDelta MaxTimeRemaining() const;

  std::string BuildExpression(const DomRoot&) const;
  std::string BuildUpdateExpression(
      const DomRoot&,
      const std::vector<SelectorId>& selector_ids) const;
  static void SerializeSelector(const SelectorProto& selector,
                                const SelectorId&,
                                bool strict,
                                size_t frame_depth,
                                JsSnippet& snippet);

  base::WeakPtrFactory<SelectorObserver> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SELECTOR_OBSERVER_H_
