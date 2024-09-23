// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_single_icon_hasher.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/base/data_url.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/smhasher/src/MurmurHash2.h"
#include "ui/gfx/codec/png_codec.h"

namespace webapps {
namespace {

// The seed to use when taking the murmur2 hash of the icon.
const uint64_t kMurmur2HashSeed = 0;

// Computes Murmur2 hash of |raw_image_data|.
std::string ComputeMurmur2Hash(const std::string& raw_image_data) {
  // WARNING: We are running in the browser process. |raw_image_data| is the
  // image's raw, unsanitized bytes from the web. |raw_image_data| may contain
  // malicious data. Decoding unsanitized bitmap data to an SkBitmap in the
  // browser process is a security bug.
  uint64_t hash = MurmurHash64A(raw_image_data.data(), raw_image_data.size(),
                                kMurmur2HashSeed);
  return base::NumberToString(hash);
}

}  // anonymous namespace

WebApkSingleIconHasher::WebApkSingleIconHasher(
    base::PassKey<WebApkIconsHasher> pass_key,
    network::mojom::URLLoaderFactory* url_loader_factory,
    base::WeakPtr<content::WebContents> web_contents,
    const url::Origin& request_initiator,
    int timeout_ms,
    WebappIcon* webapk_icon,
    base::OnceClosure callback)
    : icon_(webapk_icon), callback_(std::move(callback)) {
  DCHECK(url_loader_factory);

  if (!icon_->url().is_valid()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_)));
    return;
  }

  if (icon_->url().SchemeIs(url::kDataScheme)) {
    std::string mime_type, char_set, data;
    if (net::DataURL::Parse(icon_->url(), &mime_type, &char_set, &data) &&
        !data.empty()) {
      icon_->set_hash(ComputeMurmur2Hash(data));
      icon_->SetData(std::move(data));
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_)));
    return;
  }

  download_timeout_timer_.Start(
      FROM_HERE, base::Milliseconds(timeout_ms),
      base::BindOnce(&WebApkSingleIconHasher::OnDownloadTimedOut,
                     base::Unretained(this)));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, request_initiator,
      request_initiator, net::SiteForCookies());
  resource_request->request_initiator = request_initiator;
  resource_request->url = icon_->url();
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      TRAFFIC_ANNOTATION_WITHOUT_PROTO("webapk icon hasher"));
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&WebApkSingleIconHasher::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(web_contents),
                     icon_->GetIdealSizeInPx(), timeout_ms));
}

WebApkSingleIconHasher::~WebApkSingleIconHasher() = default;

void WebApkSingleIconHasher::OnSimpleLoaderComplete(
    base::WeakPtr<content::WebContents> web_contents,
    int ideal_icon_size,
    int timeout_ms,
    std::unique_ptr<std::string> response_body) {
  download_timeout_timer_.Stop();

  // Check for non-empty body in case of HTTP 204 (no content) response.
  if (!response_body || response_body->empty()) {
    RunCallbackAndFinish();
    return;
  }

  // If the image is png/jpg/jpeg, send the raw data to server to decode,
  // otherwise decode the image using Blink's image decoder.
  auto simple_url_loader = std::move(simple_url_loader_);
  if (simple_url_loader->ResponseInfo() &&
      (simple_url_loader->ResponseInfo()->mime_type == "image/png" ||
       simple_url_loader->ResponseInfo()->mime_type == "image/jpg" ||
       simple_url_loader->ResponseInfo()->mime_type == "image/jpeg")) {
    // WARNING: We are running in the browser process. |*response_body| is the
    // image's raw, unsanitized bytes from the web. |*response_body| may contain
    // malicious data. Decoding unsanitized bitmap data to an SkBitmap in the
    // browser process is a security bug.
    icon_->SetData(std::move(*response_body));
    icon_->set_hash(ComputeMurmur2Hash(icon_->unsafe_data()));
    RunCallbackAndFinish();
    return;
  }

  if (!web_contents) {
    RunCallbackAndFinish();
    return;
  }

  download_timeout_timer_.Start(
      FROM_HERE, base::Milliseconds(timeout_ms),
      base::BindOnce(&WebApkSingleIconHasher::OnDownloadTimedOut,
                     base::Unretained(this)));

  const gfx::Size preferred_size(ideal_icon_size, ideal_icon_size);
  web_contents->DownloadImage(
      simple_url_loader->GetFinalURL(),
      false,  // is_favicon
      preferred_size,
      std::numeric_limits<int>::max(),  // max size
      false,                            // normal cache policy
      base::BindOnce(&WebApkSingleIconHasher::OnImageDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(response_body)));
}

void WebApkSingleIconHasher::OnImageDownloaded(
    std::unique_ptr<std::string> response_body,
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  download_timeout_timer_.Stop();
  if (bitmaps.empty()) {
    RunCallbackAndFinish();
    return;
  }

  SetIconDataAndHashFromSkBitmap(icon_, bitmaps[0], std::move(response_body));

  RunCallbackAndFinish();
}

void WebApkSingleIconHasher::SetIconDataAndHashFromSkBitmap(
    WebappIcon* icon,
    const SkBitmap& bitmap,
    std::unique_ptr<std::string> response_body) {
  if (bitmap.drawsNothing()) {
    return;
  }
  std::vector<unsigned char> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_bytes);

  icon->SetData(std::string(png_bytes.begin(), png_bytes.end()));
  icon->set_hash(
      ComputeMurmur2Hash(response_body ? *response_body : icon->unsafe_data()));
}

void WebApkSingleIconHasher::OnDownloadTimedOut() {
  simple_url_loader_.reset();
  RunCallbackAndFinish();
}

void WebApkSingleIconHasher::RunCallbackAndFinish() {
  std::move(callback_).Run();
}

}  // namespace webapps
