// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"
#include "components/accuracy_tips/features.h"

namespace accuracy_tips {

AccuracyService::AccuracyService(std::unique_ptr<AccuracyTipUI> ui)
    : ui_(std::move(ui)), sample_url_(GURL(kSampleUrl.Get())) {}

AccuracyService::~AccuracyService() = default;

void AccuracyService::CheckAccuracyStatus(const GURL& url,
                                          AccuracyCheckCallback callback) {
  // TODO(crbug.com/1210891): Implement check.
  if (sample_url_.is_valid() && url == sample_url_) {
    std::move(callback).Run(AccuracyTipStatus::kMisinformation);
    return;
  }

  std::move(callback).Run(AccuracyTipStatus::kNone);
}

void AccuracyService::MaybeShowAccuracyTip(content::WebContents* web_contents) {
  // TODO(crbug.com/1210891): Implement rate limiting.
  ui_->ShowAccuracyTip(
      web_contents, AccuracyTipStatus::kMisinformation,
      base::BindOnce(&AccuracyService::OnAccuracyTipClosed,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void AccuracyService::OnAccuracyTipClosed(
    base::TimeTicks time_opened,
    AccuracyTipUI::Interaction interaction) {
  base::UmaHistogramEnumeration("Privacy.AccuracyTip.AccuracyTipInteraction",
                                interaction);
  base::UmaHistogramMediumTimes("Privacy.AccuracyTip.AccuracyTipTimeOpen",
                                base::TimeTicks::Now() - time_opened);
}

void AccuracyService::SetSampleUrlForTesting(const GURL& url) {
  sample_url_ = url;
}

}  // namespace accuracy_tips
