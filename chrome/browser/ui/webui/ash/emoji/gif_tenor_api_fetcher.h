// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"

namespace ash {

class GifTenorApiFetcher {
 public:
  GifTenorApiFetcher();

  ~GifTenorApiFetcher();

  // Fetch tenor API categories endpoint
  void FetchCategories(
      emoji_picker::mojom::PageHandler::GetCategoriesCallback callback,
      Profile* profile);

  void FetchCategoriesResponseHandler(
      emoji_picker::mojom::PageHandler::GetCategoriesCallback callback,
      std::unique_ptr<EndpointResponse> response);

 private:
  std::unique_ptr<EndpointFetcher> endpoint_fetcher_;
  base::WeakPtrFactory<GifTenorApiFetcher> weak_ptr_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_
