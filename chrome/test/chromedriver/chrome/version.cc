// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/version.h"

#include "base/strings/stringprintf.h"

namespace {

// This variable must be able to be found and parsed by the upload script.
const int kMinimumSupportedChromeVersion[] = {69, 0, 3497, 0};

}  // namespace

const int kMinimumSupportedChromeBuildNo = kMinimumSupportedChromeVersion[2];

std::string GetMinimumSupportedChromeVersion() {
  return base::StringPrintf(
      "%d.%d.%d.%d",
      kMinimumSupportedChromeVersion[0],
      kMinimumSupportedChromeVersion[1],
      kMinimumSupportedChromeVersion[2],
      kMinimumSupportedChromeVersion[3]);
}
