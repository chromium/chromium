// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_thumbnail_loader.h"

#include "ash/public/cpp/image_downloader.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/browser/file_system/file_system_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The native host name that will identify the holding space thumbnail loader to
// the image loader extension.
constexpr char kNativeMessageHostName[] =
    "com.google.holding_space_thumbnail_loader";

using ThumbnailDataCallback = base::OnceCallback<void(const std::string& data)>;

// Handles a parsed message sent from image loader extension in response to a
// thumbnail request.
void HandleParsedThumbnailResponse(
    const std::string& request_id,
    ThumbnailDataCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    VLOG(2) << "Failed to parse request response " << *result.error;
    std::move(callback).Run("");
    return;
  }

  if (!result.value->is_dict()) {
    VLOG(2) << "Invalid response format";
    std::move(callback).Run("");
    return;
  }

  const std::string* received_request_id =
      result.value->FindStringKey("taskId");
  const std::string* data = result.value->FindStringKey("data");

  if (!data || !received_request_id || *received_request_id != request_id) {
    std::move(callback).Run("");
    return;
  }

  std::move(callback).Run(*data);
}

// Native message host for communication to the image loader extension.
// It handles a single image request - when the connection to the extension is
// established, it send a message containing an image request to the image
// loader. It closes the connection once it receives a response from the image
// loader.
class ThumbnailLoaderNativeMessageHost : public extensions::NativeMessageHost {
 public:
  ThumbnailLoaderNativeMessageHost(const std::string& request_id,
                                   const std::string& message,
                                   ThumbnailDataCallback callback)
      : request_id_(request_id),
        message_(message),
        callback_(std::move(callback)) {}

  ~ThumbnailLoaderNativeMessageHost() override {
    if (callback_)
      std::move(callback_).Run("");
  }

  void OnMessage(const std::string& message) override {
    if (response_received_)
      return;
    response_received_ = true;

    // Detach the callback from the message host in case the extension closes
    // connection by the time the response is parsed.
    data_decoder::DataDecoder::ParseJsonIsolated(
        message, base::BindOnce(&HandleParsedThumbnailResponse, request_id_,
                                std::move(callback_)));

    client_->CloseChannel("");
    client_ = nullptr;
  }

  void Start(Client* client) override {
    client_ = client;
    client_->PostMessageFromNativeHost(message_);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return task_runner_;
  }

 private:
  const std::string request_id_;
  const std::string message_;
  ThumbnailDataCallback callback_;

  Client* client_ = nullptr;

  bool response_received_ = false;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::ThreadTaskRunnerHandle::Get();
};

}  // namespace

// Converts a data URL to bitmap.
class HoldingSpaceThumbnailLoader::ThumbnailDecoder
    : public BitmapFetcherDelegate {
 public:
  explicit ThumbnailDecoder(Profile* profile) : profile_(profile) {}

  ThumbnailDecoder(const ThumbnailDecoder&) = delete;
  ThumbnailDecoder& operator=(const ThumbnailDecoder&) = delete;
  ~ThumbnailDecoder() override = default;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override {
    std::move(callback_).Run(bitmap);
  }

  void Start(const std::string& data,
             HoldingSpaceThumbnailLoader::ImageCallback callback) {
    DCHECK(!callback_);
    DCHECK(!bitmap_fetcher_);

    // The data sent from the image loader extension should be in form of a data
    // URL.
    GURL data_url(data);
    if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme)) {
      std::move(callback).Run(nullptr);
      return;
    }

    callback_ = std::move(callback);

    // Note that the image downloader will not use network traffic for data
    // URLs.
    bitmap_fetcher_ = std::make_unique<BitmapFetcher>(
        data_url, this, MISSING_TRAFFIC_ANNOTATION);

    bitmap_fetcher_->Init(
        /*referrer=*/std::string(), net::ReferrerPolicy::NEVER_CLEAR,
        network::mojom::CredentialsMode::kOmit);

    bitmap_fetcher_->Start(profile_->GetURLLoaderFactory().get());
  }

 private:
  Profile* const profile_;
  HoldingSpaceThumbnailLoader::ImageCallback callback_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;
};

HoldingSpaceThumbnailLoader::HoldingSpaceThumbnailLoader(Profile* profile)
    : profile_(profile) {}

HoldingSpaceThumbnailLoader::~HoldingSpaceThumbnailLoader() = default;

HoldingSpaceThumbnailLoader::ThumbnailRequest::ThumbnailRequest(
    const base::FilePath& item_path,
    const gfx::Size& size)
    : item_path(item_path), size(size) {}

HoldingSpaceThumbnailLoader::ThumbnailRequest::~ThumbnailRequest() = default;

