// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"

namespace dom_distiller {

DistillerFactoryImpl::DistillerFactoryImpl(const DistillerOptions& options)
    : options_(options) {}

DistillerFactoryImpl::~DistillerFactoryImpl() = default;

std::unique_ptr<Distiller> DistillerFactoryImpl::CreateDistiller() {
  // This default implementation has the same behavior for all URLs.
  return std::make_unique<DistillerImpl>(options_);
}

DistillerImpl::DistillerImpl(const DistillerOptions& options)
    : options_(options) {}

DistillerImpl::~DistillerImpl() {
  DCHECK(destruction_allowed_);
}

void DistillerImpl::DistillPage(const GURL& url,
                                std::unique_ptr<DistillerPage> distiller_page,
                                DistillationFinishedCallback finished_cb) {
  DCHECK(destruction_allowed_);
  DCHECK(finished_cb_.is_null());
  DCHECK(url.is_valid());
  distiller_page_ = std::move(distiller_page);
  finished_cb_ = std::move(finished_cb);

  distiller_page_->DistillPage(
      url, options_,
      base::BindOnce(&DistillerImpl::OnPageDistillationFinished,
                     weak_factory_.GetWeakPtr(), url));
}

void DistillerImpl::OnPageDistillationFinished(
    const GURL& page_url,
    std::unique_ptr<DistilledPageProto> distilled_page,
    DistillationParseResult result) {
  distiller_page_.reset();
  DCHECK(!finished_cb_.is_null());

  auto article_proto = std::make_unique<DistilledArticleProto>();
  if (result == DistillationParseResult::kSuccess) {
    DCHECK(distilled_page);
    distilled_page->set_url(page_url.spec());
    DCHECK(distilled_page->has_word_count());
    base::UmaHistogramCounts100000("DomDistiller.WordCount",
                                   distilled_page->word_count());
    article_proto->set_title(distilled_page->title());
    *article_proto->add_pages() = std::move(*distilled_page);
  }

  base::AutoReset<bool> dont_delete_this_in_callback(&destruction_allowed_,
                                                     false);
  std::move(finished_cb_).Run(std::move(article_proto), result);
}

}  // namespace dom_distiller
