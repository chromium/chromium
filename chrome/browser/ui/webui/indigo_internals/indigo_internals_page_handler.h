// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_PAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/ui/webui/indigo_internals/indigo_internals.mojom.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

class IndigoInternalsPageHandler
    : public indigo_internals::mojom::PageHandler,
      public unified_consent::UrlKeyedDataCollectionConsentHelper::Observer {
 public:
  IndigoInternalsPageHandler(
      mojo::PendingReceiver<indigo_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<indigo_internals::mojom::Page> page,
      Profile* profile);

  IndigoInternalsPageHandler(const IndigoInternalsPageHandler&) = delete;
  IndigoInternalsPageHandler& operator=(const IndigoInternalsPageHandler&) =
      delete;

  ~IndigoInternalsPageHandler() override;

  // indigo_internals::mojom::PageHandler:
  void GetLocalEligibility(GetLocalEligibilityCallback callback) override;
  void GetCombinedEligibility(GetCombinedEligibilityCallback callback) override;
  void InvalidateRemoteEligibility() override;
  void GetOptimizationGuideStatus(
      GetOptimizationGuideStatusCallback callback) override;

 private:
  void OnLocalEligibilityChanged(indigo::LocalEligibility status);

  // unified_consent::UrlKeyedDataCollectionConsentHelper::Observer:
  void OnUrlKeyedDataCollectionConsentStateChanged(
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper)
      override;

  mojo::Receiver<indigo_internals::mojom::PageHandler> receiver_;
  mojo::Remote<indigo_internals::mojom::Page> page_;
  raw_ptr<Profile> profile_;
  base::CallbackListSubscription subscription_;

  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      consent_helper_;
  base::ScopedObservation<
      unified_consent::UrlKeyedDataCollectionConsentHelper,
      unified_consent::UrlKeyedDataCollectionConsentHelper::Observer>
      consent_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_PAGE_HANDLER_H_
