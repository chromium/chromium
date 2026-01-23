// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_fetch_batcher.h"

#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/not_fatal_until.h"
#import "base/task/task_runner.h"
#import "base/time/time.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace autofill {

namespace {

// Filters the `forms` by `form_name`. No op if `form_name` is nullopt.
// This filter is the equivalent of the form name filter
// in autofill:ExtractFormData(). Will still filter if the `form_name` string is
// empty as long as it is not nullopt.
std::optional<std::vector<FormData>> ApplyFormFilterIfNeeded(
    std::optional<std::u16string> form_name,
    std::optional<std::vector<FormData>> forms) {
  if (forms && form_name) {
    std::erase_if(*forms, [&](const FormData& form) {
      return form.name() != *form_name;
    });
  }
  return forms;
}

}  // namespace

FormFetchBatcher::FormFetchBatcher(id<AutofillDriverIOSBridge> bridge,
                                   base::WeakPtr<web::WebFrame> frame,
                                   base::TimeDelta batch_period)
    : bridge_(bridge), frame_(frame), batch_period_(batch_period) {}

FormFetchBatcher::~FormFetchBatcher() = default;

void FormFetchBatcher::PushRequestAndRun(
    FormFetchCompletion completion,
    std::optional<std::u16string> form_name_filter) {
  fetch_requests_.emplace_back(
      base::BindOnce(&ApplyFormFilterIfNeeded, std::move(form_name_filter))
          .Then(std::move(completion)));

  // Cancel the current task by invalidating the weak pointer of the scheduled
  // callback.
  weak_factory_.InvalidateWeakPtrs();

  batch_scheduled_ = true;

  // Run the batch immediately.
  Run();
}

void FormFetchBatcher::PushRequest(
    FormFetchCompletion completion,
    std::optional<std::u16string> form_name_filter) {
  fetch_requests_.emplace_back(
      base::BindOnce(&ApplyFormFilterIfNeeded, std::move(form_name_filter))
          .Then(std::move(completion)));

  if (!batch_scheduled_) {
    batch_scheduled_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FormFetchBatcher::Run, weak_factory_.GetWeakPtr()),
        batch_period_);
  }
}

void FormFetchBatcher::Run() {
  // There should be at least one fetch request in the batch when running it.
  CHECK_GT(fetch_requests_.size(), 0u);
  // Running the batch should only be done when there was an actual batch
  // scheduled.
  CHECK(batch_scheduled_);

  if (frame_) {
    auto completion =
        base::BindOnce(&FormFetchBatcher::Complete, weak_factory_.GetWeakPtr());
    [bridge_ fetchFormsFiltered:std::nullopt
                        inFrame:frame_.get()
              completionHandler:std::move(completion)];
  } else {
    // If the frame disappeared when we're about to run the batch, complete
    // immediately.
    Complete(std::nullopt);
  }
}

void FormFetchBatcher::Complete(
    std::optional<std::vector<autofill::FormData>> forms) {
  // Complete() should only be done when there are queued requests.
  CHECK_GT(fetch_requests_.size(), 0u);
  // Completing the batch should only be done when there was an actual batch
  // scheduled.
  CHECK(batch_scheduled_);

  base::UmaHistogramCounts100("Autofill.iOS.FormExtraction.ForScan.BatchSize",
                              fetch_requests_.size());

  // The batch is being completed so a new batch can be scheduled starting from
  // now which includes new requests pushed by the completion blocks that are
  // about to be completed here.
  batch_scheduled_ = false;

  // Complete the original requests. New requests may be pushed to
  // fetch_requests_ when completing the blocks here but this won't affect this
  // loop.
  for (auto& fetch_request : std::exchange(fetch_requests_, {})) {
    std::move(fetch_request).Run(forms);
  }
}

}  // namespace autofill
