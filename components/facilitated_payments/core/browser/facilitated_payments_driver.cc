// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/util/pix_code_validator.h"

namespace payments::facilitated {

FacilitatedPaymentsDriver::FacilitatedPaymentsDriver(
    std::unique_ptr<FacilitatedPaymentsManager> manager)
    : manager_(std::move(manager)) {}

FacilitatedPaymentsDriver::~FacilitatedPaymentsDriver() = default;

void FacilitatedPaymentsDriver::DidNavigateToOrAwayFromPage() const {
  manager_->Reset();
}

void FacilitatedPaymentsDriver::OnContentLoadedInThePrimaryMainFrame(
    const GURL& url,
    ukm::SourceId ukm_source_id) const {
  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(url, ukm_source_id);
}

void FacilitatedPaymentsDriver::OnTextCopiedToClipboard(
    const GURL& render_frame_host_url,
    const std::u16string& copied_text,
    ukm::SourceId ukm_source_id) {
  if (!base::FeatureList::IsEnabled(kEnablePixDetectionOnCopyEvent)) {
    return;
  }

  if (!PixCodeValidator::ContainsPixIdentifier(
          base::UTF16ToUTF8(copied_text))) {
    return;
  }
  manager_->OnPixCodeCopiedToClipboard(
      render_frame_host_url, base::UTF16ToUTF8(copied_text), ukm_source_id);
}

}  // namespace payments::facilitated
