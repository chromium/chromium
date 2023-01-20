// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace ash {

class GifTenorApiFetcher {
 public:
  GifTenorApiFetcher();

  ~GifTenorApiFetcher();

  using TenorGifsApiCallback =
      base::OnceCallback<void(emoji_picker::mojom::TenorGifResponsePtr)>;

  // Fetch tenor API Categories endpoint
  void FetchCategories(
      emoji_picker::mojom::PageHandler::GetCategoriesCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Fetch tenor API Featured endpoint
  void FetchFeaturedGifs(
      TenorGifsApiCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const absl::optional<std::string>& pos);

  // Fetch tenor API Search endpoint
  void FetchGifSearch(
      TenorGifsApiCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& query,
      const absl::optional<std::string>& pos);

  // Fetch tenor API Posts endpoint
  void FetchGifsByIds(
      emoji_picker::mojom::PageHandler::GetGifsByIdsCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::vector<std::string>& ids);

 private:
  std::unique_ptr<EndpointFetcher> endpoint_fetcher_;
  base::WeakPtrFactory<GifTenorApiFetcher> weak_ptr_factory_{this};
  GURL GetURL(const char* endpoint, const absl::optional<std::string>& pos);

  void FetchCategoriesResponseHandler(
      emoji_picker::mojom::PageHandler::GetCategoriesCallback callback,
      std::unique_ptr<EndpointResponse> response);

  void OnCategoriesJsonParsed(
      emoji_picker::mojom::PageHandler::GetCategoriesCallback callback,
      data_decoder::DataDecoder::ValueOrError result);

  void TenorGifsApiResponseHandler(TenorGifsApiCallback callback,
                                   std::unique_ptr<EndpointResponse> response);

  void OnGifsJsonParsed(TenorGifsApiCallback callback,
                        data_decoder::DataDecoder::ValueOrError result);

  void FetchGifsByIdsResponseHandler(
      emoji_picker::mojom::PageHandler::GetGifsByIdsCallback callback,
      std::unique_ptr<EndpointResponse> response);

  void OnGifsByIdsJsonParsed(
      emoji_picker::mojom::PageHandler::GetGifsByIdsCallback callback,
      data_decoder::DataDecoder::ValueOrError result);

  std::vector<emoji_picker::mojom::GifResponsePtr> ParseGifs(
      const base::Value::List* results);

  const base::Value::List* FindList(
      data_decoder::DataDecoder::ValueOrError& result,
      const std::string& key);

  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& annotation_tag);
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_GIF_TENOR_API_FETCHER_H_