base::WeakPtr<HoldingSpaceThumbnailLoader>
HoldingSpaceThumbnailLoader::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HoldingSpaceThumbnailLoader::Load(const ThumbnailRequest& request,
                                       ImageCallback callback) {
  // Get the item's last modified time - this will be used for cache lookup in
  // the image loader extension.
  file_manager::util::GetMetadataForPath(
      file_manager::util::GetFileSystemContextForExtensionId(
          profile_, file_manager::kImageLoaderExtensionId),
      request.item_path,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&HoldingSpaceThumbnailLoader::LoadForFileWithMetadata,
                     weak_factory_.GetWeakPtr(), request, std::move(callback)));
}

void HoldingSpaceThumbnailLoader::LoadForFileWithMetadata(
    const ThumbnailRequest& request,
    ImageCallback callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (file_info.is_directory) {
    std::move(callback).Run(nullptr);
    return;
  }

  GURL thumbnail_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile_, request.item_path, file_manager::kImageLoaderExtensionId,
          &thumbnail_url)) {
    std::move(callback).Run(nullptr);
    return;
  }

  base::UnguessableToken request_id = base::UnguessableToken::Create();
  requests_[request_id] = std::move(callback);

  // Unfortunately the image loader only supports cropping to square dimensions
  // but a request for a non-cropped, non-square image would result in image
  // distortion. To work around this we always request square images and then
  // crop to requested dimensions on our end if necessary after bitmap decoding.
  const int size = std::max(request.size.width(), request.size.height());

  // Generate an image loader request. The request type is defined in
  // ui/file_manager/image_loader/load_image_request.js.
  base::Value request_value(base::Value::Type::DICTIONARY);
  request_value.SetKey("taskId", base::Value(request_id.ToString()));
  request_value.SetKey("url", base::Value(thumbnail_url.spec()));
  request_value.SetKey("timestamp", util::TimeToValue(file_info.last_modified));
  request_value.SetBoolKey("cache", true);
  request_value.SetBoolKey("crop", true);
  request_value.SetKey("priority", base::Value(1));
  request_value.SetKey("width", base::Value(size));
  request_value.SetKey("height", base::Value(size));

  std::string request_message;
  base::JSONWriter::Write(request_value, &request_message);

  // Open a channel to the image loader extension using a message host that send
  // the image loader request.
  auto native_message_host = std::make_unique<ThumbnailLoaderNativeMessageHost>(
      request_id.ToString(), request_message,
      base::BindOnce(&HoldingSpaceThumbnailLoader::OnThumbnailLoaded,
                     weak_factory_.GetWeakPtr(), request_id, request.size));
  const extensions::PortId port_id(base::UnguessableToken::Create(),
                                   1 /* port_number */, true /* is_opener */);
  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile_);
  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));
  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile_), port_id,
      extensions::MessagingEndpoint::ForNativeApp(kNativeMessageHostName),
      std::move(native_message_port), file_manager::kImageLoaderExtensionId,
      GURL(), std::string() /* channel_name */);
}

void HoldingSpaceThumbnailLoader::OnThumbnailLoaded(
    const base::UnguessableToken& request_id,
    const gfx::Size& requested_size,
    const std::string& data) {
  if (!requests_.count(request_id))
    return;

  if (data.empty()) {
    RespondToRequest(request_id, requested_size, nullptr);
    return;
  }

  auto thumbnail_decoder = std::make_unique<ThumbnailDecoder>(profile_);
  ThumbnailDecoder* thumbnail_decoder_ptr = thumbnail_decoder.get();
  thumbnail_decoders_.emplace(request_id, std::move(thumbnail_decoder));
  thumbnail_decoder_ptr->Start(
      data,
      base::BindOnce(&HoldingSpaceThumbnailLoader::RespondToRequest,
                     weak_factory_.GetWeakPtr(), request_id, requested_size));
}

void HoldingSpaceThumbnailLoader::RespondToRequest(
    const base::UnguessableToken& request_id,
    const gfx::Size& requested_size,
    const SkBitmap* bitmap) {
  thumbnail_decoders_.erase(request_id);
  auto request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return;

  // To work around cropping limitations of the image loader, we requested a
  // square image. If requested dimensions were non-square, we need to perform
  // additional cropping on our end.
  SkBitmap cropped_bitmap;
  if (bitmap) {
    gfx::Rect cropped_rect(0, 0, bitmap->width(), bitmap->height());
    if (cropped_rect.size() != requested_size) {
      cropped_bitmap = *bitmap;
      cropped_rect.ClampToCenteredSize(requested_size);
      bitmap->extractSubset(&cropped_bitmap, gfx::RectToSkIRect(cropped_rect));
    }
  }

  ImageCallback callback = std::move(request_it->second);
  requests_.erase(request_it);
  std::move(callback).Run(cropped_bitmap.isNull() ? bitmap : &cropped_bitmap);
}

}  // namespace ash
