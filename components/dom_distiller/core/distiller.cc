// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/distiller_url_fetcher.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"

namespace {
// Maximum number of distilled pages in an article.
const size_t kMaxPagesInArticle = 32;
}  // namespace

namespace dom_distiller {

DistillerFactoryImpl::DistillerFactoryImpl(
    std::unique_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory,
    const dom_distiller::proto::DomDistillerOptions& dom_distiller_options)
    : distiller_url_fetcher_factory_(std::move(distiller_url_fetcher_factory)),
      dom_distiller_options_(dom_distiller_options) {}

DistillerFactoryImpl::~DistillerFactoryImpl() = default;

std::unique_ptr<Distiller> DistillerFactoryImpl::CreateDistillerForUrl(
    const GURL& unused) {
  // This default implementation has the same behavior for all URLs.
  std::unique_ptr<DistillerImpl> distiller(new DistillerImpl(
      *distiller_url_fetcher_factory_, dom_distiller_options_));
  return std::move(distiller);
}

DistillerImpl::DistilledPageData::DistilledPageData() {}

DistillerImpl::DistilledPageData::~DistilledPageData() {}

DistillerImpl::DistillerImpl(
    const DistillerURLFetcherFactory& distiller_url_fetcher_factory,
    const dom_distiller::proto::DomDistillerOptions& dom_distiller_options)
    : distiller_url_fetcher_factory_(distiller_url_fetcher_factory),
      dom_distiller_options_(dom_distiller_options),
      max_pages_in_article_(kMaxPagesInArticle),
      destruction_allowed_(true) {}

DistillerImpl::~DistillerImpl() {
  DCHECK(destruction_allowed_);
}

bool DistillerImpl::DoesFetchImages() {
// Only iOS makes use of the fetched image data.
#if BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

void DistillerImpl::SetMaxNumPagesInArticle(size_t max_num_pages) {
  max_pages_in_article_ = max_num_pages;
}

bool DistillerImpl::AreAllPagesFinished() const {
  return started_pages_index_.empty() && waiting_pages_.empty();
}

size_t DistillerImpl::TotalPageCount() const {
  return waiting_pages_.size() + started_pages_index_.size() +
         finished_pages_index_.size();
}

void DistillerImpl::AddToDistillationQueue(int page_num, const GURL& url) {
  if (!IsPageNumberInUse(page_num) && url.is_valid() &&
      TotalPageCount() < max_pages_in_article_ &&
      seen_urls_.find(url.spec()) == seen_urls_.end()) {
    waiting_pages_[page_num] = url;
  }
}

bool DistillerImpl::IsPageNumberInUse(int page_num) const {
  return waiting_pages_.find(page_num) != waiting_pages_.end() ||
         started_pages_index_.find(page_num) != started_pages_index_.end() ||
         finished_pages_index_.find(page_num) != finished_pages_index_.end();
}

DistillerImpl::DistilledPageData* DistillerImpl::GetPageAtIndex(
    size_t index) const {
  DCHECK_LT(index, pages_.size());
  DistilledPageData* page_data = pages_[index].get();
  DCHECK(page_data);
  return page_data;
}

void DistillerImpl::DistillPage(const GURL& url,
                                std::unique_ptr<DistillerPage> distiller_page,
                                DistillationFinishedCallback finished_cb,
                                const DistillationUpdateCallback& update_cb) {
  DCHECK(AreAllPagesFinished());
  distiller_page_ = std::move(distiller_page);
  finished_cb_ = std::move(finished_cb);
  update_cb_ = update_cb;

  AddToDistillationQueue(0, url);
  DistillNextPage();
}

void DistillerImpl::DistillNextPage() {
  if (!waiting_pages_.empty()) {
    auto front = waiting_pages_.begin();
    int page_num = front->first;
    const GURL url = std::move(front->second);

    waiting_pages_.erase(front);
    DCHECK(url.is_valid());
    DCHECK(started_pages_index_.find(page_num) == started_pages_index_.end());
    DCHECK(finished_pages_index_.find(page_num) == finished_pages_index_.end());
    seen_urls_.insert(url.spec());
    pages_.push_back(std::make_unique<DistilledPageData>());
    started_pages_index_[page_num] = pages_.size() - 1;

    // TODO(gilmanmh): Investigate whether this needs to be
    // base::BindRepeating() or if base::BindOnce() can be used instead.
    distiller_page_->DistillPage(
        url, dom_distiller_options_,
        base::BindRepeating(&DistillerImpl::OnPageDistillationFinished,
                            weak_factory_.GetWeakPtr(), page_num, url));
  }
}

void DistillerImpl::OnPageDistillationFinished(
    int page_num,
    const GURL& page_url,
    std::unique_ptr<proto::DomDistillerResult> distiller_result,
    bool distillation_successful) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  if (!distillation_successful) {
    started_pages_index_.erase(page_num);
    RunDistillerCallbackIfDone();
    return;
  }

  DCHECK(distiller_result);
  CHECK_LT(started_pages_index_[page_num], pages_.size())
      << "started_pages_index_[" << page_num
      << "] (=" << started_pages_index_[page_num] << ") is out of range.";
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  CHECK(page_data) << "GetPageAtIndex(started_pages_index_[" << page_num
                   << "] (=" << started_pages_index_[page_num]
                   << ")) returns nullptr. pages_.size() = " << pages_.size()
                   << ".";
  page_data->distilled_page_proto =
      new base::RefCountedData<DistilledPageProto>();
  page_data->page_num = page_num;
  if (distiller_result->has_title()) {
    page_data->distilled_page_proto->data.set_title(distiller_result->title());
  }
  page_data->distilled_page_proto->data.set_url(page_url.spec());
  bool content_empty = true;
  if (distiller_result->has_distilled_content() &&
      distiller_result->distilled_content().has_html()) {
    page_data->distilled_page_proto->data.set_html(
        distiller_result->distilled_content().html());
    if (!distiller_result->distilled_content().html().empty()) {
      content_empty = false;
    }
  }

  if (distiller_result->has_debug_info() &&
      distiller_result->debug_info().has_log()) {
    page_data->distilled_page_proto->data.mutable_debug_info()->set_log(
        distiller_result->debug_info().log());
  }

  if (distiller_result->has_text_direction()) {
    page_data->distilled_page_proto->data.set_text_direction(
        distiller_result->text_direction());
  } else {
    page_data->distilled_page_proto->data.set_text_direction("auto");
  }

  if (distiller_result->has_pagination_info()) {
    const proto::PaginationInfo& pagination_info =
        distiller_result->pagination_info();
    // Skip the next page if the first page is empty.
    if (pagination_info.has_next_page() && (page_num != 0 || !content_empty)) {
      GURL next_page_url(pagination_info.next_page());
      if (next_page_url.is_valid()) {
        // The pages should be in same origin.
        if (next_page_url.DeprecatedGetOriginAsURL() ==
            page_url.DeprecatedGetOriginAsURL()) {
          AddToDistillationQueue(page_num + 1, next_page_url);
          page_data->distilled_page_proto->data.mutable_pagination_info()
              ->set_next_page(next_page_url.spec());
        }
      }
    }

    if (pagination_info.has_prev_page()) {
      GURL prev_page_url(pagination_info.prev_page());
      if (prev_page_url.is_valid()) {
        if (prev_page_url.DeprecatedGetOriginAsURL() ==
            page_url.DeprecatedGetOriginAsURL()) {
          AddToDistillationQueue(page_num - 1, prev_page_url);
          page_data->distilled_page_proto->data.mutable_pagination_info()
              ->set_prev_page(prev_page_url.spec());
        }
      }
    }

    if (pagination_info.has_canonical_page()) {
      GURL canonical_page_url(pagination_info.canonical_page());
      if (canonical_page_url.is_valid()) {
        page_data->distilled_page_proto->data.mutable_pagination_info()
            ->set_canonical_page(canonical_page_url.spec());
      }
    }
  }

  for (int img_num = 0; img_num < distiller_result->content_images_size();
       ++img_num) {
    std::string image_id = base::NumberToString(page_num + 1) + "_" +
                           base::NumberToString(img_num);
    MaybeFetchImage(page_num, image_id,
                    distiller_result->content_images(img_num).url());
  }

  AddPageIfDone(page_num);
  DistillNextPage();
}

void DistillerImpl::MaybeFetchImage(int page_num,
                                    const std::string& image_id,
                                    const std::string& image_url) {
  if (!GURL(image_url).is_valid())
    return;
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);

