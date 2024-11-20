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
#include "base/types/expected.h"
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
  enum class Error { kNetError, kHttpError };

  // Used in tests to mock a creation of the endpoint_fetcher
  using EndpointFetcherCreator =
      base::RepeatingCallback<std::unique_ptr<EndpointFetcher>(
          scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
          const GURL& url,
          const net::NetworkTrafficAnnotationTag& annotation_tag)>;
  using GetCategoriesCallback =
      base::OnceCallback<void(base::expected<std::vector<std::string>, Error>)>;
  using TenorGifsApiCallback = base::OnceCallback<void(
      base::expected<tenor::mojom::PaginatedGifResponsesPtr, Error>)>;
  using GetGifsByIdsCallback = base::OnceCallback<void(
      base::expected<std::vector<tenor::mojom::GifResponsePtr>, Error>)>;

  GifTenorApiFetcher();
  explicit GifTenorApiFetcher(EndpointFetcherCreator endpoint_fetcher_creator);
  ~GifTenorApiFetcher();

  // Fetch tenor API Categories endpoint
  void FetchCategories(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GetCategoriesCallback callback);

  // Fetch tenor API Featured endpoint
  void FetchFeaturedGifs(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::optional<std::string>& pos,
      TenorGifsApiCallback callback);

  // Fetch tenor API Search endpoint
  void FetchGifSearch(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& query,
      const std::optional<std::string>& pos,
      std::optional<int> limit,
      TenorGifsApiCallback callback);

  // Fetch tenor API Search endpoint. Returns the `EndpointFetcher` used for the
  // request, which will cancel the network request once it is deleted.
  std::unique_ptr<EndpointFetcher> FetchGifSearchCancellable(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view query,
      const std::optional<std::string>& pos,
      std::optional<int> limit,
      TenorGifsApiCallback callback);

  // Fetch tenor API Posts endpoint
  void FetchGifsByIds(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::vector<std::string>& ids,
      GetGifsByIdsCallback callback);

 private:
  void FetchCategoriesResponseHandler(
      GetCategoriesCallback callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> response);
  void TenorGifsApiResponseHandler(
      TenorGifsApiCallback callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> response);
  void FetchGifsByIdsResponseHandler(
      GetGifsByIdsCallback callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> response);

  const EndpointFetcherCreator endpoint_fetcher_creator_;
  base::WeakPtrFactory<GifTenorApiFetcher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_EMOJI_GIF_TENOR_API_FETCHER_H_
