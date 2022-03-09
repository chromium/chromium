// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_IMPL_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/browsing_topics/browsing_topics_service.h"

namespace browsing_topics {

// A profile keyed service for providing the topics to a requesting context or
// to other internal components (e.g. UX).
class BrowsingTopicsServiceImpl : public BrowsingTopicsService {
 public:
  BrowsingTopicsServiceImpl(const BrowsingTopicsServiceImpl&) = delete;
  BrowsingTopicsServiceImpl& operator=(const BrowsingTopicsServiceImpl&) =
      delete;
  BrowsingTopicsServiceImpl(BrowsingTopicsServiceImpl&&) = delete;
  BrowsingTopicsServiceImpl& operator=(BrowsingTopicsServiceImpl&&) = delete;

  ~BrowsingTopicsServiceImpl() override;

  std::vector<privacy_sandbox::CanonicalTopic> GetTopicsForSiteForDisplay(
      const url::Origin& top_origin) const override;

  std::vector<privacy_sandbox::CanonicalTopic> GetTopTopicsForDisplay()
      const override;

 private:
  friend class BrowsingTopicsServiceFactory;

  BrowsingTopicsServiceImpl();

  base::WeakPtrFactory<BrowsingTopicsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_IMPL_H_
