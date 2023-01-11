// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_icon_hasher.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
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

// The default number of milliseconds to wait for the icon download to complete.
const int kDownloadTimeoutInMilliseconds = 60000;

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

void OnMurmur2Hash(WebApkIconHasher::Icon* result,
                   base::OnceClosure done_closure,
                   WebApkIconHasher::Icon icon) {
  *result = std::move(icon);
  std::move(done_closure).Run();
}

void OnAllMurmur2Hashes(
    std::unique_ptr<std::map<std::string, WebApkIconHasher::Icon>> icons,
    WebApkIconHasher::Murmur2HashMultipleCallback callback) {
  for (const auto& icon_pair : *icons) {
    if (icon_pair.second.hash.empty()) {
      std::move(callback).Run(absl::nullopt);
      return;
    }
  }

  std::move(callback).Run(std::move(*icons));
}

}  // anonymous namespace

// static
void WebApkIconHasher::DownloadAndComputeMurmur2Hash(
    network::mojom::URLLoaderFactory* url_loader_factory,
    base::WeakPtr<content::WebContents> web_contents,
    const url::Origin& request_initiator,
    const std::set<GURL>& icon_urls,
    Murmur2HashMultipleCallback callback) {
  auto icons_ptr = std::make_unique<std::map<std::string, Icon>>();
  auto& icons = *icons_ptr;

  auto barrier_closure = base::BarrierClosure(
      icon_urls.size(),
      base::BindOnce(&OnAllMurmur2Hashes, std::move(icons_ptr),
                     std::move(callback)));

  for (const auto& icon_url : icon_urls) {
    // |hashes| is owned by |barrier_closure|.
    DownloadAndComputeMurmur2HashWithTimeout(
        url_loader_factory, web_contents, request_initiator, icon_url,
        kDownloadTimeoutInMilliseconds,
        base::BindOnce(&OnMurmur2Hash, &icons[icon_url.spec()],
                       barrier_closure));
  }
}

// static
void WebApkIconHasher::DownloadAndComputeMurmur2HashWithTimeout(
    network::mojom::URLLoaderFactory* url_loader_factory,
    base::WeakPtr<content::WebContents> web_contents,
    const url::Origin& request_initiator,
    const GURL& icon_url,
    int timeout_ms,
    Murmur2HashCallback callback) {
  if (!icon_url.is_valid()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), Icon{}));
    return;
  }

  if (icon_url.SchemeIs(url::kDataScheme)) {
    std::string mime_type, char_set, data;
    Icon icon;
    if (net::DataURL::Parse(icon_url, &mime_type, &char_set, &data) &&
        !data.empty()) {
      icon.hash = ComputeMurmur2Hash(data);
      icon.unsafe_data = std::move(data);
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(icon)));
    return;
  }

  // The icon hasher will delete itself when it is done.
  new WebApkIconHasher(url_loader_factory, std::move(web_contents),
                       request_initiator, icon_url, timeout_ms,
                       std::move(callback));
}

WebApkIconHasher::WebApkIconHasher(
    network::mojom::URLLoaderFactory* url_loader_factory,
    base::WeakPtr<content::WebContents> web_contents,
    const url::Origin& request_initiator,
    const GURL& icon_url,
    int timeout_ms,
    Murmur2HashCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(url_loader_factory);

  download_timeout_timer_.Start(
      FROM_HERE, base::Milliseconds(timeout_ms),
      base::BindOnce(&WebApkIconHasher::OnDownloadTimedOut,
                     base::Unretained(this)));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, request_initiator,
      request_initiator, net::SiteForCookies());
  resource_request->request_initiator = request_initiator;
  resource_request->url = icon_url;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      TRAFFIC_ANNOTATION_WITHOUT_PROTO("webapk icon hasher"));
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&WebApkIconHasher::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(web_contents),
                     timeout_ms));
}

WebApkIconHasher::~WebApkIconHasher() {}

void WebApkIconHasher::OnSimpleLoaderComplete(
    base::WeakPtr<content::WebContents> web_contents,
    int timeout_ms,
    std::unique_ptr<std::string> response_body) {
  download_timeout_timer_.Stop();

  // Check for non-empty body in case of HTTP 204 (no content) response.
  if (!response_body || response_body->empty()) {
    RunCallback({});
    return;
  }

  // If it's an SVG or WEBP file, decode the image using Blink's image decoder.
  auto simple_url_loader = std::move(simple_url_loader_);
  if (simple_url_loader->ResponseInfo() &&
      (simple_url_loader->ResponseInfo()->mime_type == "image/svg+xml" ||
       simple_url_loader->ResponseInfo()->mime_type == "image/webp")) {
    if (!web_contents) {
      RunCallback({});
      return;
    }

    download_timeout_timer_.Start(
        FROM_HERE, base::Milliseconds(timeout_ms),
        base::BindOnce(&WebApkIconHasher::OnDownloadTimedOut,
                       base::Unretained(this)));

    web_contents->DownloadImage(
        simple_url_loader->GetFinalURL(),
        false,        // is_favicon
        gfx::Size(),  // no preferred size
        0,            // no max size
        false,        // normal cache policy
        base::BindOnce(&WebApkIconHasher::OnImageDownloaded,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(response_body)));
    return;
  }

  // WARNING: We are running in the browser process. |*response_body| is the
  // image's raw, unsanitized bytes from the web. |*response_body| may contain
  // malicious data. Decoding unsanitized bitmap data to an SkBitmap in the
  // browser process is a security bug.
  Icon icon;
  icon.unsafe_data = std::move(*response_body);
  icon.hash = ComputeMurmur2Hash(icon.unsafe_data);
  RunCallback(std::move(icon));
}

void WebApkIconHasher::OnImageDownloaded(
    std::unique_ptr<std::string> response_body,
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  download_timeout_timer_.Stop();

  if (bitmaps.empty()) {
    RunCallback({});
    return;
  }

  std::vector<unsigned char> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmaps[0], false, &png_bytes);

  Icon icon;
  icon.unsafe_data = std::string(png_bytes.begin(), png_bytes.end());
  icon.hash = ComputeMurmur2Hash(*response_body);
  RunCallback(std::move(icon));
}

void WebApkIconHasher::OnDownloadTimedOut() {
  simple_url_loader_.reset();

  RunCallback({});
}

void WebApkIconHasher::RunCallback(Icon icon) {
  std::move(callback_).Run(std::move(icon));
  delete this;
}

}  // namespace webapps
