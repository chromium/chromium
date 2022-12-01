// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_icon_decoder.h"

#include <utility>

#include "chromeos/ash/components/phonehub/icon_decoder.h"

namespace ash {
namespace phonehub {

FakeIconDecoder::FakeIconDecoder() = default;
FakeIconDecoder::~FakeIconDecoder() = default;

std::vector<IconDecoder::DecodingData>*
FakeIconDecoder::GetLastMutableDecodeItems() {
  return last_decode_items_.get();
}

void FakeIconDecoder::FinishLastCall() {
  std::move(last_finished_callback_).Run(std::move(last_decode_items_));
}

void FakeIconDecoder::BatchDecode(
    std::unique_ptr<std::vector<DecodingData>> decode_items,
    base::OnceCallback<void(std::unique_ptr<std::vector<DecodingData>>)>
        finished_callback) {
  last_decode_items_ = std::move(decode_items);
  last_finished_callback_ = std::move(finished_callback);
}

}  // namespace phonehub
}  // namespace ash
