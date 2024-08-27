// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_API_TOAST_ID_H_
#define CHROME_BROWSER_UI_TOASTS_API_TOAST_ID_H_

// Each toast is supposed to have its own unique toast id.
enum class ToastId {
  kLinkCopied = 0,
  kMin = kLinkCopied,
  kImageCopied = 1,
  kLinkToHighlightCopied = 2,
  kMax = kLinkToHighlightCopied
};

#endif  // CHROME_BROWSER_UI_TOASTS_API_TOAST_ID_H_
