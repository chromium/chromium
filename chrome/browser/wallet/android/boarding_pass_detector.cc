// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wallet/android/boarding_pass_detector.h"

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace wallet {
namespace {
mojo::Remote<mojom::BoardingPassExtractor> GetBoardingPassExtractorRemote(
    content::WebContents* web_contents) {
  DCHECK(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  mojo::Remote<mojom::BoardingPassExtractor> remote;
  web_contents->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
      remote.BindNewPipeAndPassReceiver());
  return remote;
}
}  // namespace

bool BoardingPassDetector::ShouldDetect(const std::string& url) {
  std::string param_val = base::GetFieldTrialParamValueByFeature(
      features::kBoardingPassDetector,
      features::kBoardingPassDetectorUrlParam.name);

  std::vector<std::string> allowed_urls =
      base::SplitString(std::move(param_val), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& allowed_url : allowed_urls) {
    if (url.starts_with(allowed_url)) {
      return true;
    }
  }
  return false;
}

BoardingPassDetector::BoardingPassDetector() = default;

BoardingPassDetector::~BoardingPassDetector() = default;

void BoardingPassDetector::DetectBoardingPass(
    content::WebContents* web_contents,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  DetectBoardingPassWithRemote(GetBoardingPassExtractorRemote(web_contents),
                               std::move(callback));
}

void BoardingPassDetector::DetectBoardingPassWithRemote(
    mojo::Remote<mojom::BoardingPassExtractor> remote,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  callback_ = std::move(callback);
  remote_ = std::move(remote);
  if (!remote_) {
    SendResultAndDeleteSelf();
  }

  remote_.set_disconnect_handler(base::BindOnce(
      &BoardingPassDetector::SendResultAndDeleteSelf, base::Unretained(this)));
  remote_->ExtractBoardingPass(
      base::BindOnce(&BoardingPassDetector::ExtractBoardingPassComplete,
                     base::Unretained(this)));
}

void BoardingPassDetector::ExtractBoardingPassComplete(
    const std::vector<std::string>& boarding_passes) {
  extraction_result_ = std::move(boarding_passes);
  SendResultAndDeleteSelf();
}

void BoardingPassDetector::SendResultAndDeleteSelf() {
  std::move(callback_).Run(std::move(extraction_result_));
  delete this;
}

}  // namespace wallet
