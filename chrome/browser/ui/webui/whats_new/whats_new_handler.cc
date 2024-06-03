// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

WhatsNewHandler::WhatsNewHandler(
    mojo::PendingReceiver<whats_new::mojom::PageHandler> receiver,
    mojo::PendingRemote<whats_new::mojom::Page> page,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

WhatsNewHandler::~WhatsNewHandler() = default;

void WhatsNewHandler::GetServerUrl(GetServerUrlCallback callback) {
  GURL result = GURL("");
  if (!whats_new::IsRemoteContentDisabled()) {
    result = whats_new::GetServerURL(true);
  }
  std::move(callback).Run(result);

  TryShowHatsSurveyWithTimeout();
}

void WhatsNewHandler::TryShowHatsSurveyWithTimeout() {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_,
                                        /* create_if_necessary = */ true);
  if (!hats_service)
    return;

  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerWhatsNew, web_contents_,
      features::kHappinessTrackingSurveysForDesktopWhatsNewTime.Get()
          .InMilliseconds(),
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/{},
      /*navigation_behaviour=*/HatsService::REQUIRE_SAME_ORIGIN);
}
