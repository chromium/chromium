// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/selector.h"

namespace autofill_assistant {
class WebController;

// Helper for checking a set of elements at the same time. It avoids duplicate
// checks.
class BatchElementChecker {
 public:
  explicit BatchElementChecker();
  virtual ~BatchElementChecker();

  // Callback for AddElementCheck. Argument is true if the check passed.
  //
  // An ElementCheckCallback must not delete its calling BatchElementChecker.
  using ElementCheckCallback = base::OnceCallback<void(const ClientStatus&)>;

  // Callback for AddFieldValueCheck. Argument is true is the element exists.
  // The string contains the field value, or an empty string if accessing the
  // value failed.
  //
  // An ElementCheckCallback must not delete its calling BatchElementChecker.
  using GetFieldValueCallback =
      base::OnceCallback<void(const ClientStatus&, const std::string&)>;

  // Checks an element.
  //
  // New element checks cannot be added once Run has been called.
  void AddElementCheck(const Selector& selector, ElementCheckCallback callback);

  // Gets the value of |selector| and return the result through |callback|. The
  // returned value will be the empty string in case of error or empty value.
  //
  // New field checks cannot be added once Run has been called.
  void AddFieldValueCheck(const Selector& selector,
                          GetFieldValueCallback callback);

  // A callback to call once all the elements have been checked. These callbacks
  // are guaranteed to be called in order, finishing with the callback passed to
  // Run().
  //
  // These callback are allowed to delete the current instance.
  void AddAllDoneCallback(base::OnceCallback<void()> all_done);

  // Returns true if all there are no checks to run.
  bool empty() const;

  // Runs the checks. Once all checks are done, calls the callbacks registered
  // to AddAllDoneCallback().
  void Run(WebController* web_controller);

 private:
  void OnElementChecked(std::vector<ElementCheckCallback>* callbacks,
                        const ClientStatus& element_status);
  void OnGetFieldValue(std::vector<GetFieldValueCallback>* callbacks,
                       const ClientStatus& element_status,
                       const std::string& value);
  void CheckDone();

  // A map of ElementCheck arguments (check_type, selector) to callbacks that
  // take the result of the check.
  std::map<Selector, std::vector<ElementCheckCallback>>
      element_check_callbacks_;

  // A map of GetFieldValue arguments (selector) to callbacks that take the
  // field value.
  std::map<Selector, std::vector<GetFieldValueCallback>>
      get_field_value_callbacks_;
  int pending_checks_count_ = 0;

  // Run() was called. Checking elements might or might not have finished yet.
  bool started_ = false;

  std::vector<base::OnceCallback<void()>> all_done_;

  base::WeakPtrFactory<BatchElementChecker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BatchElementChecker);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_
