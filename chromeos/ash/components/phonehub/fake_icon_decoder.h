// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ICON_DECODER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ICON_DECODER_H_

#include "base/functional/callback.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"

namespace ash {
namespace phonehub {

class FakeIconDecoder : public IconDecoder {
 public:
  FakeIconDecoder();

  FakeIconDecoder(const FakeIconDecoder&) = delete;
  FakeIconDecoder& operator=(const FakeIconDecoder&) = delete;
  ~FakeIconDecoder() override;

  std::vector<DecodingData>* GetLastMutableDecodeItems();
  void FinishLastCall();

  // IconDecoder:
  void BatchDecode(
      std::unique_ptr<std::vector<DecodingData>> decode_items,
      base::OnceCallback<void(std::unique_ptr<std::vector<DecodingData>>)>
          finished_callback) override;

 private:
  std::unique_ptr<std::vector<DecodingData>> last_decode_items_;
  base::OnceCallback<void(std::unique_ptr<std::vector<DecodingData>>)>
      last_finished_callback_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ICON_DECODER_H_
