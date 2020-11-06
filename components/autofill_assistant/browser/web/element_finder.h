// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/js_snippets.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace autofill_assistant {
class DevtoolsClient;

// Worker class to find element(s) matching a selector.
class ElementFinder : public WebControllerWorker {
 public:
  enum ResultType {
    // Result.object_id contains the object ID of the single node that matched.
    // If there are no matches, status is ELEMENT_RESOLUTION_FAILED. If there
    // are more than one matches, status is TOO_MANY_ELEMENTS.
    kExactlyOneMatch = 0,

    // Result.object_id contains the object ID of one of the nodes that matched.
    // If there are no matches, status is ELEMENT_RESOLUTION_FAILED.
    kAnyMatch,

    // Result.object_id contains the object ID of an array containing all the
    // nodes
    // that matched. If there are no matches, status is
    // ELEMENT_RESOLUTION_FAILED.
    kMatchArray,
  };

  struct Result {
    Result();
    ~Result();
    Result(const Result&);

    // The render frame host contains the element.
    content::RenderFrameHost* container_frame_host = nullptr;

    // The object id of the element.
    std::string object_id;

    // The frame id to use to execute devtools Javascript calls within the
    // context of the frame. Might be empty if no frame id needs to be
    // specified.
    std::string node_frame_id;

    std::vector<Result> frame_stack;
  };

  // |web_contents| and |devtools_client| must be valid for the lifetime of the
  // instance.
  ElementFinder(content::WebContents* web_contents,
                DevtoolsClient* devtools_client,
                const Selector& selector,
                ResultType result_type);
  ~ElementFinder() override;

  using Callback =
      base::OnceCallback<void(const ClientStatus&, std::unique_ptr<Result>)>;

  // Finds the element and calls the callback.
  void Start(Callback callback_);

 private:
  // Helper for building JavaScript functions.
  //
  // TODO(b/155264465): extract this into a top-level class in its own file, so
  // it can be tested.
  class JsFilterBuilder {
   public:
    JsFilterBuilder();
    ~JsFilterBuilder();

    // Builds the argument list for the function.
    std::vector<std::unique_ptr<runtime::CallArgument>> BuildArgumentList()
        const;

    // Return the JavaScript function.
    std::string BuildFunction() const;

    // Adds a filter, if possible.
    bool AddFilter(const SelectorProto::Filter& filter);

   private:
    std::vector<std::string> arguments_;
    JsSnippet snippet_;
    bool defined_query_all_deduplicated_ = false;

    // A number that's increased by each call to DeclareVariable() to make sure
    // we generate unique variables.
    int variable_counter_ = 0;

    // Adds a regexp filter.
    void AddRegexpFilter(const SelectorProto::TextFilter& filter,
                         const std::string& property);

    // Declares and initializes a variable containing a RegExp object that
    // correspond to |filter| and returns the variable name.
    std::string AddRegexpInstance(const SelectorProto::TextFilter& filter);

    // Returns the name of a new unique variable.
    std::string DeclareVariable();

    // Adds an argument to the argument list and returns its JavaScript
    // representation.
    //
    // This allows passing strings to the JavaScript code without having to
    // hardcode and escape them - this helps avoid XSS issues.
    std::string AddArgument(const std::string& value);

    // Adds a line of JavaScript code to the function, between the header and
    // footer. At that point, the variable "elements" contains the current set
    // of matches, as an array of nodes. It should be updated to contain the new
    // set of matches.
    //
    // IMPORTANT: Only pass strings that originate from hardcoded strings to
    // this method.
    void AddLine(const std::string& line) { snippet_.AddLine(line); }

    // Adds a line of JavaScript code to the function that's made up of multiple
    // parts to be concatenated together.
    //
    // IMPORTANT: Only pass strings that originate from hardcoded strings to
    // this method.
    void AddLine(const std::vector<std::string>& line) {
      snippet_.AddLine(line);
    }

    // Define a |queryAllDeduplicated(roots, selector)| JS function that calls
    // querySelectorAll(selector) on all |roots| (in order) and returns a
    // deduplicated list of the matching elements.
    // Calling this function a second time does not do anything; the function
    // will be defined only once.
    void DefineQueryAllDeduplicated();
  };

  // Finds the element, starting at |frame| and calls |callback|.
  //
  // |document_object_id| might be empty, in which case we first look for the
  // frame's document.
  void StartInternal(Callback callback,
                     content::RenderFrameHost* frame,
                     const std::string& frame_id,
                     const std::string& document_object_id);

