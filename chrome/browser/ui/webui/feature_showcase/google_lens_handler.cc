// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/google_lens_handler.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"
#include "components/lens/lens_overlay_metrics.h"

GoogleLensHandler::GoogleLensHandler(
    mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandler>
        receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

GoogleLensHandler::~GoogleLensHandler() {
  if (!metrics_recorded_) {
    lens::RecordFirstRunPermissionNoticeUserAction(
        lens::LensPermissionUserAction::kEscKeyPressed);
  }
}

void GoogleLensHandler::EnableGoogleLens() {
  metrics_recorded_ = true;
  RecordStepUserAction(FeatureShowcaseStep::kGoogleLens,
                       FeatureShowcaseStepUserAction::kAccepted);
  lens::RecordFirstRunPermissionNoticeUserAction(
      lens::LensPermissionUserAction::kAcceptButtonPressed);

  lens::GrantLensOverlayNeededPermissions(profile_);
}

void GoogleLensHandler::SkipGoogleLens() {
  metrics_recorded_ = true;
  RecordStepUserAction(FeatureShowcaseStep::kGoogleLens,
                       FeatureShowcaseStepUserAction::kDeclined);
  lens::RecordFirstRunPermissionNoticeUserAction(
      lens::LensPermissionUserAction::kCancelButtonPressed);
}
