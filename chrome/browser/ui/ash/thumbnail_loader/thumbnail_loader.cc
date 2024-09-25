// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/thumbnail_loader/thumbnail_loader.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "ash/public/cpp/image_downloader.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "net/base/data_url.h"
#include "net/base/mime_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The native host name that will identify the thumbnail loader to the image
// loader extension.
constexpr char kNativeMessageHostName[] = "com.google.ash_thumbnail_loader";

// Returns whether the given `file_path` is supported by the `ThumbnailLoader`.
bool IsSupported(const base::FilePath& file_path) {
  constexpr std::array<std::pair<const char*, const char*>, 25>
      kFileMatchPatterns = {{
          // Document types ----------------------------------------------------
          {
              /*extension=*/"(?i)\\.pdf$",
              /*mime_type=*/"(?i)application\\/pdf",
          },
          // Image types -------------------------------------------------------
          {
              /*extension=*/"(?i)\\.jpe?g$",
              /*mime_type=*/"(?i)image\\/jpeg",
          },
          {
              /*extension=*/"(?i)\\.bmp$",
              /*mime_type=*/"(?i)image\\/bmp",
          },
          {
              /*extension=*/"(?i)\\.gif$",
              /*mime_type=*/"(?i)image\\/gif",
          },
          {
              /*extension=*/"(?i)\\.ico$",
              /*mime_type=*/"(?i)image\\/x\\-icon",
          },
          {
              /*extension=*/"(?i)\\.png$",
              /*mime_type=*/"(?i)image\\/png",
          },
          {
              /*extension=*/"(?i)\\.webp$",
              /*mime_type=*/"(?i)image\\/webp",
          },
          {
              /*extension=*/"(?i)\\.tiff?$",
              /*mime_type=*/"(?i)image\\/tiff",
          },
          {
              /*extension=*/"(?i)\\.svg$",
              /*mime_type=*/"(?i)image\\/svg\\+xml",
          },
          {
              /*extension=*/"(?i)\\.avif$",
              /*mime_type=*/"(?i)image\\/avif",
          },
          // Raw types ---------------------------------------------------------
          {
              /*extension=*/"(?i)\\.arw$",
              /*mime_type=*/nullptr,
          },
          {
              /*extension=*/"(?i)\\.cr2$",
              /*mime_type=*/nullptr,
          },
          {
              /*extension=*/"(?i)\\.dng$",
              /*mime_type=*/nullptr,
          },
          {
              /*extension=*/"(?i)\\.nef$",
              /*mime_type=*/nullptr,
          },
          {
              /*extension=*/"(?i)\\.nrw$",
              /*mime_type=*/nullptr,
          },
          {
              /*extension=*/"(?i)\\.orf$",
              /*mime_type=*/nullptr,
          },
          {
              /*extension=*/"(?i)\\.raf$",
              /*mime_type=*/nullptr,
          },
          {
              /*extension=*/"(?i)\\.rw2$",
              /*mime_type=*/nullptr,
          },
          // Video types -------------------------------------------------------
          {
              /*extension=*/"(?i)\\.3gpp?$",
              /*mime_type=*/"(?i)video\\/3gpp",
          },
          {
              /*extension=*/"(?i)\\.avi$",
              /*mime_type=*/"(?i)video\\/x\\-msvideo",
          },
          {
              /*extension=*/"(?i)\\.mov$",
              /*mime_type=*/"(?i)video\\/quicktime",
          },
          {
              /*extension=*/"\\.mkv$",
              /*mime_type=*/"video\\/x\\-matroska",
          },
          {
              /*extension=*/"(?i)\\.m(p4|4v|pg|peg|pg4|peg4)$",
              /*mime_type=*/"(?i)video\\/mp(4|eg)",
          },
          {
              /*extension=*/"(?i)\\.og(m|v|x)$",
              /*mime_type=*/"(?i)(application|video)\\/ogg",
          },
          {
              /*extension=*/"(?i)\\.webm$",
              /*mime_type=*/"(?i)video\\/webm",
          },
      }};

  // First attempt to match based on `mime_type`.
  std::string ext = file_path.Extension();
  std::string mime_type;
  if (!ext.empty() &&
      net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type)) {
    for (const auto& file_match_pattern : kFileMatchPatterns) {
      if (file_match_pattern.second &&
          re2::RE2::FullMatch(mime_type, file_match_pattern.second)) {
        return true;
      }
    }
  }

  // Then attempt to match based on `file_path` extension.
  for (const auto& file_match_pattern : kFileMatchPatterns) {
    if (re2::RE2::FullMatch(file_path.Extension(), file_match_pattern.first))
      return true;
  }

  return false;
}

