// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_feature_update_observer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/common/mojom/constants.mojom.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "components/prefs/pref_service.h"

namespace chromecast {

CastFeatureUpdateObserver::CastFeatureUpdateObserver(
    external_service_support::ExternalConnector* connector,
    PrefService* pref_service)
    : connector_(connector), pref_service_(pref_service) {
  DCHECK(connector_);
  DCHECK(pref_service_);

  BindFeatureUpdateService();
}

CastFeatureUpdateObserver::~CastFeatureUpdateObserver() = default;

void CastFeatureUpdateObserver::BindFeatureUpdateService() {
  feature_update_service_.reset();
  receiver_.reset();
  connector_->BindInterface(
      mojom::kChromecastServiceName,
      feature_update_service_.BindNewPipeAndPassReceiver());
  feature_update_service_->RegisterFeatureUpdateObserver(
      receiver_.BindNewPipeAndPassRemote());

  // Right now we are in the process of making the `cast_service` manage the
  // lifecycle of `cast_browser`. Until that is done, `cast_service` has a
  // shorter lifecycle than `cast_browser`, so we need to handle disconnects
  // here.
  // TODO(crbug.com/40210465): remove once process lifecycles are inverted.
  receiver_.set_disconnect_handler(
      base::BindOnce(&CastFeatureUpdateObserver::BindFeatureUpdateService,
                     base::Unretained(this)));
}

void CastFeatureUpdateObserver::OnFeaturesUpdated(base::Value::Dict features) {
  pref_service_->SetDict(prefs::kLatestDCSFeatures, std::move(features));
  pref_service_->CommitPendingWrite();
}

}  // namespace chromecast
