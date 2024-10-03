// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_API_TOAST_ID_H_
#define CHROME_BROWSER_UI_TOASTS_API_TOAST_ID_H_

#include <string>

// Each toast is supposed to have its own unique toast id and corresponding
// string name. New additions to ToastId enum should also be added to
// tools/metrics/histograms/metadata/toasts/enums.xml and toasts that
// adds an action/close button should add an entry to
// tools/metrics/actions/actions.xml.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(ToastId)
enum class ToastId {
  kLinkCopied = 0,
  kMinValue = kLinkCopied,
  kImageCopied = 1,
  kLinkToHighlightCopied = 2,
  kAddedToReadingList = 3,
  kLensOverlay = 4,
  kNonMilestoneUpdate = 5,
  kAddedToComparisonTable = 6,
  kClearBrowsingData = 7,
  kMaxValue = kClearBrowsingData
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/toasts/enums.xml:ToastId)

// Returns the string equivalent name persisted to logs for `toast_id`.
// New additions should also be added to
// tools/metrics/histograms/metadata/toasts/histograms.xml
std::string GetToastName(ToastId toast_id);

#endif  // CHROME_BROWSER_UI_TOASTS_API_TOAST_ID_H_
