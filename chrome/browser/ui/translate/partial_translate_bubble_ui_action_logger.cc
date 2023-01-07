// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/partial_translate_bubble_ui_action_logger.h"

#include "base/metrics/histogram_macros.h"

namespace translate {

void ReportPartialTranslateBubbleUiAction(
    translate::PartialTranslateBubbleUiEvent action) {
  UMA_HISTOGRAM_ENUMERATION(
      kPartialTranslateBubbleUiEventHistogramName, action,
      translate::PartialTranslateBubbleUiEvent::kMaxValue);
}

}  // namespace translate
