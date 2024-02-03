// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_FACTORY_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace payments::facilitated {

// Manages the lifetime of `ContentFacilitatedPaymentsDriver`. It is owned by
// `WebContents`. Creates one `ContentFacilitatedPaymentsDriver` for each
// outermost main `RenderFrameHost`.
class ContentFacilitatedPaymentsDriverFactory
    : public content::WebContentsUserData<
          ContentFacilitatedPaymentsDriverFactory>,
      public content::WebContentsObserver {
 public:
  ContentFacilitatedPaymentsDriverFactory(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  ContentFacilitatedPaymentsDriverFactory(
      const ContentFacilitatedPaymentsDriverFactory&) = delete;
  ContentFacilitatedPaymentsDriverFactory& operator=(
      const ContentFacilitatedPaymentsDriverFactory&) = delete;
  ~ContentFacilitatedPaymentsDriverFactory() override;

 private:
  friend class content::WebContentsUserData<
      ContentFacilitatedPaymentsDriverFactory>;

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // Gets or creates a dedicated `ContentFacilitatedPaymentsDriver` for the
  // `render_frame_host`. Drivers are only created for the outermost main frame.
  ContentFacilitatedPaymentsDriver& GetOrCreateForFrame(
      content::RenderFrameHost* render_frame_host);

  // Owns the drivers, one for each render frame host. Should be empty at
  // destruction time because its elements are erased in RenderFrameDeleted().
  base::flat_map<content::RenderFrameHost*,
                 std::unique_ptr<ContentFacilitatedPaymentsDriver>>
      driver_map_;

  // The optimization guide decider to help determine whether the current main
  // frame URL is eligible for facilitated payments.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_FACTORY_H_
