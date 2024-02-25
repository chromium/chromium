// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/url_data_source.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "url/gurl.h"

class Profile;
class SkBitmap;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

// The sanitized image source provides a convenient mean to embed images into
// WebUIs. For security reasons WebUIs are not allowed to download and decode
// external images in their renderer process. The sanitized image source allows
// external images in WebUIs by downloading the image in the browser process,
// decoding the image in an isolated utility process, re-encoding the image and
// sending the now sanitized image back to the requesting WebUI. You can reach
// the image source in the following ways:
//
//   chrome://image?<external image URL>
//   chrome://image?url=<external image URL>
//
// If `static-encode` attribute is set, the image will be re-encoded as a static
// PNG, or a static WebP image depending on if `encode-type` attribute is set to
// `webp`. You can use these attributes in the following ways:
//
//   chrome://image?url=<external image URL>&staticEncode=true
//   chrome://image?url=<external image URL>&encodeType=webp
//   chrome://image?url=<external image URL>&staticEncode=true&encodeType=webp
//
// If the image source points to Google Photos storage, meaning it needs an auth
// token, you can use the `is-google-photos` attribute in the following way:
//
//   chrome://image?url=<external image URL>&isGooglePhotos=true
//
// [CrOS only]: If the source is an animated image, it will be re-encoded as an
// animated WebP image; otherwise it will be re-encoded as a static image as
// though `static-encode` attribute had been set.
class SanitizedImageSource : public content::URLDataSource {
 public:
  using DecodeImageCallback = data_decoder::DecodeImageCallback;
  using DecodeAnimationCallback =
      data_decoder::mojom::ImageDecoder::DecodeAnimationCallback;

  // A delegate class that is faked out for testing purposes.
  class DataDecoderDelegate {
   public:
    DataDecoderDelegate() = default;
    virtual ~DataDecoderDelegate() = default;

    virtual void DecodeImage(const std::string& data,
                             DecodeImageCallback callback);

    virtual void DecodeAnimation(const std::string& data,
                                 DecodeAnimationCallback callback);

   private:
    // The instance of the Data Decoder used by this DataDecoderDelegate to
    // perform any image decoding operations. The underlying service instance is
    // started lazily when needed and torn down when not in use.
    data_decoder::DataDecoder data_decoder_;
  };

  explicit SanitizedImageSource(Profile* profile);
  // This constructor lets us pass mock dependencies for testing.
  SanitizedImageSource(
      Profile* profile,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DataDecoderDelegate> delegate);
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
  struct RequestAttributes {
    enum EncodeType {
      kPng = 0,
      kWebP = 1,
    };

    RequestAttributes();
    RequestAttributes(const RequestAttributes&);
    ~RequestAttributes();

    GURL image_url = GURL();
    bool static_encode = false;
    EncodeType encode_type = EncodeType::kPng;
    std::optional<signin::AccessTokenInfo> access_token_info;
  };

  void StartImageDownload(RequestAttributes request_attributes,
                          content::URLDataSource::GotDataCallback callback);
  void OnImageLoaded(std::unique_ptr<network::SimpleURLLoader> loader,
                     RequestAttributes request_attributes,
                     content::URLDataSource::GotDataCallback callback,
                     std::unique_ptr<std::string> body);
  void OnAnimationDecoded(
      RequestAttributes request_attributes,
      content::URLDataSource::GotDataCallback callback,
      std::vector<data_decoder::mojom::AnimationFramePtr> mojo_frames);

  void EncodeAndReplyStaticImage(
      RequestAttributes request_attributes,
      content::URLDataSource::GotDataCallback callback,
      const SkBitmap& bitmap);
  void EncodeAndReplyAnimatedImage(
      content::URLDataSource::GotDataCallback callback,
      std::vector<data_decoder::mojom::AnimationFramePtr> mojo_frames);

  // Owned by `IdentityManagerFactory` or `IdentityTestEnvironment`.
  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<DataDecoderDelegate> data_decoder_delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SanitizedImageSource> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SANITIZED_IMAGE_SOURCE_H_
