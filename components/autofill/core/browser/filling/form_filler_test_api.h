// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_FILLER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_FILLER_TEST_API_H_

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/filling/form_filler.h"

namespace autofill {

// Exposes some testing operations for BrowserAutofillManager.
class FormFillerTestApi {
 public:
  explicit FormFillerTestApi(FormFiller* form_filler)
      : form_filler_(*form_filler) {}

  void set_limit_before_refill(base::TimeDelta limit) {
    form_filler_->limit_before_automatic_refill_ = limit;
    form_filler_->limit_before_programmatic_refill_ = limit;
  }

  // Returns whether there are `OnAfterFillForm()` tasks that
  // `FormFiller::Reset()` or the destruction of the `FormFiller` would cancel.
  bool has_pending_post_fill_tasks() const {
    return form_filler_->post_fill_weak_ptr_factory_.HasWeakPtrs();
  }

  // Runs tasks on the current sequence until no post-fill bookkeeping task
  // (`OnAfterFillForm()`) is pending anymore, including bookkeeping tasks that
  // are posted while doing so (e.g., by a deferred refill). Returns immediately
  // if none is pending. Unlike `RunUntilIdle()`, this stops as soon as no
  // bookkeeping task is pending; unlike `base::test::RunUntil()`, it never
  // advances MOCK_TIME.
  //
  // This relies on `post_fill_weak_ptr_factory_` being bound only to the posted
  // bookkeeping tasks (otherwise it would never return). Must not be called
  // from within a task.
  void RunPendingPostFillTasks() {
    base::WeakPtr<FormFiller> form_filler = form_filler_->GetWeakPtr();
    while (form_filler && has_pending_post_fill_tasks()) {
      base::RunLoop run_loop;
      // `FillOrPreviewForm()` posts to the same `SequencedTaskRunner`, so
      // `run_loop` quits after all bookkeeping tasks posted so far have run.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

 private:
  raw_ref<FormFiller> form_filler_;
};

inline FormFillerTestApi test_api(FormFiller& form_filler) {
  return FormFillerTestApi(&form_filler);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FORM_FILLER_TEST_API_H_
