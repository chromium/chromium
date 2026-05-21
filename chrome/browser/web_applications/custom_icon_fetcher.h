// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CUSTOM_ICON_FETCHER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CUSTOM_ICON_FETCHER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "url/gurl.h"

class Profile;

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace web_app {

// CustomIconFetcher downloads and validates custom icons securely inside the
// browser process using network::SimpleURLLoader, and decodes them safely
// out-of-process. Takes the expected SHA256 hash of the icon as an input
// and fails by returning std::nullopt if the SHA256 hash of the downloaded
// icon does not match the expected one.
class CustomIconFetcher : public ImageDecoder::ImageRequest {
 public:
  using Callback = base::OnceCallback<void(std::optional<SkBitmap>)>;

  CustomIconFetcher(Profile* profile,
                    const GURL& icon_url,
                    const std::optional<std::string>& expected_hash);
  ~CustomIconFetcher() override;

  CustomIconFetcher(const CustomIconFetcher&) = delete;
  CustomIconFetcher& operator=(const CustomIconFetcher&) = delete;

  void StartRequest(Callback callback);

 private:
  void OnDownloaded(std::optional<std::string> response_body);

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  const raw_ref<Profile> profile_;
  const GURL icon_url_;
  const std::optional<std::string> expected_hash_;
  Callback callback_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<CustomIconFetcher> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CUSTOM_ICON_FETCHER_H_
