// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"

#include <memory>
#include <utility>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

FramebustBlockMessageDelegate::FramebustBlockMessageDelegate(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    OutcomeCallback intervention_callback)
    : intervention_callback_(std::move(intervention_callback)),
      web_contents_(web_contents),
      blocked_url_(blocked_url) {}

FramebustBlockMessageDelegate::~FramebustBlockMessageDelegate() = default;

const GURL& FramebustBlockMessageDelegate::GetBlockedUrl() const {
  return blocked_url_;
}

void FramebustBlockMessageDelegate::AcceptIntervention() {
  if (!intervention_callback_.is_null())
    std::move(intervention_callback_).Run(InterventionOutcome::kAccepted);
}

void FramebustBlockMessageDelegate::DeclineIntervention() {
  if (!intervention_callback_.is_null()) {
    std::move(intervention_callback_)
        .Run(InterventionOutcome::kDeclinedAndNavigated);
  }
  web_contents_->OpenURL(content::OpenURLParams(
      blocked_url_, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void FramebustBlockMessageDelegate::DeclineInterventionWithReload() {
  DeclineIntervention();
}

void FramebustBlockMessageDelegate::DeclineInterventionSticky() {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  DCHECK(settings_map);
  settings_map->SetContentSettingDefaultScope(
      web_contents_->GetLastCommittedURL(), GURL(), ContentSettingsType::POPUPS,
      std::string(), CONTENT_SETTING_ALLOW);
  DeclineIntervention();
}
