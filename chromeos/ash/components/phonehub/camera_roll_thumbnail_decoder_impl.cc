// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/camera_roll_thumbnail_decoder_impl.h"

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/camera_roll_item.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace phonehub {

namespace {

using BatchDecodeCallback =
    ::base::OnceCallback<void(CameraRollThumbnailDecoder::BatchDecodeResult,
                              const std::vector<CameraRollItem>&)>;

}  // namespace

CameraRollThumbnailDecoderImpl::DecodeRequest::DecodeRequest(
    const proto::CameraRollItem& item_proto)
    : item_proto_(item_proto) {}

CameraRollThumbnailDecoderImpl::DecodeRequest::~DecodeRequest() = default;

const proto::CameraRollItemMetadata&
CameraRollThumbnailDecoderImpl::DecodeRequest::GetMetadata() const {
  return item_proto_.metadata();
}

void CameraRollThumbnailDecoderImpl::DecodeRequest::CompleteWithDecodedBitmap(
    const SkBitmap& bitmap) {
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  if (!image_skia.isNull()) {
    image_skia.MakeThreadSafe();
    decoded_thumbnail_ = gfx::Image(image_skia);
  }
  is_completed_ = true;
}

void CameraRollThumbnailDecoderImpl::DecodeRequest::CompleteWithExistingImage(
    const gfx::Image& image) {
  decoded_thumbnail_ = image;
  is_completed_ = true;
}

const std::string&
CameraRollThumbnailDecoderImpl::DecodeRequest::GetEncodedThumbnail() const {
  return item_proto_.thumbnail().data();
}

CameraRollThumbnailDecoderImpl::DecoderDelegate::DecoderDelegate() = default;

CameraRollThumbnailDecoderImpl::DecoderDelegate::~DecoderDelegate() = default;

void CameraRollThumbnailDecoderImpl::DecoderDelegate::DecodeThumbnail(
    const DecodeRequest& request,
    data_decoder::DecodeImageCallback callback) {
  const std::string& encoded_thumbnail = request.GetEncodedThumbnail();
  data_decoder::DecodeImage(
      &data_decoder_, base::as_bytes(base::make_span(encoded_thumbnail)),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(), std::move(callback));
}

CameraRollThumbnailDecoderImpl::CameraRollThumbnailDecoderImpl()
    : decoder_delegate_(std::make_unique<DecoderDelegate>()) {}

CameraRollThumbnailDecoderImpl::~CameraRollThumbnailDecoderImpl() = default;

void CameraRollThumbnailDecoderImpl::BatchDecode(
    const proto::FetchCameraRollItemsResponse& response,
    const std::vector<CameraRollItem>& current_items,
    BatchDecodeCallback callback) {
  CancelPendingRequests();

  for (const proto::CameraRollItem& item_proto : response.items()) {
    pending_requests_.emplace_back(item_proto);
  }
  pending_callback_ = std::move(callback);

  base::flat_map<std::string, const gfx::Image*> existing_thumbnail_map;
  for (const CameraRollItem& item : current_items) {
    existing_thumbnail_map[item.metadata().key()] = &item.thumbnail();
  }

  for (DecodeRequest& request : pending_requests_) {
    if (!request.GetEncodedThumbnail().empty()) {
      // Thumbnail of this item is sent by the phone either because it is new or
      // has been changed.
      decoder_delegate_->DecodeThumbnail(
          request, base::BindOnce(
                       &CameraRollThumbnailDecoderImpl::OnThumbnailDecoded,
                       weak_ptr_factory_.GetWeakPtr(), request.GetMetadata()));
    } else if (existing_thumbnail_map.contains(request.GetMetadata().key())) {
      // Existing items that have not changed should have their thumbnails
      // already decoded.
      request.CompleteWithExistingImage(
          *(existing_thumbnail_map.at(request.GetMetadata().key())));
    } else {
      // Thumbnail for this item is not already available but it is not sent by
      // the phone. Most likely that this is an outdated response. Ignore it and
      // wait for the next response.
      CancelPendingRequests();
      return;
    }
  }
  // Check if all requests are already completed at this point.
  CheckPendingThumbnailRequests();
}

void CameraRollThumbnailDecoderImpl::OnThumbnailDecoded(
    const proto::CameraRollItemMetadata& metadata,
    const SkBitmap& thumbnail_bitmap) {
  for (DecodeRequest& request : pending_requests_) {
    if (request.GetMetadata().key() != metadata.key()) {
      continue;
    }

    request.CompleteWithDecodedBitmap(thumbnail_bitmap);
    CheckPendingThumbnailRequests();
    break;
  }
}

void CameraRollThumbnailDecoderImpl::CheckPendingThumbnailRequests() {
  for (const DecodeRequest& request : pending_requests_) {
    if (!request.is_completed()) {
      return;
    }
  }

  // All pending requests have been completed. Replace current items with this
  // new batch.
  std::vector<CameraRollItem> new_items;
  for (const DecodeRequest& request : pending_requests_) {
    // The decoded thumbnail image can be null if the thumbnail bytes cannot be
    // decoded.
    if (!request.decoded_thumbnail().IsEmpty()) {
      new_items.emplace_back(request.GetMetadata(),
                             request.decoded_thumbnail());
    } else {
      PA_LOG(ERROR) << "Failed to decode thumbnail for file "
                    << request.GetMetadata().file_name();
    }
  }
  pending_requests_.clear();

  std::move(pending_callback_).Run(BatchDecodeResult::kCompleted, new_items);
}

void CameraRollThumbnailDecoderImpl::CancelPendingRequests() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  pending_requests_.clear();
  if (!pending_callback_.is_null()) {
    std::vector<CameraRollItem> empty_items;
    std::move(pending_callback_)
        .Run(BatchDecodeResult::kCancelled, empty_items);
  }
}

}  // namespace phonehub
}  // namespace ash
