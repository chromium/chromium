// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "url/gurl.h"

namespace ash {

class GifTenorApiFetcher {
 public:
  GifTenorApiFetcher();

  ~GifTenorApiFetcher();

  using TenorApiCallback = base::OnceCallback<void(const std::string&)>;

  // Fetch tenor API Categories endpoint
  void FetchCategories(
      TenorApiCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  void FetchCategoriesResponseHandler(
      TenorApiCallback callback,
      std::unique_ptr<EndpointResponse> response);

  // Fetch tenor API Featured endpoint
  void FetchFeaturedGifs(
      TenorApiCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const absl::optional<std::string>& pos);

  // Fetch tenor API Search endpoint
  void FetchGifSearch(
      TenorApiCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& query,
      const absl::optional<std::string>& pos);

 private:
  std::unique_ptr<EndpointFetcher> endpoint_fetcher_;
  base::WeakPtrFactory<GifTenorApiFetcher> weak_ptr_factory_{this};
  GURL GetURL(const char* endpoint, const absl::optional<std::string>& pos);
  void ResponseHandler(TenorApiCallback callback,
                       std::unique_ptr<EndpointResponse> response);
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_
