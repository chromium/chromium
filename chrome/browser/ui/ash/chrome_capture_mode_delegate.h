// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_CAPTURE_MODE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_CAPTURE_MODE_DELEGATE_H_

#include "ash/public/cpp/capture_mode_delegate.h"

// Implements the interface needed for the delegate of the Capture Mode feature
// in Chrome.
class ChromeCaptureModeDelegate : public ash::CaptureModeDelegate {
 public:
  ChromeCaptureModeDelegate();
  ChromeCaptureModeDelegate(const ChromeCaptureModeDelegate&) = delete;
  ChromeCaptureModeDelegate& operator=(const ChromeCaptureModeDelegate&) =
      delete;
  ~ChromeCaptureModeDelegate() override;

  // ash::CaptureModeDelegate:
  base::FilePath GetActiveUserDownloadsDir() const override;
  void ShowScreenCaptureItemInFolder(const base::FilePath& file_path) override;
  bool Uses24HourFormat() const override;
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_CAPTURE_MODE_DELEGATE_H_
