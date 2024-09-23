// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_preview_feature.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media_preview_feature {

bool ShouldShowMediaPreview(content::BrowserContext& browser_context,
                            const GURL& requesting_origin_url,
                            const GURL& embedding_origin_url,
                            media_preview_metrics::UiLocation ui_location) {
  if (!base::FeatureList::IsEnabled(blink::features::kCameraMicPreview)) {
    return false;
  }

  // If we somehow get invalid or opaque origins, it's a corner case like a
  // data:, srcdoc:, or about: document that can't be part of the OT.
  if (!embedding_origin_url.is_valid() && !requesting_origin_url.is_valid()) {
    media_preview_metrics::RecordOriginTrialAllowed(ui_location, true);
    return true;
  }

  content::OriginTrialsControllerDelegate* origin_trials =
      browser_context.GetOriginTrialsControllerDelegate();

  // Incognito and guest profiles don't have an origin trials controller, so
  // they will just depend on the feature flag.
  if (!origin_trials) {
    media_preview_metrics::RecordOriginTrialAllowed(ui_location, true);
    return true;
  }

  // The URLs passed in originate a url::Origin, so this is safe.
  url::Origin requesting_origin = url::Origin::Create(requesting_origin_url);
  url::Origin embedding_origin = url::Origin::Create(embedding_origin_url);
  base::Time now = base::Time::Now();

  if (origin_trials->IsFeaturePersistedForOrigin(
          requesting_origin, requesting_origin,
          blink::mojom::OriginTrialFeature::kMediaPreviewsOptOut, now)) {
    media_preview_metrics::RecordOriginTrialAllowed(ui_location, false);
    return false;
  }

  if (embedding_origin != requesting_origin &&
      origin_trials->IsFeaturePersistedForOrigin(
          embedding_origin, embedding_origin,
          blink::mojom::OriginTrialFeature::kMediaPreviewsOptOut, now)) {
    media_preview_metrics::RecordOriginTrialAllowed(ui_location, false);
    return false;
  }

  media_preview_metrics::RecordOriginTrialAllowed(ui_location, true);
  return true;
}

}  // namespace media_preview_feature
