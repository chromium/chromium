// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/public/browser/url_data_source.h"

class Profile;

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageDecoder;
}  // namespace image_fetcher

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

// The sanitized image source provides a convenient mean to embed images into
// WebUIs. For security reasons WebUIs are not allowed to download and decode
// external images in their renderer process. The sanitized image source allows
// external images in WebUIs by downloading the image in the browser process,
// decoding the image in an isolated utility process, re-encoding the image as
// PNG and sending the now sanitized image back to the requesting WebUI. You can
// reach the image source via:
//
//   chrome://image?<external image URL>
//
// If the image source points to Google Photos storage, meaning it needs an auth
// token to be downloaded, you can use the is-google-photos attribute as
// follows:
//   chrome://image?url=<external image URL>&isGooglePhotos=true
class SanitizedImageSource : public content::URLDataSource {
 public:
  explicit SanitizedImageSource(Profile* profile);
  // This constructor lets us pass mock dependencies for testing.
  SanitizedImageSource(
      Profile* profile,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<image_fetcher::ImageDecoder> image_decoder);
  SanitizedImageSource(const SanitizedImageSource&) = delete;
  SanitizedImageSource& operator=(const SanitizedImageSource&) = delete;
  ~SanitizedImageSource() override;

  // content::URLDataSource:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool ShouldReplaceExistingSource() override;

  void set_identity_manager_for_test(
      signin::IdentityManager* identity_manager) {
    identity_manager_ = identity_manager;
  }

 private:
  void StartImageDownload(
      GURL image_url,
      content::URLDataSource::GotDataCallback callback,
      absl::optional<signin::AccessTokenInfo> access_token_info);
  void OnImageLoaded(std::unique_ptr<network::SimpleURLLoader> loader,
                     content::URLDataSource::GotDataCallback callback,
                     std::unique_ptr<std::string> body);
  void OnImageDecoded(content::URLDataSource::GotDataCallback callback,
                      const gfx::Image& image);

  // Owned by `IdentityManagerFactory` or `IdentityTestEnvironment`.
  raw_ptr<signin::IdentityManager> identity_manager_;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SanitizedImageSource> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_
