// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_PRECONDITION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_PRECONDITION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
class BatchElementChecker;

class ElementPrecondition {
 public:
  // Callback being called after the checks are done with the success status,
  // the matching payloads and a set of matching element lookup results.
  using Callback = base::OnceCallback<void(
      const ClientStatus&,
      const std::vector<std::string>&,
      const base::flat_map<std::string, DomObjectFrameStack>&)>;

  ElementPrecondition(const ElementConditionProto& proto);
  ~ElementPrecondition();

  // Check whether the conditions are satisfied and return the result through
  // |callback|. |batch_checks| must remain valid until the callback is run.
  //
  // Calling Check() while another check is in progress cancels the previously
  // running check.
  //
  // The callback gets a status, which is ACTION_APPLIED if the overall
  // condition matched, the payloads of specific conditions that matched and
  // a representation of element results found during the checks.
  // Note that payloads and element results can still be sent out even though
  // the overall condition did not match.
  void Check(BatchElementChecker* batch_checks, Callback callback);

  bool empty() {
    return proto_.type_case() == ElementConditionProto::TYPE_NOT_SET;
  }

 private:
  // Selector that should be checked and the result of checking that selector.
  struct Result {
    Result();
    ~Result();
    Result(const Result&);

    Selector selector;
    bool match = false;

    // The identifier given to this result through the script. This identifier
    // can be used to later find the element in the |ElementStore|.
    base::Optional<std::string> client_id;
  };

  // Add selectors from |proto| to |results_|, doing a depth-first search.
  void AddResults(const ElementConditionProto& proto);

  void OnCheckElementExists(size_t result_index,
                            const ClientStatus& element_status,
                            const ElementFinder::Result& element_reference);

  void OnAllElementChecksDone(Callback callback);

  bool EvaluateResults(const ElementConditionProto& proto_,
                       size_t* next_result_index,
                       std::vector<std::string>* payloads);

  const ElementConditionProto proto_;

  // Maps ElementConditionProto.match from proto_ to result. Results appear in
  // the same order as in proto_, assuming a depth first search.
  std::vector<Result> results_;

  base::flat_map<std::string, DomObjectFrameStack> elements_;

  base::WeakPtrFactory<ElementPrecondition> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ElementPrecondition);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_PRECONDITION_H_
