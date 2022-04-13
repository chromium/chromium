// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/js_filter_builder.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/autofill_assistant/content/common/autofill_assistant_agent.mojom.h"
#include "components/autofill_assistant/content/common/node_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
class RenderFrameHost;
struct GlobalRenderFrameHostId;
}  // namespace content

namespace autofill_assistant {
class DevtoolsClient;
class UserData;

// ElementFinderResult is the fully resolved element that can be used without
// limitations. This means that |render_frame_host()| has been found and is not
// nullptr.
class ElementFinderResult {
 public:
  ElementFinderResult();
  ~ElementFinderResult();
  ElementFinderResult(const ElementFinderResult&);

  // Create an instance that is deemed to be empty. This can be used for
  // optional Elements (e.g. optional an frame).
  static ElementFinderResult EmptyResult();

  const DomObjectFrameStack& dom_object() const { return dom_object_; }

  content::RenderFrameHost* render_frame_host() const {
    if (!render_frame_id_) {
      return nullptr;
    }
    return content::RenderFrameHost::FromID(*render_frame_id_);
  }

  const std::string& object_id() const {
    return dom_object_.object_data.object_id;
  }

  const std::string& node_frame_id() const {
    return dom_object_.object_data.node_frame_id;
  }

  const std::vector<JsObjectIdentifier>& frame_stack() const {
    return dom_object_.frame_stack;
  }

  bool IsEmpty() const {
    return object_id().empty() && node_frame_id().empty();
  }

  void SetRenderFrameHost(content::RenderFrameHost* render_frame_host) {
    if (!render_frame_host) {
      return;
    }
    render_frame_id_ = render_frame_host->GetGlobalId();
  }

  void SetObjectId(const std::string& object_id) {
    dom_object_.object_data.object_id = object_id;
  }

  void SetNodeFrameId(const std::string& node_frame_id) {
    dom_object_.object_data.node_frame_id = node_frame_id;
  }

  void SetFrameStack(const std::vector<JsObjectIdentifier>& frame_stack) {
    dom_object_.frame_stack = frame_stack;
  }

 private:
  DomObjectFrameStack dom_object_;

  // The id of the render frame host that contains the element.
  absl::optional<content::GlobalRenderFrameHostId> render_frame_id_;
};

// Worker class to find element(s) matching a selector. This will keep entering
// iFrames until the element is found in the last frame, then returns the
// element together with the owning frame. All subsequent operations should
// be performed on that frame.
class ElementFinder : public WebControllerWorker {
 public:
  enum ResultType {
    // ElementFinderResult.object_id contains the object ID of the single node
    // that matched.
    // If there are no matches, status is ELEMENT_RESOLUTION_FAILED. If there
    // are more than one matches, status is TOO_MANY_ELEMENTS.
    kExactlyOneMatch = 0,

    // ElementFinderResult.object_id contains the object ID of one of the nodes
    // that matched.
    // If there are no matches, status is ELEMENT_RESOLUTION_FAILED.
    kAnyMatch,

    // ElementFinderResult.object_id contains the object ID of an array
    // containing all the
    // nodes
    // that matched. If there are no matches, status is
    // ELEMENT_RESOLUTION_FAILED.
    kMatchArray,
  };

  // |web_contents|, |devtools_client| and |user_data| must be valid for the
  // lifetime of the instance. If |annotate_dom_model_service| is not nullptr,
  // must be valid for the lifetime of the instance.
  ElementFinder(content::WebContents* web_contents,
                DevtoolsClient* devtools_client,
                const UserData* user_data,
                ProcessedActionStatusDetailsProto* log_info,
                AnnotateDomModelService* annotate_dom_model_service,
                const Selector& selector,
                ResultType result_type);
  ~ElementFinder() override;

  using Callback =
      base::OnceCallback<void(const ClientStatus&,
                              std::unique_ptr<ElementFinderResult>)>;

  // Finds the element and calls the callback starting from the |start_element|.
  // If it is empty, it will start looking for the Document of the main frame.
  void Start(const ElementFinderResult& start_element, Callback callback);

 private:
  class ElementFinderBase {
   public:
    virtual ~ElementFinderBase();

