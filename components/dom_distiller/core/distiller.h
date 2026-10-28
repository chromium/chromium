// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/distiller_options.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "url/gurl.h"

namespace dom_distiller {

class ArticleDistillationUpdate;
class DistilledPageProto;

class Distiller {
 public:
  using DistillationFinishedCallback =
      base::OnceCallback<void(std::unique_ptr<DistilledArticleProto> proto,
                              DistillationParseResult result)>;
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
  virtual std::unique_ptr<Distiller> CreateDistiller() = 0;
  virtual ~DistillerFactory() = default;
};

// Factory for creating a Distiller.
class DistillerFactoryImpl : public DistillerFactory {
 public:
  explicit DistillerFactoryImpl(const DistillerOptions& options);
  ~DistillerFactoryImpl() override;
  std::unique_ptr<Distiller> CreateDistiller() override;

 private:
  DistillerOptions options_;
};

// Distills an article from a page.
class DistillerImpl : public Distiller {
 public:
  explicit DistillerImpl(const DistillerOptions& options);
  ~DistillerImpl() override;

  void DistillPage(const GURL& url,
                   std::unique_ptr<DistillerPage> distiller_page,
                   DistillationFinishedCallback finished_cb,
                   const DistillationUpdateCallback& update_cb) override;

  DistillerImpl(const DistillerImpl&) = delete;
  DistillerImpl& operator=(const DistillerImpl&) = delete;

 private:
  void OnPageDistillationFinished(
      const GURL& page_url,
      std::unique_ptr<DistilledPageProto> distilled_page,
      DistillationParseResult result);

  std::unique_ptr<DistillerPage> distiller_page_;

  DistillerOptions options_;
  DistillationFinishedCallback finished_cb_;

  bool destruction_allowed_ = true;

  base::WeakPtrFactory<DistillerImpl> weak_factory_{this};
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
