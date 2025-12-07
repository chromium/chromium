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

// Detects boarding passes on a web page. This class is self-owned and will
// delete itself after the detection is complete or if the renderer process
// crashes.
class BoardingPassDetector {
 public:
  BoardingPassDetector();
  ~BoardingPassDetector();

  // Initiates the boarding pass detection on the given `web_contents`.
  // `callback` is invoked with a vector of detected boarding pass strings.
  void DetectBoardingPass(
      content::WebContents* web_contents,
      base::OnceCallback<void(const std::vector<std::string>&)> callback);

  // Performs the boarding pass detection using the provided `remote`.
  void DetectBoardingPassWithRemote(
      mojo::Remote<mojom::BoardingPassExtractor> remote,
      base::OnceCallback<void(const std::vector<std::string>&)> callback);

  // Returns true if the boarding pass detection should be run on the given
  // `url`.
  static bool ShouldDetect(const std::string& url);

 private:
  // Invoked when the boarding pass extraction is complete.
  void ExtractBoardingPassComplete(
      const std::vector<std::string>& boarding_passes);

  // Sends the detection result to the callback and deletes this object.
  void SendResultAndDeleteSelf();

  // The remote for the boarding pass extractor service.
  mojo::Remote<mojom::BoardingPassExtractor> remote_;

  // The callback to be invoked with the detection results.
  base::OnceCallback<void(const std::vector<std::string>&)> callback_;

  // The result of the boarding pass extraction.
  std::vector<std::string> extraction_result_;
};

}  // namespace wallet

#endif  // CHROME_BROWSER_WALLET_ANDROID_BOARDING_PASS_DETECTOR_H_