    // Start looking for the element and return it through |callback| with
    // a status. If |start_element| is not empty, use it as a starting point
    // instead of starting from the main frame.
    virtual void Start(const ElementFinderResult& start_element,
                       Callback callback) = 0;

    // Get the log information for the last run. Should only be run after the
    // run has completed (i.e. |callback_| has been called).
    virtual ElementFinderInfoProto GetLogInfo() const = 0;

    // Returns the backend node id that was previously collected.
    virtual int GetBackendNodeId() const = 0;
  };

  class SemanticElementFinder : public ElementFinderBase {
   public:
    SemanticElementFinder(content::WebContents* web_contents,
                          DevtoolsClient* devtools_client,
                          AnnotateDomModelService* annotate_dom_model_service,
                          const Selector& selector);
    ~SemanticElementFinder() override;

    SemanticElementFinder(const SemanticElementFinder&) = delete;
    SemanticElementFinder& operator=(const SemanticElementFinder&) = delete;

    void Start(const ElementFinderResult& start_element,
               Callback callback) override;

    ElementFinderInfoProto GetLogInfo() const override;

    // Returns the backend node id of the first result (if any), or 0.
    int GetBackendNodeId() const override;

   private:
    // Returns the given status and no element. This expects an error status.
    void GiveUpWithError(const ClientStatus& status);

    // Builds a result from the |render_frame_host| and the |object_id| and
    // returns it withan ok status.
    void ResultFound(content::RenderFrameHost* render_frame_host,
                     const std::string& object_id);

    // Call |callback_| with the |status| and |result|.
    void SendResult(const ClientStatus& status,
                    const ElementFinderResult& result);

    // Run the model annotation on all frames for the current |start_frame|.
    void RunAnnotateDomModel(content::RenderFrameHost* start_frame);

    // Runs the model on the frame identified by |host_id|.
    void RunAnnotateDomModelOnFrame(
        const content::GlobalRenderFrameHostId& host_id,
        base::OnceCallback<void(std::vector<GlobalBackendNodeId>)> callback);
    void OnRunAnnotateDomModelOnFrame(
        const content::GlobalRenderFrameHostId& host_id,
        base::OnceCallback<void(std::vector<GlobalBackendNodeId>)> callback,
        mojom::NodeDataStatus status,
        const std::vector<NodeData>& node_data);

    // Called once the model has been run on all frames.
    void OnRunAnnotateDomModel(
        const std::vector<std::vector<GlobalBackendNodeId>>& all_nodes);

    void OnResolveNodeForAnnotateDom(
        content::GlobalRenderFrameHostId host_id,
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<dom::ResolveNodeResult> result);

    const raw_ptr<content::WebContents> web_contents_;
    const raw_ptr<DevtoolsClient> devtools_client_;
    const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;
    const Selector selector_;
    Callback callback_;

    // Elements gathered through all frames. Unused if the |selector_| does not
    // contain |SemanticInformation|.
    std::vector<GlobalBackendNodeId> semantic_node_results_;
    std::vector<mojom::NodeDataStatus> node_data_frame_status_;

    base::WeakPtrFactory<SemanticElementFinder> weak_ptr_factory_{this};
  };

  class CssElementFinder : public ElementFinderBase {
   public:
    CssElementFinder(content::WebContents* web_contents,
                     DevtoolsClient* devtools_client,
                     const UserData* user_data,
                     const ResultType result_type,
                     const Selector& selector);
    ~CssElementFinder() override;

    CssElementFinder(const CssElementFinder&) = delete;
    CssElementFinder& operator=(const CssElementFinder&) = delete;

    void Start(const ElementFinderResult& start_element,
               Callback callback) override;

    ElementFinderInfoProto GetLogInfo() const override;

    // Returns the backend node id of the result if the proto contains
    // |semantic_information|, or 0.
    int GetBackendNodeId() const override;

   private:
    // Returns the given status and no element. This expects an error status.
    void GiveUpWithError(const ClientStatus& status);

    // Found a valid result.
    void ResultFound(const std::string& object_id);

    // Builds a result from the current state of the finder and returns it with
    // an ok status.
    void BuildAndSendResult(const std::string& object_id);

    // Call |callback_| with the |status| and |result|.
    void SendResult(const ClientStatus& status,
                    const ElementFinderResult& result);

