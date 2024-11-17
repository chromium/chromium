// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_METRICS_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_METRICS_H_

enum class ToastId;

namespace toasts {
enum class ToastCloseReason;
}

void RecordToastTriggeredToShow(ToastId toast_id);

void RecordToastActionButtonClicked(ToastId toast_id);

void RecordToastCloseButtonClicked(ToastId toast_id);

void RecordToastDismissReason(ToastId toast_id,
                              toasts::ToastCloseReason close_reason);

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_METRICS_H_
