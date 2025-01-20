// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_fetch_batcher.h"

#include "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
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
  if (!forms || !form_name) {
    return forms;
  }

  std::vector<FormData> filtered_forms;
  base::ranges::copy_if(
      *forms, std::back_inserter(filtered_forms),
      [&](const std::u16string& name) { return name == *form_name; },
      &FormData::name);
  return filtered_forms;
}
}  // namespace

FormFetchBatcher::FormFetchBatcher(id<AutofillDriverIOSBridge> bridge,
                                   base::WeakPtr<web::WebFrame> frame,
                                   base::TimeDelta batch_period)
    : bridge_(bridge), frame_(frame), batch_period_(batch_period) {}

FormFetchBatcher::~FormFetchBatcher() = default;

void FormFetchBatcher::PushRequestAndRun(
    FormFetchCompletion&& completion,
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
    FormFetchCompletion&& completion,
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
  CHECK_GT(fetch_requests_.size(), 0u, base::NotFatalUntil::M133);
  // Running the batch should only be done when there was an actual batch
  // scheduled.
  CHECK(batch_scheduled_);

  if (frame_) {
    auto completion =
        base::BindOnce(&FormFetchBatcher::Complete, weak_factory_.GetWeakPtr());
    [bridge_ fetchFormsFiltered:NO
                       withName:u""
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
  CHECK_GT(fetch_requests_.size(), 0u, base::NotFatalUntil::M133);
  // Completing the batch should only be done when there was an actual batch
  // scheduled.
  CHECK(batch_scheduled_);

  base::UmaHistogramCounts100("Autofill.iOS.FormExtraction.ForScan.BatchSize",
                              fetch_requests_.size());

  // The batch is being completed so a new batch can be scheduled starting from
  // now which includes new requests pushed by the completion blocks that are
  // about to be completed here.
  batch_scheduled_ = false;

  // Make a local copy of the queue of completion blocks so the vector cannot
  // be resized if one of the callback pushes a new request in the
  // `fetch_requests_` queue. Resizing the vector will invalidate the iteration
  // loop. Any new requests that were pushed when emptying the loop will be
  // pushed to the next scheduled batch. A new batch will be scheduled if
  // needed.
  std::vector<FormFetchCompletion> fetch_requests_copy =
      std::move(fetch_requests_);
  fetch_requests_ = std::vector<FormFetchCompletion>();

  // Complete the original requests. New requests may be pushed to
  // fetch_requests_ when completing the blocks here but this won't affect this
  // loop.
  for (auto& fetch_request : fetch_requests_copy) {
    std::move(fetch_request).Run(forms);
  }
}

}  // namespace autofill
