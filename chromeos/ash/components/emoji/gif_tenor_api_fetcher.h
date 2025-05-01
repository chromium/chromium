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

namespace endpoint_fetcher {
class EndpointFetcher;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace ash {

// TODO: b/368442959: Turn these methods into functions.
class GifTenorApiFetcher {
 public:
  enum class Error { kNetError, kHttpError };

  using GetCategoriesCallback =
      base::OnceCallback<void(base::expected<std::vector<std::string>, Error>)>;
  using TenorGifsApiCallback = base::OnceCallback<void(
      base::expected<tenor::mojom::PaginatedGifResponsesPtr, Error>)>;
  using GetGifsByIdsCallback = base::OnceCallback<void(
      base::expected<std::vector<tenor::mojom::GifResponsePtr>, Error>)>;

  GifTenorApiFetcher();
  ~GifTenorApiFetcher();

  // Fetch tenor API Categories endpoint
  static void FetchCategories(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GetCategoriesCallback callback);

  // Fetch tenor API Featured endpoint
  static void FetchFeaturedGifs(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::optional<std::string>& pos,
      TenorGifsApiCallback callback);

  // Fetch tenor API Search endpoint
  static void FetchGifSearch(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& query,
      const std::optional<std::string>& pos,
      std::optional<int> limit,
      TenorGifsApiCallback callback);

  // Fetch tenor API Search endpoint. Returns the `EndpointFetcher` used for the
  // request, which will cancel the network request once it is deleted.
  static std::unique_ptr<endpoint_fetcher::EndpointFetcher>
  FetchGifSearchCancellable(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view query,
      const std::optional<std::string>& pos,
      std::optional<int> limit,
      TenorGifsApiCallback callback);

  // Fetch tenor API Posts endpoint
  static void FetchGifsByIds(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::vector<std::string>& ids,
      GetGifsByIdsCallback callback);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_EMOJI_GIF_TENOR_API_FETCHER_H_
