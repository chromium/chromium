// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"

namespace {
// Maximum number of distilled pages in an article.
const size_t kMaxPagesInArticle = 32;
}  // namespace

namespace dom_distiller {

DistillerFactoryImpl::DistillerFactoryImpl(const DistillerOptions& options)
    : options_(options) {}

DistillerFactoryImpl::~DistillerFactoryImpl() = default;

std::unique_ptr<Distiller> DistillerFactoryImpl::CreateDistiller() {
  // This default implementation has the same behavior for all URLs.
  std::unique_ptr<DistillerImpl> distiller = std::make_unique<DistillerImpl>(options_);
  return std::move(distiller);
}

DistillerImpl::DistilledPageData::DistilledPageData() = default;

DistillerImpl::DistilledPageData::~DistilledPageData() = default;

DistillerImpl::DistillerImpl(const DistillerOptions& options)
    : options_(options),
      max_pages_in_article_(kMaxPagesInArticle),
      destruction_allowed_(true) {}

DistillerImpl::~DistillerImpl() {
  DCHECK(destruction_allowed_);
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
        url, options_,
        base::BindRepeating(&DistillerImpl::OnPageDistillationFinished,
                            weak_factory_.GetWeakPtr(), page_num, url));
  }
}

void DistillerImpl::OnPageDistillationFinished(
    int page_num,
    const GURL& page_url,
    std::unique_ptr<DistilledPageProto> distilled_page,
    DistillationParseResult result) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  if (result != DistillationParseResult::kSuccess) {
    last_error_ = result;
    started_pages_index_.erase(page_num);
    RunDistillerCallbackIfDone();
    return;
  }

  DCHECK(distilled_page);
  CHECK_LT(started_pages_index_[page_num], pages_.size())
      << "started_pages_index_[" << page_num
      << "] (=" << started_pages_index_[page_num] << ") is out of range.";
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  CHECK(page_data) << "GetPageAtIndex(started_pages_index_[" << page_num
                   << "] (=" << started_pages_index_[page_num]
                   << ")) returns nullptr. pages_.size() = " << pages_.size()
                   << ".";
  distilled_page->set_url(page_url.spec());
  DCHECK(distilled_page->has_word_count());
  base::UmaHistogramCounts100000("DomDistiller.WordCount",
                                 distilled_page->word_count());
  page_data->distilled_page_proto =
      new base::RefCountedData<DistilledPageProto>();
  page_data->distilled_page_proto->data = std::move(*distilled_page);
  page_data->page_num = page_num;

  AddPageIfDone(page_num);
  DistillNextPage();
}

void DistillerImpl::AddPageIfDone(int page_num) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DCHECK(finished_pages_index_.find(page_num) == finished_pages_index_.end());
  finished_pages_index_[page_num] = started_pages_index_[page_num];
  started_pages_index_.erase(page_num);
  const ArticleDistillationUpdate& article_update = CreateDistillationUpdate();
  DCHECK_EQ(article_update.GetPagesSize(), finished_pages_index_.size());
  update_cb_.Run(article_update);
  RunDistillerCallbackIfDone();
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
    std::move(finished_cb_)
        .Run(std::move(article_proto),
             last_error_.value_or(DistillationParseResult::kSuccess));
  }
}

}  // namespace dom_distiller
