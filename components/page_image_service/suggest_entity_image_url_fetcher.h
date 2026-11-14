// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_SERVICE_SUGGEST_ENTITY_IMAGE_URL_FETCHER_H_
#define COMPONENTS_PAGE_IMAGE_SERVICE_SUGGEST_ENTITY_IMAGE_URL_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"

class AutocompleteSchemeClassifier;
class GURL;
class RemoteSuggestionsService;
class SearchTermsData;
class TemplateURL;

namespace network {
class SimpleURLLoader;
}

namespace page_image_service {

// A one-time use object that uses Suggest to get an image URL corresponding
// to `search_query` and `entity_id`. This is a temporary implementation;
// ideally this should be replaced by persisted Suggest-provided entities.
class SuggestEntityImageURLFetcher {
 public:
  // `autocomplete_scheme_classifier` must outlive this fetcher.
  SuggestEntityImageURLFetcher(
      const AutocompleteSchemeClassifier& autocomplete_scheme_classifier,
      mojom::ClientId client_id,
      const std::u16string& search_query,
      const std::string& entity_id);
  SuggestEntityImageURLFetcher(const SuggestEntityImageURLFetcher&) = delete;
  SuggestEntityImageURLFetcher& operator=(const SuggestEntityImageURLFetcher&) =
      delete;
  ~SuggestEntityImageURLFetcher();

  // Calls `callback` with the matching image URL, or an empty URL on failure.
  // If no request can be started, posts the callback asynchronously. The
  // callback may own and destroy this fetcher.
  void Start(const TemplateURL* template_url,
             const SearchTermsData& search_terms_data,
             RemoteSuggestionsService* remote_suggestions_service,
             base::OnceCallback<void(const GURL&)> callback);

 private:
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         int response_code,
                         std::optional<std::string> response_body);

  const raw_ref<const AutocompleteSchemeClassifier>
      autocomplete_scheme_classifier_;
  mojom::ClientId client_id_;
  const std::u16string search_query_;
  const std::string entity_id_;
  base::OnceCallback<void(const GURL&)> callback_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::WeakPtrFactory<SuggestEntityImageURLFetcher> weak_factory_{this};
};

}  // namespace page_image_service

#endif  // COMPONENTS_PAGE_IMAGE_SERVICE_SUGGEST_ENTITY_IMAGE_URL_FETCHER_H_
