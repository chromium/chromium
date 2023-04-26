// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/icon_decoder_impl.h"

#include <functional>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "ubidiimp.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace phonehub {

IconDecoderImpl::DecoderDelegate::DecoderDelegate() = default;

IconDecoderImpl::DecoderDelegate::~DecoderDelegate() = default;

void IconDecoderImpl::DecoderDelegate::Decode(
    const DecodingData& request,
    data_decoder::DecodeImageCallback callback) {
  const std::string& encoded_icon = *request.input_data;
  data_decoder::DecodeImage(
      &data_decoder_, base::as_bytes(base::make_span(encoded_icon)),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(), std::move(callback));
}

IconDecoderImpl::IconDecoderImpl()
    : decoder_delegate_(std::make_unique<DecoderDelegate>()) {}

IconDecoderImpl::~IconDecoderImpl() = default;

void IconDecoderImpl::BatchDecode(
    std::unique_ptr<std::vector<DecodingData>> decode_items,
    base::OnceCallback<void(std::unique_ptr<std::vector<DecodingData>>)>
        finished_callback) {
  CancelPendingRequests();
  pending_items_ = std::move(decode_items);

  barrier_closure_ =
      base::BarrierClosure(pending_items_->size(),
                           base::BindOnce(&IconDecoderImpl::OnAllIconsDecoded,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(finished_callback)));

  // If decode_items is empty, barrier closure must have been called by this
  // point and the pending_items_ pointer must be already reset.
  if (!pending_items_)
    return;
  for (DecodingData& request : *pending_items_) {
    decoder_delegate_->Decode(
        request,
        base::BindOnce(&IconDecoderImpl::OnIconDecoded,
                       weak_ptr_factory_.GetWeakPtr(), std::ref(request)));
  }
}

void IconDecoderImpl::OnAllIconsDecoded(
    base::OnceCallback<void(std::unique_ptr<std::vector<DecodingData>>)>
        finished_callback) {
  std::move(finished_callback).Run(std::move(pending_items_));
}

void IconDecoderImpl::OnIconDecoded(DecodingData& decoding_data,
                                    const SkBitmap& result) {
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(result);

  // If |image_skia| is null, indicating that the data decoder failed to decode
  // the image, the image will be empty, and cannot be made thread safe.
  if (!image_skia.isNull())
    image_skia.MakeThreadSafe();

  decoding_data.result = gfx::Image(image_skia);
  barrier_closure_.Run();
}

void IconDecoderImpl::CancelPendingRequests() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace phonehub
}  // namespace ash
