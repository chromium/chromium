// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_FEATURE_UPDATE_OBSERVER_H_
#define CHROMECAST_BROWSER_CAST_FEATURE_UPDATE_OBSERVER_H_

#include "base/values.h"
#include "chromecast/common/mojom/feature_update.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace chromecast {

namespace external_service_support {
class ExternalConnector;
}  // namespace external_service_support

class CastFeatureUpdateObserver
    : public chromecast::mojom::FeatureUpdateObserver {
 public:
  CastFeatureUpdateObserver(
      external_service_support::ExternalConnector* connector,
      PrefService* pref_service);
  CastFeatureUpdateObserver(const CastFeatureUpdateObserver&) = delete;
  CastFeatureUpdateObserver& operator=(const CastFeatureUpdateObserver&) =
      delete;
  ~CastFeatureUpdateObserver() override;

 private:
  // chromecast::mojom::FeatureUpdateObserver implementation:
  void OnFeaturesUpdated(base::Value::Dict features) override;

  void BindFeatureUpdateService();

  external_service_support::ExternalConnector* const connector_;
  PrefService* const pref_service_;

  mojo::Receiver<chromecast::mojom::FeatureUpdateObserver> receiver_{this};
  mojo::Remote<chromecast::mojom::FeatureUpdateService> feature_update_service_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_FEATURE_UPDATE_OBSERVER_H_