using ThumbnailDataCallback = base::OnceCallback<void(const std::string& data)>;

// Handles a parsed message sent from image loader extension in response to a
// thumbnail request.
void HandleParsedThumbnailResponse(
    const std::string& request_id,
    ThumbnailDataCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    VLOG(2) << "Failed to parse request response " << result.error();
    std::move(callback).Run("");
    return;
  }

  if (!result->is_dict()) {
    VLOG(2) << "Invalid response format";
    std::move(callback).Run("");
    return;
  }

  const std::string* received_request_id =
      result->GetDict().FindString("taskId");
  const std::string* data = result->GetDict().FindString("data");

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

  raw_ptr<Client> client_ = nullptr;

  bool response_received_ = false;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
};

}  // namespace

// Converts a data URL to bitmap.
class ThumbnailLoader::ThumbnailDecoder : public ImageDecoder::ImageRequest {
 public:
  ThumbnailDecoder() = default;

  ThumbnailDecoder(const ThumbnailDecoder&) = delete;
  ThumbnailDecoder& operator=(const ThumbnailDecoder&) = delete;
  ~ThumbnailDecoder() override = default;

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& bitmap) override {
    std::move(callback_).Run(&bitmap, base::File::FILE_OK);
  }

  // ImageDecoder::ImageRequest:
  void OnDecodeImageFailed() override {
    std::move(callback_).Run(/*bitmap=*/nullptr, base::File::FILE_ERROR_FAILED);
  }

  void Start(const std::string& data, ThumbnailLoader::ImageCallback callback) {
    DCHECK(!callback_);

    // The data sent from the image loader extension should be in form of a data
    // URL.
    GURL data_url(data);
    if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme)) {
      std::move(callback).Run(/*bitmap=*/nullptr,
                              base::File::FILE_ERROR_FAILED);
      return;
    }

    std::string mime_type, charset, image_data;
    if (!net::DataURL::Parse(data_url, &mime_type, &charset, &image_data)) {
      std::move(callback).Run(/*bitmap=*/nullptr,
                              base::File::FILE_ERROR_FAILED);
      return;
    }

    callback_ = std::move(callback);
    ImageDecoder::Start(this, std::move(image_data));
  }

 private:
  ThumbnailLoader::ImageCallback callback_;
};

ThumbnailLoader::ThumbnailLoader(Profile* profile) : profile_(profile) {}

ThumbnailLoader::~ThumbnailLoader() {
  // Run any pending callbacks to clean them up.
  for (auto it = requests_.begin(); it != requests_.end();) {
    std::move(it->second).Run(nullptr, base::File::Error::FILE_ERROR_ABORT);
    it = requests_.erase(it);
  }
}

ThumbnailLoader::ThumbnailRequest::ThumbnailRequest(
    const base::FilePath& file_path,
    const gfx::Size& size)
    : file_path(file_path), size(size) {}

ThumbnailLoader::ThumbnailRequest::~ThumbnailRequest() = default;

base::WeakPtr<ThumbnailLoader> ThumbnailLoader::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ThumbnailLoader::Load(const ThumbnailRequest& request,
                           ImageCallback callback) {
  // Get the file's last modified time - this will be used for cache lookup in
  // the image loader extension.
  GURL source_url = extensions::Extension::GetBaseURLFromExtensionId(
      file_manager::kImageLoaderExtensionId);
  file_manager::util::GetMetadataForPath(
      file_manager::util::GetFileSystemContextForSourceURL(profile_,
                                                           source_url),
      request.file_path,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kLastModified},
      base::BindOnce(&ThumbnailLoader::LoadForFileWithMetadata,
                     weak_factory_.GetWeakPtr(), request, std::move(callback)));
}

