// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "url/gurl.h"

CoreAppLauncherHandler::CoreAppLauncherHandler() {}

CoreAppLauncherHandler::~CoreAppLauncherHandler() {}

// static
void CoreAppLauncherHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kNtpAppPageNames,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void CoreAppLauncherHandler::HandleRecordAppLaunchByUrl(
    const base::ListValue* args) {
  std::string url;
  CHECK(args->GetString(0, &url));
  double source;
  CHECK(args->GetDouble(1, &source));

  extension_misc::AppLaunchBucket bucket =
      static_cast<extension_misc::AppLaunchBucket>(static_cast<int>(source));
  CHECK(source < extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

  RecordAppLaunchByUrl(Profile::FromWebUI(web_ui()), url, bucket);
}

void CoreAppLauncherHandler::RecordAppLaunchByUrl(
    Profile* profile,
    std::string url,
    extension_misc::AppLaunchBucket bucket) {
  CHECK(bucket != extension_misc::APP_LAUNCH_BUCKET_INVALID);

  if (!extensions::ExtensionRegistry::Get(profile)
           ->enabled_extensions()
           .GetAppByURL(GURL(url))) {
    return;
  }

  extensions::RecordAppLaunchType(bucket,
                                  extensions::Manifest::TYPE_HOSTED_APP);
}

void CoreAppLauncherHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "recordAppLaunchByURL",
      base::BindRepeating(&CoreAppLauncherHandler::HandleRecordAppLaunchByUrl,
                          base::Unretained(this)));
}
