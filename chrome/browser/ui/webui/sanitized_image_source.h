// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_

#include <list>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/public/browser/url_data_source.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}  // namespace base

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
  std::string GetMimeType(const std::string& path) override;
  bool ShouldReplaceExistingSource() override;

 private:
  void OnImageLoaded(network::SimpleURLLoader* loader,
                     content::URLDataSource::GotDataCallback callback,
                     std::unique_ptr<std::string> body);
  void OnImageDecoded(content::URLDataSource::GotDataCallback callback,
                      const gfx::Image& image);

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::list<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;
  scoped_refptr<base::SequencedTaskRunner> encode_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SanitizedImageSource> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_