void ThumbnailLoader::LoadForFileWithMetadata(
    const ThumbnailRequest& request,
    ImageCallback callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(/*bitmap=*/nullptr, result);
    return;
  }

  // Short-circuit icons for folders.
  if (file_info.is_directory) {
    // `FILE_ERROR_NOT_A_FILE` is a special value used to signify that the
    // file for which the thumbnail was requested is actually a folder.
    std::move(callback).Run(/*bitmap=*/nullptr,
                            base::File::FILE_ERROR_NOT_A_FILE);
    return;
  }

  // Short-circuit if unsupported.
  if (!IsSupported(request.file_path)) {
    std::move(callback).Run(/*bitmap=*/nullptr, base::File::FILE_ERROR_ABORT);
    return;
  }

  GURL thumbnail_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile_, request.file_path,
          extensions::Extension::GetBaseURLFromExtensionId(
              file_manager::kImageLoaderExtensionId),
          &thumbnail_url)) {
    std::move(callback).Run(/*bitmap=*/nullptr, base::File::FILE_ERROR_FAILED);
    return;
  }

  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile_);
  if (!message_service) {  // May be `nullptr` in tests.
    std::move(callback).Run(/*bitmap=*/nullptr, base::File::FILE_ERROR_FAILED);
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
  base::Value::Dict request_dict;
  request_dict.Set("taskId", base::Value(request_id.ToString()));
  request_dict.Set("url", base::Value(thumbnail_url.spec()));
  request_dict.Set("timestamp", base::TimeToValue(file_info.last_modified));
  // TODO(crbug.com/2650014) : Add an arg to set this to false for sharesheet.
  request_dict.Set("cache", true);
  request_dict.Set("crop", true);
  request_dict.Set("priority", base::Value(1));
  request_dict.Set("width", base::Value(size));
  request_dict.Set("height", base::Value(size));

  std::string request_message;
  base::JSONWriter::Write(request_dict, &request_message);

  // Open a channel to the image loader extension using a message host that send
  // the image loader request.
  auto native_message_host = std::make_unique<ThumbnailLoaderNativeMessageHost>(
      request_id.ToString(), request_message,
      base::BindOnce(&ThumbnailLoader::OnThumbnailLoaded,
                     weak_factory_.GetWeakPtr(), request_id, request.size));
  const extensions::PortId port_id(
      base::UnguessableToken::Create(), 1 /* port_number */,
      true /* is_opener */, extensions::mojom::SerializationFormat::kJson);
  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));
  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile_), port_id,
      extensions::MessagingEndpoint::ForNativeApp(kNativeMessageHostName),
      std::move(native_message_port), file_manager::kImageLoaderExtensionId,
      GURL(), extensions::mojom::ChannelType::kNative,
      std::string() /* channel_name */);
}

void ThumbnailLoader::OnThumbnailLoaded(
    const base::UnguessableToken& request_id,
    const gfx::Size& requested_size,
    const std::string& data) {
  if (!requests_.count(request_id))
    return;

  if (data.empty()) {
    RespondToRequest(request_id, requested_size, /*bitmap=*/nullptr,
                     base::File::FILE_ERROR_FAILED);
    return;
  }

  auto thumbnail_decoder = std::make_unique<ThumbnailDecoder>();
  ThumbnailDecoder* thumbnail_decoder_ptr = thumbnail_decoder.get();
  thumbnail_decoders_.emplace(request_id, std::move(thumbnail_decoder));
  thumbnail_decoder_ptr->Start(
      data,
      base::BindOnce(&ThumbnailLoader::RespondToRequest,
                     weak_factory_.GetWeakPtr(), request_id, requested_size));
}

void ThumbnailLoader::RespondToRequest(const base::UnguessableToken& request_id,
                                       const gfx::Size& requested_size,
                                       const SkBitmap* bitmap,
                                       base::File::Error error) {
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
  std::move(callback).Run(cropped_bitmap.isNull() ? bitmap : &cropped_bitmap,
                          error);
}

}  // namespace ash
