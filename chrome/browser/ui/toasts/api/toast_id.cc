// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/api/toast_id.h"

#include <string>

#include "base/notreached.h"

std::string GetToastName(ToastId toast_id) {
  switch (toast_id) {
    case ToastId::kLinkCopied:
      return "LinkCopied";
    case ToastId::kImageCopied:
      return "ImageCopied";
    case ToastId::kLinkToHighlightCopied:
      return "LinkToHighlightCopied";
    case ToastId::kAddedToReadingList:
      return "AddedToReadingList";
    case ToastId::kLensOverlay:
      return "LensOverlay";
    case ToastId::kNonMilestoneUpdate:
      return "NonMilestoneUpdate";
    case ToastId::kAddedToComparisonTable:
      return "AddedToComparisonTable";
    case ToastId::kClearBrowsingData:
      return "ClearBrowsingData";
  }

  NOTREACHED();
}
