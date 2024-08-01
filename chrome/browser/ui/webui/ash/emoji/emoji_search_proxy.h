// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_SEARCH_PROXY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_SEARCH_PROXY_H_

#include <memory>
#include <vector>

#include "chromeos/ash/components/emoji/emoji_search.h"
#include "chromeos/ash/components/emoji/emoji_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class EmojiSearchProxy : public emoji_search::mojom::EmojiSearch {
 public:
  explicit EmojiSearchProxy(
      mojo::PendingReceiver<emoji_search::mojom::EmojiSearch> receiver);
  ~EmojiSearchProxy() override;

  void SearchEmoji(const std::string& query,
                   const std::vector<std::string>& language_codes,
                   SearchEmojiCallback callback) override;

  void LoadEmojiLanguages(
      const std::vector<std::string>& language_codes) override;

 private:
  mojo::Receiver<emoji_search::mojom::EmojiSearch> receiver_;
  std::unique_ptr<emoji::EmojiSearch> search_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_SEARCH_PROXY_H_
