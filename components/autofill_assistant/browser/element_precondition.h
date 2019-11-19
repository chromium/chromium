// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_PRECONDITION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_PRECONDITION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class BatchElementChecker;
struct Selector;

class ElementPrecondition {
 public:
  ElementPrecondition(
      const google::protobuf::RepeatedPtrField<ElementReferenceProto>&
          element_exists,
      const google::protobuf::RepeatedPtrField<FormValueMatchProto>&
          form_value_match);
  ~ElementPrecondition();

  // Check whether the conditions satisfied and return the result through
  // |callback|. |batch_checks| must remain valid until the callback is run.
  //
  // Calling Check() while another check is in progress cancels the previously
  // running check.
  void Check(BatchElementChecker* batch_checks,
             base::OnceCallback<void(bool)> callback);

  bool empty() { return elements_exist_.empty() && form_value_match_.empty(); }

 private:
  void OnCheckElementExists(const ClientStatus& element_status);
  void OnGetFieldValue(int index,
                       const ClientStatus& element_status,
                       const std::string& value);
  void ReportCheckResult(bool success);

  std::vector<Selector> elements_exist_;
  std::vector<FormValueMatchProto> form_value_match_;

  // Number of checks for which there's still no result.
  int pending_check_count_;

  base::OnceCallback<void(bool)> callback_;

  base::WeakPtrFactory<ElementPrecondition> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ElementPrecondition);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_PRECONDITION_H_