  // Sends a result with the given status and no data.
  void SendResult(const ClientStatus& status);

  // Builds a result from the current state of the finder and returns it.
  void SendSuccessResult(const std::string& object_id);

  // Report |object_id| as result in |result| and initialize the frame-related
  // fields of |result| from the current state. Leaves the frame stack empty.
  Result BuildResult(const std::string& object_id);

  // Figures out what to do next given the current state.
  //
  // Most background operations in this worker end by updating the state and
  // calling ExecuteNextTask() again either directly or through Report*().
  void ExecuteNextTask();

  // Prepare a batch of |n| tasks that are sent at the same time to compute one
  // or more matching elements.
  //
  // After calling this, Report*(i, ...) should be called *exactly once* for all
  // 0 <= i < n to report the tasks results.
  //
  // Once all tasks reported their result, the object ID of all matching
  // elements will be added to |current_matches_| and ExecuteNextTask() will be
  // called.
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
  // If there are too many or too few matches, this function sends an error and
  // returns false.
  //
  // If this returns true, continue processing. If this returns false, return
  // from ExecuteNextTask(). ExecuteNextTask() will be called again once the
  // required data is available.
  bool ConsumeOneMatchOrFail(std::string& object_id_out);

  // Make sure there's at least |index + 1| matches, take the one at that index
  // and put it in |object_id_out|, then return true.
  //
  // If there are not enough matches, send an error response and return false.
  bool ConsumeMatchAtOrFail(size_t index, std::string& object_id_out);

  // Make sure there's at least one match and move them all into
  // |matches_out|.
  //
  // If there are no matches, send an error response and return false.
  // If there are not enough matches yet, fetch them in the background and
  // return false. This calls ExecuteNextTask() once matches have been fetched.
  //
  // If this returns true, continue processing. If this returns false, return
  // from ExecuteNextTask(). ExecuteNextTask() will be called again once the
  // required data is available.
  bool ConsumeAllMatchesOrFail(std::vector<std::string>& matches_out);

  // Make sure there's at least one match and move them all into a single array.
  //
  // If there are no matches, call SendResult() and return false.
  //
  // If there are matches, return false directly and move the matches into
  // an JS array in the background. ExecuteNextTask() is called again
  // once the background tasks have executed, and calling this will return true
  // and write the JS array id to |array_object_id_out|.
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
  void OnApplyJsFilters(size_t task_id,
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
  void OnDescribeNodeForFrame(const std::string& object_id,
                              const DevtoolsClient::ReplyStatus& reply_status,
                              std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNode(const DevtoolsClient::ReplyStatus& reply_status,
                     std::unique_ptr<dom::ResolveNodeResult> result);
  content::RenderFrameHost* FindCorrespondingRenderFrameHost(
      std::string frame_id);

  // Handle TaskType::PROXIMITY
  void ApplyProximityFilter(int filter_index,
                            const std::string& array_object_id);
  void OnProximityFilterTarget(int filter_index,
                               const std::string& array_object_id,
                               const ClientStatus& status,
                               std::unique_ptr<Result> result);
  void OnProximityFilterJs(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  // Fill |current_matches_js_array_| with the values in |current_matches_|
  // starting from |index|, then clear |current_matches_| and call
  // ExecuteNextTask().
  void MoveMatchesToJSArrayRecursive(size_t index);

  void OnMoveMatchesToJSArrayRecursive(
      size_t index,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  content::WebContents* const web_contents_;
  DevtoolsClient* const devtools_client_;
  const Selector selector_;
  const ResultType result_type_;
  Callback callback_;

  // The index of the next filter to process, in selector_.proto.filters.
  int next_filter_index_ = 0;

  // Pointer to the current frame
  content::RenderFrameHost* current_frame_ = nullptr;

  // The frame id to use to execute devtools Javascript calls within the
  // context of the frame. Might be empty if no frame id needs to be
  // specified.
  std::string current_frame_id_;

  // Object ID of the root of |current_frame_|.
  std::string current_frame_root_;

  // Object IDs of the current set matching elements. Cleared once it's used to
  // query or filter.
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

  std::vector<Result> frame_stack_;

  // Finder for the target of the current proximity filter.
  std::unique_ptr<ElementFinder> proximity_target_filter_;

  base::WeakPtrFactory<ElementFinder> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
