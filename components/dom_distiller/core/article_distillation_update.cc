// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/article_distillation_update.h"

#include "base/check_op.h"

namespace dom_distiller {

ArticleDistillationUpdate::ArticleDistillationUpdate(
    const std::vector<scoped_refptr<RefCountedPageProto>>& pages,
    bool has_next_page,
    bool has_prev_page)
    : has_next_page_(has_next_page),
      has_prev_page_(has_prev_page),
      pages_(pages) {}

ArticleDistillationUpdate::ArticleDistillationUpdate(
    const ArticleDistillationUpdate& other) = default;

ArticleDistillationUpdate::~ArticleDistillationUpdate() = default;

const DistilledPageProto& ArticleDistillationUpdate::GetDistilledPage(
    size_t index) const {
  DCHECK_GT(pages_.size(), index);
  return pages_[index]->data;
}

}  // namespace dom_distiller
