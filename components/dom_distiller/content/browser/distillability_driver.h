// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace dom_distiller {

// This is an IPC helper for determining whether a page should be distilled.
class DistillabilityDriver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DistillabilityDriver> {
 public:
  ~DistillabilityDriver() override;
  void CreateDistillabilityService(
      mojo::PendingReceiver<mojom::DistillabilityService> receiver);

  base::ObserverList<DistillabilityObserver>* GetObserverList() {
    return &observers_;
  }
  base::Optional<DistillabilityResult> GetLatestResult() const {
    return latest_result_;
  }

 private:
  explicit DistillabilityDriver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<DistillabilityDriver>;
  friend class DistillabilityServiceImpl;

  void OnDistillability(const DistillabilityResult& result);

  base::ObserverList<DistillabilityObserver> observers_;

  // The most recently received result from the distillability service.
  //
  // TODO(https://crbug.com/952042): Set this to nullopt when navigating to a
  // new page, accounting for same-document navigation.
  base::Optional<DistillabilityResult> latest_result_;

  base::WeakPtrFactory<DistillabilityDriver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(DistillabilityDriver);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_
