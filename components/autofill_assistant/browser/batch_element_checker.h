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

namespace autofill_assistant {
class WebController;

// Types of element checks.
enum ElementCheckType { kExistenceCheck, kVisibilityCheck };

// Helper for checking a set of elements at the same time. It avoids duplicate
// checks and supports retries.
//
// Single check:
//
// The simplest way of using a BatchElementChecker is to:
// - create an instance, using WebController::CreateBatchElementChecker or
//   ActionDelegate::CreateBatchElementChecker
// - call AddElementCheck() and AddFieldValueCheck()
// - call Run() with duration set to 0.
//
// The result of the checks is reported to the callbacks passed to
// AddElementCheck(kExistenceCheck, ) and AddFieldValueCheck(), then the
// callback passed to Run() is called, to report the end of the a run.
//
// Check with retries:
//
// To check for existence more than once, call Run() with a duration that
// specifies how long you're willing to wait. In that mode, elements that are
// found are reported immediately. Elements that are not found are reported at
// the end, once the specified deadline has passed, just before giving up and
// calling the callback passed to Run().
class BatchElementChecker {
 public:
  explicit BatchElementChecker(WebController* web_controller);
  virtual ~BatchElementChecker();

  // Callback for AddElementCheck. Argument is true if the check passed.
  using ElementCheckCallback = base::OnceCallback<void(bool)>;

  // Callback for AddFieldValueCheck. Argument is true is the element exists.
  // The string contains the field value, or an empty string if accessing the
  // value failed.
  using GetFieldValueCallback =
      base::OnceCallback<void(bool, const std::string&)>;

  // Checks an an element.
  //
  // kElementCheck checks whether at least one element given by |selectors|
  // exists on the web page.
  //
  // kVisibilityCheck checks whether at least one element given by |selectors|
  // is visible on the page.
  //
  // New element checks cannot be added once Run has been called.
  void AddElementCheck(ElementCheckType check_type,
                       const std::vector<std::string>& selectors,
                       ElementCheckCallback callback);

  // Gets the value of |selectors| and return the result through |callback|. The
  // returned value will be the empty string in case of error or empty value.
  //
  // New field checks cannot be added once Run has been called.
  void AddFieldValueCheck(const std::vector<std::string>& selectors,
                          GetFieldValueCallback callback);

  // Runs the checks until all elements exist or for |duration|, whichever one
  // comes first. Elements found are reported as soon as they're founds.
  // Elements not found are reported right before |all_done| is run.
  //
  // |duration| can be 0. In this case the checks are run once, without waiting.
  // |try_done| is run at the end of each try.
  void Run(const base::TimeDelta& duration,
           base::RepeatingCallback<void()> try_done,
           base::OnceCallback<void()> all_done);

  // Makes any pending Run stop after the end of the current try.
  void StopTrying() { stopped_ = true; }

  // Returns true if all element that were asked for have been found. Can be
  // called while Run is progress or afterwards.
  bool all_found() { return all_found_; }

 private:
  // Tries running the checks, reporting only successes.
  //
  // Calls |try_done_callback| at the end of the run.
  void Try(base::OnceCallback<void()> try_done_callback);

  void OnTryDone(int64_t remaining_attempts,
                 base::RepeatingCallback<void()> try_done,
                 base::OnceCallback<void()> all_done);

  // If there are still callbacks not called by a previous call to Try, call
  // them now. When this method returns, all callbacks are guaranteed to have
  // been run.
  void GiveUp();

  void OnElementChecked(std::vector<ElementCheckCallback>* callbacks,
                        bool exists);
  void OnGetFieldValue(std::vector<GetFieldValueCallback>* callbacks,
                       bool exists,
                       const std::string& value);
  void CheckTryDone();
  void RunCallbacks(std::vector<ElementCheckCallback>* callbacks, bool result);
  void RunCallbacks(std::vector<GetFieldValueCallback>* callbacks,
                    bool result,
                    const std::string& value);
  bool HasMoreChecksToRun();

  WebController* const web_controller_;

  // A map of ElementCheck arguments (check_type, selectors) to callbacks that
  // take the result of the check.
  std::map<std::pair<ElementCheckType, std::vector<std::string>>,
           std::vector<ElementCheckCallback>>
      element_check_callbacks_;

  // A map of GetFieldValue arguments (selectors) to callbacks that take the
  // field value.
  std::map<std::vector<std::string>, std::vector<GetFieldValueCallback>>
      get_field_value_callbacks_;
  int pending_checks_count_;
  bool all_found_;
  bool stopped_;

  // The callback built for Try(). It is kept around while going through the
  // maps and called once all selectors in that map have been looked up once,
  // whether that lookup was successful or not. Also used to guarantee that only
  // one Try runs at a time.
  base::OnceCallback<void()> try_done_callback_;

  base::WeakPtrFactory<BatchElementChecker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BatchElementChecker);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_
