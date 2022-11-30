// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTINUOUS_SEARCH_RENDERER_SEARCH_RESULT_EXTRACTOR_IMPL_H_
#define COMPONENTS_CONTINUOUS_SEARCH_RENDERER_SEARCH_RESULT_EXTRACTOR_IMPL_H_

#include <vector>

#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace continuous_search {

// Implementation of `mojom::SearchResultExtractor`.
class SearchResultExtractorImpl : public content::RenderFrameObserver,
                                  mojom::SearchResultExtractor {
 public:
  static SearchResultExtractorImpl* Create(content::RenderFrame* render_frame);

  ~SearchResultExtractorImpl() override;

  SearchResultExtractorImpl(const SearchResultExtractorImpl&) = delete;
  SearchResultExtractorImpl& operator=(const SearchResultExtractorImpl&) =
      delete;

  void ExtractCurrentSearchResults(
      const std::vector<mojom::ResultType>& result_types,
      ExtractCurrentSearchResultsCallback callback) override;

 private:
  explicit SearchResultExtractorImpl(content::RenderFrame* render_frame);

  void OnDestruct() override;

  void BindSearchResultExtractor(
      mojo::PendingAssociatedReceiver<mojom::SearchResultExtractor> receiver);

  mojo::AssociatedReceiver<mojom::SearchResultExtractor> receiver_{this};

  base::WeakPtrFactory<SearchResultExtractorImpl> weak_ptr_factory_{this};
};

}  // namespace continuous_search

#endif  // COMPONENTS_CONTINUOUS_SEARCH_RENDERER_SEARCH_RESULT_EXTRACTOR_IMPL_H_
