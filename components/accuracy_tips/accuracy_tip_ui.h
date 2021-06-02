// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_UI_H_
#define COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_UI_H_

#include "base/callback.h"
#include "components/accuracy_tips/accuracy_tip_status.h"

namespace content {
class WebContents;
}

namespace accuracy_tips {

// Abstract class that creates a platform-specific UI for accuracy tips.
class AccuracyTipUI {
 public:
  AccuracyTipUI() = default;
  virtual ~AccuracyTipUI() = default;

  AccuracyTipUI(const AccuracyTipUI&) = delete;
  AccuracyTipUI& operator=(const AccuracyTipUI&) = delete;

  // Represents the different user interactions with a AccuracyTip dialog.
  enum class Interaction {
    kNoAction = 0,
    kDismiss = 1,
    kIgnore = 2,
    kLearnMore = 3,

    kMaxValue = kLearnMore,
  };

  // Shows AccuracyTip UI using the specified information if it is not already
  // showing. |close_callback| will be called when
  // the dialog is closed; the argument indicates the action that the user took
  // (if any) to close the dialog.
  virtual void ShowAccuracyTip(
      content::WebContents* web_contents,
      AccuracyTipStatus type,
      base::OnceCallback<void(Interaction)> close_callback) = 0;
};

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_UI_H_
