// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WALLET_ANDROID_BOARDING_PASS_DETECTOR_H_
#define CHROME_BROWSER_WALLET_ANDROID_BOARDING_PASS_DETECTOR_H_

#include <string>
#include "chrome/common/wallet/boarding_pass_extractor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace wallet {

class BoardingPassDetector {
 public:
  BoardingPassDetector();
  ~BoardingPassDetector();

  // Detects boarding pass on cureent web_contents.
  void DetectBoardingPass(
      content::WebContents* web_contents,
      base::OnceCallback<void(const std::vector<std::string>&)> callback);

  void DetectBoardingPassWithRemote(
      mojo::Remote<mojom::BoardingPassExtractor> remote,
      base::OnceCallback<void(const std::vector<std::string>&)> callback);

  // Decides whether to run boarding pass detection on given url.
  static bool ShouldDetect(const std::string& url);

 private:
  void ExtractBoardingPassComplete(
      const std::vector<std::string>& boarding_passes);
  void SendResultAndDeleteSelf();

  mojo::Remote<mojom::BoardingPassExtractor> remote_;
  base::OnceCallback<void(const std::vector<std::string>&)> callback_;
  std::vector<std::string> extraction_result_;
};

}  // namespace wallet

#endif  // CHROME_BROWSER_WALLET_ANDROID_BOARDING_PASS_DETECTOR_H_
