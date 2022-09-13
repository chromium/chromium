// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_DISTILLATION_UPDATE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_DISTILLATION_UPDATE_H_

#include <stddef.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"

namespace dom_distiller {

// Update about an article that is currently under distillation.
class ArticleDistillationUpdate {
 public:
  typedef base::RefCountedData<DistilledPageProto> RefCountedPageProto;

  ArticleDistillationUpdate(
      const std::vector<scoped_refptr<RefCountedPageProto>>& pages,
      bool has_next_page,
      bool has_prev_page);
  ArticleDistillationUpdate(const ArticleDistillationUpdate& other);
  ~ArticleDistillationUpdate();

  // Returns the  distilled page at |index|.
  const DistilledPageProto& GetDistilledPage(size_t index) const;

  // Returns the size of distilled pages in this update.
  size_t GetPagesSize() const { return pages_.size(); }

  // Returns true, if article has a next page that is currently under
  // distillation and that is not part of the distilled pages included in this
  // update.
  bool HasNextPage() const { return has_next_page_; }

  // Returns true if article has a previous page that is currently under
  // distillation and that is not part of the distilled pages included in this
  // update.
  bool HasPrevPage() const { return has_prev_page_; }

 private:
  bool has_next_page_;
  bool has_prev_page_;
  // Currently available pages.
  std::vector<scoped_refptr<RefCountedPageProto>> pages_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_DISTILLATION_UPDATE_H_
