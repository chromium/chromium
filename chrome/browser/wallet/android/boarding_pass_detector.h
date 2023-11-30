// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WALLET_ANDROID_BOARDING_PASS_DETECTOR_H_
#define CHROME_BROWSER_WALLET_ANDROID_BOARDING_PASS_DETECTOR_H_

#include <string>

namespace wallet {

class BoardingPassDetector {
 public:
  // Decides whether to run boarding pass detection on given url.
  static bool ShouldDetect(const std::string& url);
};

}  // namespace wallet

#endif  // CHROME_BROWSER_WALLET_ANDROID_BOARDING_PASS_DETECTOR_H_