    // Figures out what to do next given the current state.
    //
    // Most background operations in this worker end by updating the state and
    // calling ExecuteNextTask() again either directly or through Report*().
    void ExecuteNextTask();

    // Prepare a batch of |n| tasks that are sent at the same time to compute
    // one or more matching elements.
    //
    // After calling this, Report*(i, ...) should be called *exactly once* for
    // all 0 <= i < n to report the tasks results.
    //
    // Once all tasks reported their result, the object ID of all matching
    // elements will be added to |current_matches_| and ExecuteNextTask() will
    // be called.
    void PrepareBatchTasks(int n);

    // Report that task with ID |task_id| didn't match any element.
    void ReportNoMatchingElement(size_t task_id);

    // Report that task with ID |task_id| matched a single element with ID
    // |object_id|.
    void ReportMatchingElement(size_t task_id, const std::string& object_id);

    // Report that task with ID |task_id| matched multiple elements that are
    // stored in the JS array with ID |object_id|.
    void ReportMatchingElementsArray(size_t task_id,
                                     const std::string& array_object_id);
    void ReportMatchingElementsArrayRecursive(
        size_t task_id,
        const std::string& array_object_id,
        std::unique_ptr<std::vector<std::string>> acc,
        int index);
    void OnReportMatchingElementsArrayRecursive(
        size_t task_id,
        const std::string& array_object_id,
        std::unique_ptr<std::vector<std::string>> acc,
        int index,
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<runtime::CallFunctionOnResult> result);

    // If all batch tasks reported their result, add all tasks results to
    // |current_matches_| then call ExecuteNextTask().
    void MaybeFinalizeBatchTasks();

    // Make sure there's exactly one match, set it |object_id_out| then return
    // true.
    //
    // If there are too many or too few matches, this function sends an error
    // and returns false.
    //
    // If this returns true, continue processing. If this returns false, return
    // from ExecuteNextTask(). ExecuteNextTask() will be called again once the
    // required data is available.
    bool ConsumeOneMatchOrFail(std::string& object_id_out);

    // Make sure there's at least |index + 1| matches, take the one at that
    // index and put it in |object_id_out|, then return true.
    //
    // If there are not enough matches, send an error response and return false.
    bool ConsumeMatchAtOrFail(size_t index, std::string& object_id_out);

    // Make sure there's at least one match and move them all into
    // |matches_out|.
    //
    // If there are no matches, send an error response and return false.
    // If there are not enough matches yet, fetch them in the background and
    // return false. This calls ExecuteNextTask() once matches have been
    // fetched.
    //
    // If this returns true, continue processing. If this returns false, return
    // from ExecuteNextTask(). ExecuteNextTask() will be called again once the
    // required data is available.
    bool ConsumeAllMatchesOrFail(std::vector<std::string>& matches_out);

    // Make sure there's at least one match and move them all into a single
    // array.
    //
    // If there are no matches, call SendResult() and return false.
    //
    // If there are matches, return false directly and move the matches into
    // an JS array in the background. ExecuteNextTask() is called again
    // once the background tasks have executed, and calling this will return
    // true and write the JS array id to |array_object_id_out|.
    bool ConsumeMatchArrayOrFail(std::string& array_object_id_out);

    void OnConsumeMatchArray(
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<runtime::CallFunctionOnResult> result);

    // Gets a document element from the current frame and us it as root for the
    // rest of the tasks, then call ExecuteNextTask().
    void GetDocumentElement();
    void OnGetDocumentElement(const DevtoolsClient::ReplyStatus& reply_status,
                              std::unique_ptr<runtime::EvaluateResult> result);

    // Handle Javascript filters
    void ApplyJsFilters(const JsFilterBuilder& builder,
                        const std::vector<std::string>& object_ids);
    void OnApplyJsFilters(
        size_t task_id,
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<runtime::CallFunctionOnResult> result);

    // Handle PSEUDO_TYPE
    void ResolvePseudoElement(PseudoType pseudo_type,
                              const std::vector<std::string>& object_ids);
    void OnDescribeNodeForPseudoElement(
        dom::PseudoType pseudo_type,
        size_t task_id,
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<dom::DescribeNodeResult> result);
    void OnResolveNodeForPseudoElement(
        size_t task_id,
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<dom::ResolveNodeResult> result);

