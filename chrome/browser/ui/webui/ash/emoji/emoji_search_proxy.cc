// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/emoji_search_proxy.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/emoji/emoji_search.h"

namespace ash {

EmojiSearchProxy::EmojiSearchProxy(
    mojo::PendingReceiver<emoji_search::mojom::EmojiSearch> receiver)
    : receiver_(this, std::move(receiver)) {
  // Before enabling flag - move search creation to background thread.
  search_ = std::make_unique<emoji::EmojiSearch>();
}

EmojiSearchProxy::~EmojiSearchProxy() {}

void EmojiSearchProxy::SearchEmoji(const std::string& query,
                                   SearchEmojiCallback callback) {
  CHECK(search_);
  search_->SearchEmoji(query, std::move(callback));
}

}  // namespace ash
