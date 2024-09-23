// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABILITY_DRIVER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABILITY_DRIVER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/content/browser/uma_helper.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace dom_distiller {

// This is an IPC helper for determining whether a page should be distilled.
class DistillabilityDriver
    : public content::WebContentsUserData<DistillabilityDriver>,
      public content::WebContentsObserver {
 public:
  ~DistillabilityDriver() override;
  void CreateDistillabilityService(
      mojo::PendingReceiver<mojom::DistillabilityService> receiver);

  base::ObserverList<DistillabilityObserver>* GetObserverList() {
    return &observers_;
  }
  std::optional<DistillabilityResult> GetLatestResult() const {
    return latest_result_;
  }

  // Sets a callback which can be used to determine the security of a page,
  // to decide whether it can be distilled. Only SECURE pages are currently
  // distillable on Desktop. Android does not check page security status
  // before distilling.
  void SetIsSecureCallback(
      base::RepeatingCallback<bool(content::WebContents*)> is_secure_check);

  UMAHelper::DistillabilityDriverTimer& GetTimer() { return timer_; }

  // content::WebContentsObserver overrides.
  void PrimaryPageChanged(content::Page& page) override;

  DistillabilityDriver(const DistillabilityDriver&) = delete;
  DistillabilityDriver& operator=(const DistillabilityDriver&) = delete;

 private:
  explicit DistillabilityDriver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<DistillabilityDriver>;
  friend class DistillabilityServiceImpl;

  void OnDistillability(const DistillabilityResult& result);

  base::ObserverList<DistillabilityObserver> observers_;

  // The most recently received result from the distillability service.
  //
  // TODO(crbug.com/40622813): Set this to nullopt when navigating to a
  // new page, accounting for same-document navigation.
  std::optional<DistillabilityResult> latest_result_;

  // For UMA metrics on durations spent in distilled or distillable pages.
  // Because each DistillabilityDriver is associated with just one WebContents,
  // it can be used to track the amount of time spent actively viewing that
  // WebContents when the page is distillable or distilled, creating useful
  // metrics for the ReaderMode experiment.
  UMAHelper::DistillabilityDriverTimer timer_;

  base::RepeatingCallback<bool(content::WebContents*)> is_secure_check_;

  base::WeakPtrFactory<DistillabilityDriver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABILITY_DRIVER_H_
