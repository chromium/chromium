// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_content_util.h"

#include "base/metrics/histogram_macros.h"
#include "components/previews/content/previews_user_data.h"

namespace previews {

bool HasEnabledPreviews(content::PreviewsState previews_state) {
  return previews_state != content::PREVIEWS_UNSPECIFIED &&
         !(previews_state & content::PREVIEWS_OFF) &&
         !(previews_state & content::PREVIEWS_NO_TRANSFORM);
}

content::PreviewsState DetermineAllowedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    bool is_reload,
    bool is_data_saver_user,
    previews::PreviewsDecider* previews_decider) {
  content::PreviewsState previews_state = content::PREVIEWS_UNSPECIFIED;

  // Record whether the hint cache has a matching entry for this pre-commit URL.
  previews_decider->LogHintCacheMatch(url, false /* is_committed */);

  if (!previews::params::ArePreviewsAllowed()) {
    return previews_state;
  }

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return previews_state;
  }

  if (previews_decider->ShouldAllowPreview(previews_data, url, is_reload,
                                           previews::PreviewsType::OFFLINE)) {
    previews_state |= content::OFFLINE_PAGE_ON;
  }

  // Only offline previews can be shown for non-data saver users.
  if (!is_data_saver_user)
    return previews_state;

  if (previews_decider->ShouldAllowPreview(
          previews_data, url, is_reload,
          previews::PreviewsType::RESOURCE_LOADING_HINTS)) {
    previews_state |= content::RESOURCE_LOADING_HINTS_ON;
    // Initiate load of any applicable hint details.
    previews_decider->LoadResourceHints(url);
  }

  // Check for client-side previews in precedence order.
  // Note: this is for the beginning of navigation so we should not
  // check for https here (since an http request may redirect to https).
  if (previews_decider->ShouldAllowPreview(previews_data, url, is_reload,
                                           previews::PreviewsType::NOSCRIPT)) {
    previews_state |= content::NOSCRIPT_ON;
  }

  if (previews::params::IsClientLoFiEnabled() &&
      previews_decider->ShouldAllowPreviewAtECT(
          previews_data, url, is_reload, previews::PreviewsType::LOFI,
          previews::params::EffectiveConnectionTypeThresholdForClientLoFi(),
          previews::params::GetBlackListedHostsForClientLoFiFieldTrial(),
          false)) {
    previews_state |= content::CLIENT_LOFI_ON;
  }

  return previews_state;
}

content::PreviewsState DetermineCommittedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    content::PreviewsState previews_state,
    const previews::PreviewsDecider* previews_decider) {
  bool is_https = url.SchemeIs(url::kHttpsScheme);

  // Record whether the hint cache has a matching entry for this committed URL.
  previews_decider->LogHintCacheMatch(url, true /* is_committed */);

  // Check if an offline preview was actually served.
  if (previews_data && previews_data->offline_preview_used()) {
    DCHECK(previews_state & content::OFFLINE_PAGE_ON);
    return content::OFFLINE_PAGE_ON;
  }
  previews_state &= ~content::OFFLINE_PAGE_ON;

  // If a server preview is set, retain only the bits determined for the server.
  // |previews_state| must already have been updated for server previews from
  // the main frame response headers (so if they are set here, then they are
  // the specify the committed preview). Note: for Server LoFi we keep the
  // Client LoFi bit on so that it is applied to both HTTP and HTTPS images.
  if (previews_state &
      (content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON)) {
    return previews_state & (content::SERVER_LITE_PAGE_ON |
                             content::SERVER_LOFI_ON | content::CLIENT_LOFI_ON);
  }

  if (previews_data && previews_data->cache_control_no_transform_directive()) {
    if (HasEnabledPreviews(previews_state)) {
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.CacheControlNoTransform.BlockedPreview",
          GetMainFramePreviewsType(previews_state),
          previews::PreviewsType::LAST);
    }
    return content::PREVIEWS_OFF;
  }

  // Make priority decision among allowed client preview types that can be
  // decided at Commit time.
  if (previews_state & content::RESOURCE_LOADING_HINTS_ON) {
    // Resource loading hints was chosen for the original URL but only continue
    // with it if the committed URL has HTTPS scheme and is allowed by decider.
    if (is_https && previews_decider &&
        previews_decider->IsURLAllowedForPreview(
            previews_data, url,
            previews::PreviewsType::RESOURCE_LOADING_HINTS)) {
      return content::RESOURCE_LOADING_HINTS_ON;
    }
    // Remove RESOURCE_LOADING_HINTS_ON from |previews_state| since we decided
    // not to commit to it.
    previews_state = previews_state & ~content::RESOURCE_LOADING_HINTS_ON;
  }

  if (previews_state & content::NOSCRIPT_ON) {
    // NoScript was chosen for the original URL but only continue with it
    // if the committed URL has HTTPS scheme and is allowed by decider.
    if (is_https && previews_decider &&
        previews_decider->IsURLAllowedForPreview(
            previews_data, url, previews::PreviewsType::NOSCRIPT)) {
      return content::NOSCRIPT_ON;
    }
    return content::PREVIEWS_OFF;
  }
  if (previews_state & content::CLIENT_LOFI_ON) {
    return content::CLIENT_LOFI_ON;
  }

  if (!previews_state) {
    return content::PREVIEWS_OFF;
  }

  DCHECK(previews_state == content::PREVIEWS_OFF ||
         previews_state == content::PREVIEWS_UNSPECIFIED);
  return content::PREVIEWS_OFF;
}

previews::PreviewsType GetMainFramePreviewsType(
    content::PreviewsState previews_state) {
  // The order is important here.
  if (previews_state & content::OFFLINE_PAGE_ON)
    return previews::PreviewsType::OFFLINE;
  if (previews_state & content::SERVER_LITE_PAGE_ON)
    return previews::PreviewsType::LITE_PAGE;
  if (previews_state & content::SERVER_LOFI_ON)
    return previews::PreviewsType::LOFI;
  if (previews_state & content::RESOURCE_LOADING_HINTS_ON)
    return previews::PreviewsType::RESOURCE_LOADING_HINTS;
  if (previews_state & content::NOSCRIPT_ON)
    return previews::PreviewsType::NOSCRIPT;
  if (previews_state & content::CLIENT_LOFI_ON)
    return previews::PreviewsType::LOFI;

  DCHECK_EQ(content::PREVIEWS_UNSPECIFIED,
            previews_state & ~content::CLIENT_LOFI_AUTO_RELOAD &
                ~content::PREVIEWS_NO_TRANSFORM & ~content::PREVIEWS_OFF);
  return previews::PreviewsType::NONE;
}

}  // namespace previews
