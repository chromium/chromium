// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_ICON_DECODER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_ICON_DECODER_IMPL_H_

#include "chromeos/ash/components/phonehub/icon_decoder.h"

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace phonehub {

class IconDecoderImpl : public IconDecoder {
 public:
  IconDecoderImpl();
  ~IconDecoderImpl() override;

  void BatchDecode(
      std::unique_ptr<std::vector<DecodingData>> decode_items,
      base::OnceCallback<void(std::unique_ptr<std::vector<DecodingData>>)>
          finished_callback) override;

 private:
  friend class IconDecoderImplTest;
  friend class TestDecoderDelegate;
  friend class PhoneStatusProcessorTest;

  // Delegate class that decodes icons. Can be overridden in tests.
  class DecoderDelegate {
   public:
    DecoderDelegate();
    virtual ~DecoderDelegate();

    virtual void Decode(const DecodingData& data,
                        data_decoder::DecodeImageCallback callback);

   private:
    // The instance of DataDecoder to decode thumbnail images. The underlying
    // service instance is started lazily when needed and torn down when not in
    // use.
    data_decoder::DataDecoder data_decoder_;
  };

  void OnIconDecoded(DecodingData& decoding_data, const SkBitmap& result);
  void OnAllIconsDecoded(
      base::OnceCallback<void(std::unique_ptr<std::vector<DecodingData>>)>
          finished_callback);
  void CancelPendingRequests();

  std::unique_ptr<DecoderDelegate> decoder_delegate_;
  std::unique_ptr<std::vector<DecodingData>> pending_items_;
  base::RepeatingClosure barrier_closure_;

  // Contains weak pointers to callbacks passed to the |DecoderDelegate|.
  base::WeakPtrFactory<IconDecoderImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_ICON_DECODER_IMPL_H_
