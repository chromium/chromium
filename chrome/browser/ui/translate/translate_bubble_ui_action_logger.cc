// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_ui_action_logger.h"

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"

namespace translate {

void ReportUiAction(translate::TranslateBubbleUiEvent action) {
  UMA_HISTOGRAM_ENUMERATION("Translate.BubbleUiEvent", action,
                            translate::TRANSLATE_BUBBLE_UI_EVENT_MAX);
}

}  // namespace translate
