// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_view.h"

void RecordToastTriggeredToShow(ToastId toast_id) {
  base::UmaHistogramEnumeration("Toast.TriggeredToShow", toast_id);
}

void RecordToastActionButtonClicked(ToastId toast_id) {
  base::RecordComputedAction(
      base::StrCat({"Toast.ActionButtonClicked.", GetToastName(toast_id)}));
}

void RecordToastCloseButtonClicked(ToastId toast_id) {
  base::RecordComputedAction(
      base::StrCat({"Toast.CloseButtonClicked.", GetToastName(toast_id)}));
}

void RecordToastDismissReason(ToastId toast_id,
                              toasts::ToastCloseReason close_reason) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Toast.", GetToastName(toast_id), ".Dismissed"}),
      close_reason);
}
