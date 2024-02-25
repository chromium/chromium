// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_SERVICE_H_
#define COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_SERVICE_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;
class TemplateURLService;

namespace page_info {
namespace proto {
class SiteInfo;
}

static const char AboutThisSiteRenderModeParameterName[] = "ilrm";
static const char AboutThisSiteRenderModeParameterValue[] = "minimal";

// Provides "About this site" information for a web site. It includes short
// description about the website (from external source, usually from Wikipedia),
// when the website was first indexed and other data if available.
class AboutThisSiteService : public KeyedService {
 public:
  using DecisionAndMetadata =
      std::pair<optimization_guide::OptimizationGuideDecision,
                std::optional<page_info::proto::AboutThisSiteMetadata>>;

  class TabHelper {
   public:
    virtual DecisionAndMetadata GetAboutThisSiteMetadata() const = 0;
    virtual ~TabHelper() = default;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Keep in sync with AboutThisSiteInteraction in enums.xml
  enum class AboutThisSiteInteraction {
    kNotShown = 0,
    kShownWithDescription = 1,
    kShownWithoutDescription = 2,
    kClickedWithDescription = 3,
    kClickedWithoutDescription = 4,
    kOpenedDirectlyFromSidePanel = 5,
    kNotShownNonGoogleDSE = 6,
    kNotShownLocalHost = 7,
    kNotShownOptimizationGuideNotAllowed = 8,
    // kShownWithoutMsbb = 9 deprecated
    kSameTabNavigation = 10,

    kMaxValue = kSameTabNavigation
  };

  explicit AboutThisSiteService(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      bool is_off_the_record,
      PrefService* prefs,
      TemplateURLService* template_url_service);
  ~AboutThisSiteService() override;

  AboutThisSiteService(const AboutThisSiteService&) = delete;
  AboutThisSiteService& operator=(const AboutThisSiteService&) = delete;

  // Returns "About this site" information for the website with |url|.
  std::optional<proto::SiteInfo> GetAboutThisSiteInfo(
      const GURL& url,
      ukm::SourceId source_id,
      const TabHelper* tab_helper) const;

  static GURL CreateMoreAboutUrlForNavigation(const GURL& url);
  static void OnAboutThisSiteRowClicked(bool with_description);
  static void OnOpenedDirectlyFromSidePanel();
  static void OnSameTabNavigation();

  base::WeakPtr<AboutThisSiteService> GetWeakPtr();

 private:
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
  const bool is_off_the_record_;
  const raw_ptr<PrefService> prefs_;
  raw_ptr<TemplateURLService, DanglingUntriaged> template_url_service_;

  base::WeakPtrFactory<AboutThisSiteService> weak_ptr_factory_{this};

  // Virtual for tests.
  virtual bool IsOptimizationGuideAllowed() const;
  virtual optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::OptimizationMetadata* optimization_metadata) const;
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_SERVICE_H_
