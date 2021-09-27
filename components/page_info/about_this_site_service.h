// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_H_
#define COMPONENTS_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"

class GURL;

namespace page_info {

// Provides "About this site" information for a web site. It includes short
// description about the website (from external source, usually from Wikipedia),
// when the website was first indexed and other data if available.
class AboutThisSiteService : public KeyedService {
 public:
  // Provides platform-independant access to an optimization guide service.
  // OptimizationGuideService on iOS doesn't implement OptimizationGuideDecider,
  // therefore the interface cannot be used in this service.
  class Client {
   public:
    virtual optimization_guide::OptimizationGuideDecision CanApplyOptimization(
        const GURL& url,
        optimization_guide::OptimizationMetadata* optimization_metadata) = 0;
    virtual ~Client() = default;
  };

  explicit AboutThisSiteService(std::unique_ptr<Client> client);
  ~AboutThisSiteService() override;

  AboutThisSiteService(const AboutThisSiteService&) = delete;
  AboutThisSiteService& operator=(const AboutThisSiteService&) = delete;

  // Returns "About this site" information for the website with |url|.
  std::u16string GetAboutThisSiteDescription(const GURL& url) const;

 private:
  std::unique_ptr<Client> client_;
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_H_
