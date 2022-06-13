// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CSS_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CSS_ELEMENT_FINDER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/base_element_finder.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_finder_result_type.h"
#include "components/autofill_assistant/browser/web/js_filter_builder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
struct GlobalRenderFrameHostId;
}  // namespace content

namespace autofill_assistant {
class DevtoolsClient;
class UserData;
class ElementFinderResult;

class CssElementFinder : public BaseElementFinder {
 public:
  CssElementFinder(content::WebContents* web_contents,
                   DevtoolsClient* devtools_client,
                   const UserData* user_data,
                   const ElementFinderResultType result_type,
                   const Selector& selector);
  ~CssElementFinder() override;

  CssElementFinder(const CssElementFinder&) = delete;
  CssElementFinder& operator=(const CssElementFinder&) = delete;

  void Start(const ElementFinderResult& start_element,
             BaseElementFinder::Callback callback) override;

  ElementFinderInfoProto GetLogInfo() const override;

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
  const ElementFinderResultType result_type_;
  const Selector selector_;
  BaseElementFinder::Callback callback_;

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
  content::GlobalRenderFrameHostId current_frame_global_id_;

  // The frame id to use to execute devtools Javascript calls within the
  // context of the frame. Might be empty if no frame id needs to be
  // specified.
  std::string current_frame_devtools_id_;

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

  base::WeakPtrFactory<CssElementFinder> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CSS_ELEMENT_FINDER_H_
