// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_USER_EDUCATION_USER_EDUCATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_USER_EDUCATION_USER_EDUCATION_HANDLER_H_

#include "base/feature.h"
#include "base/memory/raw_ptr.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/new_badge/new_badge_controller.h"
#include "components/user_education/webui/user_education.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class BrowserUserEducationInterface;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// Provides a subset of the functionality of `BrowserUserEducationInterface`
// for WebUI that is appropriate for use in both trusted and untrusted WebUI.
//
// This base class allows unit testing without being directly hooked up to a
// WebUI.
class UserEducationMixedTrustHandlerBase
    : public user_education::mojom::UserEducationMixedTrustHandler {
 public:
  UserEducationMixedTrustHandlerBase();
  UserEducationMixedTrustHandlerBase(
      const UserEducationMixedTrustHandlerBase&) = delete;
  void operator=(const UserEducationMixedTrustHandlerBase&) = delete;
  ~UserEducationMixedTrustHandlerBase() override;

  // UserEducationMixedTrustHandler:
  void MaybeShowFeaturePromo(
      user_education::mojom::FeaturePromoParamsPtr params) override;
  void NotifyFeaturePromoFeatureUsed(
      const std::string& feature_name,
      user_education::mojom::FeaturePromoFeatureUsedAction action) override;
  void NotifyAdditionalConditionEvent(const std::string& event_name) override;
  void NotifyNewBadgeFeatureUsed(const std::string& feature_name) override;
  void MaybeShowNewBadgeFor(const std::string& feature_name,
                            MaybeShowNewBadgeForCallback callback) override;

 protected:
  // Override to use mojo error handling; defaults to NOTREACHED().
  virtual void ReportBadMessage(std::string_view error);

  virtual const user_education::NewBadgeRegistry* GetNewBadgeRegistry()
      const = 0;
  virtual user_education::NewBadgeController* GetNewBadgeController() = 0;
  virtual const user_education::FeaturePromoRegistry* GetFeaturePromoRegistry()
      const = 0;
  virtual user_education::FeaturePromoController*
  GetFeaturePromoController() = 0;
  virtual feature_engagement::Tracker* GetFeatureEngagementTracker() = 0;

  // This is potentially more expensive than the other operations.
  virtual BrowserUserEducationInterface* GetBrowserUserEducationInterface() = 0;

 private:
  const base::Feature* FeaturePromoFeatureFromName(
      const std::string& feature_name) const;
  const base::Feature* NewBadgeFeatureFromName(
      const std::string& feature_name) const;
};

// The actual implementation to be used by WebUI.
class UserEducationMixedTrustHandler
    : public UserEducationMixedTrustHandlerBase {
 public:
  UserEducationMixedTrustHandler(
      mojo::PendingReceiver<
          user_education::mojom::UserEducationMixedTrustHandler>
          pending_handler,
      content::WebContents* contents);
  ~UserEducationMixedTrustHandler() override;

 protected:
  void ReportBadMessage(std::string_view error) override;
  const user_education::NewBadgeRegistry* GetNewBadgeRegistry() const override;
  user_education::NewBadgeController* GetNewBadgeController() override;
  const user_education::FeaturePromoRegistry* GetFeaturePromoRegistry()
      const override;
  user_education::FeaturePromoController* GetFeaturePromoController() override;
  feature_engagement::Tracker* GetFeatureEngagementTracker() override;
  BrowserUserEducationInterface* GetBrowserUserEducationInterface() override;

 private:
  content::BrowserContext* GetBrowserContext() const;

  mojo::Receiver<user_education::mojom::UserEducationMixedTrustHandler>
      receiver_;
  const raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_USER_EDUCATION_USER_EDUCATION_HANDLER_H_
