// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_readability_distiller.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"

ReadabilityDistiller::ReadabilityDistiller(
    RequestDistillationCallback request_distillation_callback,
    DistillationCompleteCallback on_distillation_complete_callback)
    : request_distillation_callback_(std::move(request_distillation_callback)),
      on_distillation_complete_callback_(
          std::move(on_distillation_complete_callback)) {
  CHECK(request_distillation_callback_);
  CHECK(on_distillation_complete_callback_);
}

ReadabilityDistiller::~ReadabilityDistiller() = default;

// `request` is unused because Readability runs on the DOM in the browser
// process and does not require an AXTree.
void ReadabilityDistiller::Distill(std::optional<DistillationRequest> request) {
  // Invalidate any pending callback from a superseded request so its reply
  // cannot clear `is_distillation_in_progress_` or invoke `OnContentReceived()`
  // while the new request is in flight.
  weak_ptr_factory_.InvalidateWeakPtrs();
  is_distillation_in_progress_ = true;
  request_distillation_callback_.Run(
      base::BindOnce(&ReadabilityDistiller::OnContentReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ReadabilityDistiller::Reset() {
  // TODO(crbug.com/40802192): Implement in-flight distillation cancellation
  // in a future CL.
}

bool ReadabilityDistiller::IsDistillationInProgress() const {
  return is_distillation_in_progress_;
}

ReadAnythingAppModel::DistillationMethod
ReadabilityDistiller::GetDistillationMethod() const {
  return ReadAnythingAppModel::DistillationMethod::kReadability;
}

void ReadabilityDistiller::OnContentReceived(
    read_anything::mojom::ReadabilityDistillationResult result,
    const std::string& title,
    const std::string& content) {
  is_distillation_in_progress_ = false;

  // Ignore cancelled requests (e.g. superseded by a newer request or cancelled
  // on navigation) so their empty content does not trigger a spurious fallback
  // to Screen2x.
  if (result ==
      read_anything::mojom::ReadabilityDistillationResult::kCancelled) {
    return;
  }

  DistillationResult distillation_result;
  distillation_result.type = DistillationResult::Type::kHTML;
  distillation_result.title = title;
  distillation_result.html_content = content;

  // Copy the callback locally before running it, as empty content triggers a
  // Screen2x fallback that replaces the active distiller and destroys `this`
  // synchronously.
  DistillationCompleteCallback callback = on_distillation_complete_callback_;
  callback.Run(distillation_result);
}