  if (!DoesFetchImages()) {
    DistilledPageProto_Image* image =
        page_data->distilled_page_proto->data.add_image();
    image->set_name(image_id);
    image->set_url(image_url);
    return;
  }

  DistillerURLFetcher* fetcher =
      distiller_url_fetcher_factory_->CreateDistillerURLFetcher();
  page_data->image_fetchers_.push_back(base::WrapUnique(fetcher));

  // TODO(gilmanmh): Investigate whether this needs to be base::BindRepeating()
  // or if base::BindOnce() can be used instead.
  fetcher->FetchURL(
      image_url,
      base::BindRepeating(&DistillerImpl::OnFetchImageDone,
                          weak_factory_.GetWeakPtr(), page_num,
                          base::Unretained(fetcher), image_id, image_url));
}

void DistillerImpl::OnFetchImageDone(int page_num,
                                     DistillerURLFetcher* url_fetcher,
                                     const std::string& id,
                                     const std::string& original_url,
                                     const std::string& response) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  DCHECK(page_data->distilled_page_proto);
  DCHECK(url_fetcher);
  auto fetcher_it =
      base::ranges::find(page_data->image_fetchers_, url_fetcher,
                         &std::unique_ptr<DistillerURLFetcher>::get);

  DCHECK(fetcher_it != page_data->image_fetchers_.end());
  // Delete the |url_fetcher| by DeleteSoon since the OnFetchImageDone
  // callback is invoked by the |url_fetcher|.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(*fetcher_it));
  page_data->image_fetchers_.erase(fetcher_it);

  DistilledPageProto_Image* image =
      page_data->distilled_page_proto->data.add_image();
  image->set_name(id);
  image->set_data(response);
  image->set_url(original_url);

  AddPageIfDone(page_num);
}