    // Handle ENTER_FRAME
    void EnterFrame(const std::string& object_id);
    void OnDescribeNodeForFrame(
        const std::string& object_id,
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<dom::DescribeNodeResult> result);
    void OnResolveNode(const DevtoolsClient::ReplyStatus& reply_status,
                       std::unique_ptr<dom::ResolveNodeResult> result);

    // Fill |current_matches_js_array_| with the values in |current_matches_|
    // starting from |index|, then clear |current_matches_| and call
    // ExecuteNextTask().
    void MoveMatchesToJSArrayRecursive(size_t index);

    void OnMoveMatchesToJSArrayRecursive(
        size_t index,
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<runtime::CallFunctionOnResult> result);

    void OnDescribeNodeForId(
        const std::string& object_id,
        const DevtoolsClient::ReplyStatus& reply_status,
        std::unique_ptr<dom::DescribeNodeResult> node_result);

    const raw_ptr<content::WebContents> web_contents_;
    const raw_ptr<DevtoolsClient> devtools_client_;
    const raw_ptr<const UserData> user_data_;
    const ResultType result_type_;
    const Selector selector_;
    Callback callback_;

    // The modified selector to use going forward. This is guaranteed to have
    // resolved any filters that need a data lookup.
    SelectorProto selector_proto_;

    // The index of the next filter to process, in selector__proto_.filters.
    int next_filter_index_ = 0;

    // Getting the document failed. Used for error reporting.
    bool get_document_failed_ = false;

    // The currently worked on filters are starting at this index..
    int current_filter_index_range_start_ = -1;

    // Pointer to the current frame
    raw_ptr<content::RenderFrameHost> current_frame_ = nullptr;

    // The frame id to use to execute devtools Javascript calls within the
    // context of the frame. Might be empty if no frame id needs to be
    // specified.
    std::string current_frame_id_;

    // Object IDs of the current set matching elements. Cleared once it's used
    // to query or filter.
    std::vector<std::string> current_matches_;

    // Object ID of the JavaScript array of the currently matching elements. In
    // practice, this is used by ConsumeMatchArrayOrFail() to convert
    // |current_matches_| to a JavaScript array.
    std::string current_matches_js_array_;

    // True if current_matches are pseudo-elements.
    bool matching_pseudo_elements_ = false;

    // The result of the background tasks. |tasks_results_[i]| contains the
    // elements matched by task i, or nullptr if the task is still running.
    std::vector<std::unique_ptr<std::vector<std::string>>> tasks_results_;

    std::vector<JsObjectIdentifier> frame_stack_;

    // The backend node id of the result. Only gets assigned if required, when
    // this will be used for a comparison with the result of a semantic run.
    absl::optional<int> backend_node_id_;

    // The client status of the last run.
    ClientStatus client_status_;

    // Finder for the target of the current proximity filter.
    std::unique_ptr<ElementFinder> proximity_target_filter_;

    base::WeakPtrFactory<CssElementFinder> weak_ptr_factory_{this};
  };

  // Updates |log_info_| and calls |callback_| with the |status| and |result|.
  void SendResult(const ClientStatus& status,
                  std::unique_ptr<ElementFinderResult> result);

  void UpdateLogInfo(const ClientStatus& status);

  // Adds a runner to the list and starts it from the |start_element|.
  void AddAndStartRunner(const ElementFinderResult& start_element,
                         std::unique_ptr<ElementFinderBase> runner);
  void OnResult(size_t index,
                const ClientStatus& status,
                std::unique_ptr<ElementFinderResult> result);

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<DevtoolsClient> devtools_client_;
  const raw_ptr<const UserData> user_data_;
  const raw_ptr<ProcessedActionStatusDetailsProto> log_info_;
  const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;
  const Selector selector_;
  const ResultType result_type_;
  Callback callback_;

  std::vector<std::unique_ptr<ElementFinderBase>> runners_;
  std::vector<std::pair<ClientStatus, std::unique_ptr<ElementFinderResult>>>
      results_;
  size_t num_results_ = 0;

  base::WeakPtrFactory<ElementFinder> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
