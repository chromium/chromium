// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_FETCH_BATCHER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_FETCH_BATCHER_H_

#import <optional>
#import <vector>

#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace autofill {

// Controller for recurrently doing batches of form extraction requests on a
// given frame. Can be safely deleted while there is an ongoing batch where the
// scheduled batch will simply be no op (the equivalent of canceling a task).
// Can only do unfiltered extractions where it gives back all forms that could
// be extracted.
class FormFetchBatcher {
 public:
  FormFetchBatcher(const FormFetchBatcher&) = delete;
  FormFetchBatcher(FormFetchBatcher&&) = delete;

  FormFetchBatcher(id<AutofillDriverIOSBridge> bridge,
                   base::WeakPtr<web::WebFrame> frame,
                   base::TimeDelta batch_period);
  ~FormFetchBatcher();

  // Pushes a request into the current batch. Automatically schedules a new
  // batch if there isn't already one. Consumes the request. Applies
  // `form_name_filter` filtering which will filter forms by name when
  // specified.
  void PushRequest(
      FormFetchCompletion&& fetch_request,
      std::optional<std::u16string> form_name_filter = std::nullopt);

  // Pushes a request into the current batch, runs immediately the current
  // batch, and cancels the scheduled task if there is one. Otherwise, works the
  // same way as PushRequest().
  void PushRequestAndRun(
      FormFetchCompletion&& fetch_request,
      std::optional<std::u16string> form_name_filter = std::nullopt);

 private:
  // Runs an extraction of forms in the frame's document to complete the current
  // batch of requests. The batch isn't completed yet when calling this as there
  // is still the form extraction to be done.
  void Run();

  // Completes the current batch of pending form extraction requests after
  // receiving the results from the scheduled form extraction. Clears the
  // pending requests and makes the batcher available for a new batch.
  void Complete(std::optional<std::vector<autofill::FormData>> forms);

  // Interface for fetching forms.
  id<AutofillDriverIOSBridge> bridge_;
  // Web frame on which the batcher extract forms.
  base::WeakPtr<web::WebFrame> frame_;
  // Queued fetch requests for the next scheduled batch.
  std::vector<FormFetchCompletion> fetch_requests_;
  // Period of time between the batches.
  base::TimeDelta batch_period_;
  // True if a batch was scheduled, so no further scheduling is required.
  bool batch_scheduled_ = false;

  base::WeakPtrFactory<FormFetchBatcher> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_FETCH_BATCHER_H_