void DistillerImpl::AddPageIfDone(int page_num) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DCHECK(finished_pages_index_.find(page_num) == finished_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  if (page_data->image_fetchers_.empty()) {
    finished_pages_index_[page_num] = started_pages_index_[page_num];
    started_pages_index_.erase(page_num);
    const ArticleDistillationUpdate& article_update =
        CreateDistillationUpdate();
    DCHECK_EQ(article_update.GetPagesSize(), finished_pages_index_.size());
    update_cb_.Run(article_update);
    RunDistillerCallbackIfDone();
  }
}

const ArticleDistillationUpdate DistillerImpl::CreateDistillationUpdate()
    const {
  bool has_prev_page = false;
  bool has_next_page = false;
  if (!finished_pages_index_.empty()) {
    int prev_page_num = finished_pages_index_.begin()->first - 1;
    int next_page_num = finished_pages_index_.rbegin()->first + 1;
    has_prev_page = IsPageNumberInUse(prev_page_num);
    has_next_page = IsPageNumberInUse(next_page_num);
  }

  std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto>>
      update_pages;
  for (auto it = finished_pages_index_.begin();
       it != finished_pages_index_.end(); ++it) {
    update_pages.push_back(pages_[it->second]->distilled_page_proto);
  }
  return ArticleDistillationUpdate(update_pages, has_next_page, has_prev_page);
}

void DistillerImpl::RunDistillerCallbackIfDone() {
  DCHECK(!finished_cb_.is_null());
  if (AreAllPagesFinished()) {
    bool first_page = true;
    std::unique_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    // Stitch the pages back into the article.
    for (auto it = finished_pages_index_.begin();
         it != finished_pages_index_.end();) {
      DistilledPageData* page_data = GetPageAtIndex(it->second);
      *(article_proto->add_pages()) = page_data->distilled_page_proto->data;

      if (first_page) {
        article_proto->set_title(page_data->distilled_page_proto->data.title());
        first_page = false;
      }

      finished_pages_index_.erase(it++);
    }

    pages_.clear();
    DCHECK_LE(static_cast<size_t>(article_proto->pages_size()),
              max_pages_in_article_);

    DCHECK(pages_.empty());
    DCHECK(finished_pages_index_.empty());

    base::AutoReset<bool> dont_delete_this_in_callback(&destruction_allowed_,
                                                       false);
    std::move(finished_cb_).Run(std::move(article_proto));
  }
}

}  // namespace dom_distiller
