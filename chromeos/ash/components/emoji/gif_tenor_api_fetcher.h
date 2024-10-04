// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EMOJI_GIF_TENOR_API_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_EMOJI_GIF_TENOR_API_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/emoji/tenor_types.mojom.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

struct EndpointResponse;
class EndpointFetcher;
class GURL;

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace ash {

class GifTenorApiFetcher {
 public:
  // Used in tests to mock a creation of the endpoint_fetcher
  using EndpointFetcherCreator =
      base::RepeatingCallback<std::unique_ptr<EndpointFetcher>(
          const scoped_refptr<network::SharedURLLoaderFactory>
              url_loader_factory,
          const GURL& url,
          const net::NetworkTrafficAnnotationTag& annotation_tag)>;

  GifTenorApiFetcher();

  explicit GifTenorApiFetcher(EndpointFetcherCreator endpoint_fetcher_creator);

  ~GifTenorApiFetcher();

  using GetCategoriesCallback =
      base::OnceCallback<void(tenor::mojom::Status,
                              const std::vector<std::string>& categories)>;
  using TenorGifsApiCallback =
      base::OnceCallback<void(tenor::mojom::Status,
                              tenor::mojom::PaginatedGifResponsesPtr)>;
  using GetGifsByIdsCallback = base::OnceCallback<void(
      tenor::mojom::Status,
      std::vector<tenor::mojom::GifResponsePtr> selected_gifs)>;

  // Fetch tenor API Categories endpoint
  void FetchCategories(
      GetCategoriesCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Fetch tenor API Featured endpoint
  void FetchFeaturedGifs(
      TenorGifsApiCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::optional<std::string>& pos);

  // Fetch tenor API Search endpoint
  void FetchGifSearch(
      TenorGifsApiCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& query,
      const std::optional<std::string>& pos,
      std::optional<int> limit = std::nullopt);

  // Fetch tenor API Search endpoint. Returns the `EndpointFetcher` used for the
  // request, which will cancel the network request once it is deleted.
  std::unique_ptr<EndpointFetcher> FetchGifSearchCancellable(
      TenorGifsApiCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view query,
      const std::optional<std::string>& pos,
      std::optional<int> limit = std::nullopt);

  // Fetch tenor API Posts endpoint
  void FetchGifsByIds(
      GetGifsByIdsCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::vector<std::string>& ids);

 private:
  const EndpointFetcherCreator endpoint_fetcher_creator_;
  base::WeakPtrFactory<GifTenorApiFetcher> weak_ptr_factory_{this};

  void FetchCategoriesResponseHandler(
      GetCategoriesCallback callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> response);

  void OnCategoriesJsonParsed(GetCategoriesCallback callback,
                              data_decoder::DataDecoder::ValueOrError result);

  void TenorGifsApiResponseHandler(
      TenorGifsApiCallback callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> response);

  void OnGifsJsonParsed(TenorGifsApiCallback callback,
                        data_decoder::DataDecoder::ValueOrError result);

  void FetchGifsByIdsResponseHandler(
      GetGifsByIdsCallback callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> response);

  void OnGifsByIdsJsonParsed(GetGifsByIdsCallback callback,
                             data_decoder::DataDecoder::ValueOrError result);
};
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_EMOJI_GIF_TENOR_API_FETCHER_H_
