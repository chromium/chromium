// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/article_distillation_update.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/distiller_url_fetcher.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "url/gurl.h"

namespace dom_distiller {

class DistillerImpl;

class Distiller {
 public:
  using DistillationFinishedCallback =
      base::OnceCallback<void(std::unique_ptr<DistilledArticleProto>)>;
  using DistillationUpdateCallback =
      base::RepeatingCallback<void(const ArticleDistillationUpdate&)>;

  virtual ~Distiller() = default;

  // Distills a page, and asynchronously returns the article HTML to the
  // supplied |finished_cb| callback. |update_cb| is invoked whenever article
  // under distillation is updated with more data.
  // E.g. when distilling a 2 page article, |update_cb| may be invoked each time
  // a distilled page is added and |finished_cb| will be invoked once
  // distillation is completed.
  virtual void DistillPage(const GURL& url,
                           std::unique_ptr<DistillerPage> distiller_page,
                           DistillationFinishedCallback finished_cb,
                           const DistillationUpdateCallback& update_cb) = 0;
};

class DistillerFactory {
 public:
  virtual std::unique_ptr<Distiller> CreateDistillerForUrl(const GURL& url) = 0;
  virtual ~DistillerFactory() = default;
};

// Factory for creating a Distiller.
class DistillerFactoryImpl : public DistillerFactory {
 public:
  DistillerFactoryImpl(
      std::unique_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory,
      const dom_distiller::proto::DomDistillerOptions& dom_distiller_options);
  ~DistillerFactoryImpl() override;
  std::unique_ptr<Distiller> CreateDistillerForUrl(const GURL& url) override;

 private:
  std::unique_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory_;
  dom_distiller::proto::DomDistillerOptions dom_distiller_options_;
};

// Distills a article from a page and associated pages.
class DistillerImpl : public Distiller {
 public:
  DistillerImpl(
      const DistillerURLFetcherFactory& distiller_url_fetcher_factory,
      const dom_distiller::proto::DomDistillerOptions& dom_distiller_options);
  ~DistillerImpl() override;

  void DistillPage(const GURL& url,
                   std::unique_ptr<DistillerPage> distiller_page,
                   DistillationFinishedCallback finished_cb,
                   const DistillationUpdateCallback& update_cb) override;

  void SetMaxNumPagesInArticle(size_t max_num_pages);

  static bool DoesFetchImages();

  DistillerImpl(const DistillerImpl&) = delete;
  DistillerImpl& operator=(const DistillerImpl&) = delete;

 private:
  // In case of multiple pages, the Distiller maintains state of multiple pages
  // as page numbers relative to the page number where distillation started.
  // E.g. if distillation starts at page 2 for a 3 page article. The relative
  // page numbers assigned to pages will be [-1,0,1].

  // Class representing the state of a page under distillation.
  struct DistilledPageData {
    DistilledPageData();
    virtual ~DistilledPageData();
    // Relative page number of the page.
    int page_num;
    std::vector<std::unique_ptr<DistillerURLFetcher>> image_fetchers_;
    scoped_refptr<base::RefCountedData<DistilledPageProto>>
        distilled_page_proto;

    DistilledPageData(const DistilledPageData&) = delete;
    DistilledPageData& operator=(const DistilledPageData&) = delete;
  };

  void OnFetchImageDone(int page_num,
                        DistillerURLFetcher* url_fetcher,
                        const std::string& id,
                        const std::string& original_url,
                        const std::string& response);

  void OnPageDistillationFinished(
      int page_num,
      const GURL& page_url,
      std::unique_ptr<proto::DomDistillerResult> distilled_page,
      bool distillation_successful);

  virtual void MaybeFetchImage(int page_num,
                               const std::string& image_id,
                               const std::string& image_url);

  // Distills the next page.
  void DistillNextPage();

  // Adds the |url| to |pages_to_be_distilled| if |page_num| is a valid relative
  // page number and |url| is valid. Ignores duplicate pages and urls.
  void AddToDistillationQueue(int page_num, const GURL& url);

  // Check if |page_num| is a valid relative page number, i.e. page with
  // |page_num| is either under distillation or has already completed
  // distillation.
  bool IsPageNumberInUse(int page_num) const;

  bool AreAllPagesFinished() const;

  // Total number of pages in the article that the distiller knows of, this
  // includes pages that are pending distillation.
  size_t TotalPageCount() const;

  // Runs |finished_cb_| if all distillation callbacks and image fetches are
  // complete.
  void RunDistillerCallbackIfDone();

  // Checks if page |distilled_page_data| has finished distillation, including
  // all image fetches.
  void AddPageIfDone(int page_num);

  DistilledPageData* GetPageAtIndex(size_t index) const;

  // Create an ArticleDistillationUpdate for the current distillation
  // state.
  const ArticleDistillationUpdate CreateDistillationUpdate() const;

  const raw_ref<const DistillerURLFetcherFactory>
      distiller_url_fetcher_factory_;
  std::unique_ptr<DistillerPage> distiller_page_;

  dom_distiller::proto::DomDistillerOptions dom_distiller_options_;
  DistillationFinishedCallback finished_cb_;
  DistillationUpdateCallback update_cb_;

  // Set of pages that are under distillation or have finished distillation.
  // |started_pages_index_| and |finished_pages_index_| maintains the mapping
  // from page number to the indices in |pages_|.
  std::vector<std::unique_ptr<DistilledPageData>> pages_;

  // Maps page numbers of finished pages to the indices in |pages_|.
  std::map<int, size_t> finished_pages_index_;

  // Maps page numbers of pages under distillation to the indices in |pages_|.
  // If a page is |started_pages_| that means it is still waiting for an action
  // (distillation or image fetch) to finish.
  std::unordered_map<int, size_t> started_pages_index_;

  // The list of pages that are still waiting for distillation to start.
  // This is a map, to make distiller prefer distilling lower page numbers
  // first.
  std::map<int, GURL> waiting_pages_;

  // Set to keep track of which urls are already seen by the distiller. Used to
  // prevent distiller from distilling the same url twice.
  std::unordered_set<std::string> seen_urls_;

  size_t max_pages_in_article_;

  bool destruction_allowed_;

  base::WeakPtrFactory<DistillerImpl> weak_factory_{this};
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
