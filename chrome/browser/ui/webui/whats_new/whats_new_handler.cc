// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

WhatsNewHandler::WhatsNewHandler() = default;

WhatsNewHandler::~WhatsNewHandler() = default;

void WhatsNewHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize", base::BindRepeating(&WhatsNewHandler::HandleInitialize,
                                        base::Unretained(this)));
}

void WhatsNewHandler::HandleInitialize(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();

  AllowJavascript();
  ResolveJavascriptCallback(
      base::Value(callback_id),
      whats_new::IsRemoteContentDisabled()
          ? base::Value()
          : base::Value(whats_new::GetServerURL(true).spec()));
  TryShowHatsSurveyWithTimeout();
}

void WhatsNewHandler::TryShowHatsSurveyWithTimeout() {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()),
                                        /* create_if_necessary = */ true);
  if (!hats_service)
    return;

  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerWhatsNew, web_ui()->GetWebContents(),
      features::kHappinessTrackingSurveysForDesktopWhatsNewTime.Get()
          .InMilliseconds(),
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/{},
      /*require_same_origin=*/true);
}
