// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "chromeos/ash/components/emoji/gif_tenor_api_fetcher.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class EmojiUI;

class EmojiPageHandler : public emoji_picker::mojom::PageHandler {
 public:
  EmojiPageHandler(
      mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver,
      content::WebUI* web_ui,
      EmojiUI* webui_controller,
      bool incognito_mode,
      bool no_text_field,
      emoji_picker::mojom::Category initial_category,
      const std::string& initial_query);
  EmojiPageHandler(const EmojiPageHandler&) = delete;
  EmojiPageHandler& operator=(const EmojiPageHandler&) = delete;
  ~EmojiPageHandler() override;

  // emoji_picker::mojom::PageHandler:
  void ShowUI() override;
  void InsertEmoji(const std::string& emoji_to_insert,
                   bool is_variant,
                   int16_t search_length) override;
  void IsIncognitoTextField(IsIncognitoTextFieldCallback callback) override;
  void GetFeatureList(GetFeatureListCallback callback) override;
  void GetCategories(GetCategoriesCallback callback) override;
  void GetFeaturedGifs(const std::optional<std::string>& pos,
                       GetFeaturedGifsCallback callback) override;
  void SearchGifs(const std::string& query,
                  const std::optional<std::string>& pos,
                  SearchGifsCallback callback) override;
  void GetGifsByIds(const std::vector<std::string>& ids,
                    GetGifsByIdsCallback callback) override;
  void InsertGif(const GURL& gif) override;
  void OnUiFullyLoaded() override;
  void GetInitialCategory(GetInitialCategoryCallback callback) override;
  void GetInitialQuery(GetInitialQueryCallback callback) override;
  void UpdateHistoryInPrefs(
      emoji_picker::mojom::Category category,
      std::vector<emoji_picker::mojom::HistoryItemPtr> history) override;
  void UpdatePreferredVariantsInPrefs(
      std::vector<emoji_picker::mojom::EmojiVariantPtr> preferred_variants)
      override;
  void GetHistoryFromPrefs(emoji_picker::mojom::Category category,
                           GetHistoryFromPrefsCallback callback) override;

 private:
  mojo::Receiver<emoji_picker::mojom::PageHandler> receiver_;

  base::TimeTicks shown_time_;
  const raw_ptr<EmojiUI> webui_controller_;
  bool gif_support_enabled_;
  bool incognito_mode_;
  bool no_text_field_;
  GifTenorApiFetcher gif_tenor_api_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  emoji_picker::mojom::Category initial_category_;
  std::string initial_query_;
  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_PAGE_HANDLER_H_
