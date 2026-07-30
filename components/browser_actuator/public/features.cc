// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/features.h"

#include "base/strings/strcat.h"
#include "url/gurl.h"

namespace browser_actuator {

namespace {

const base::FeatureParam<std::string> kBrowserActuatorEndpointUrlParam{
    &kBrowserActuator, "BrowserActuatorEndpointUrl",
    "https://confection.pa.googleapis.com/v1:"};

const base::FeatureParam<std::string> kSendSessionMessagePathParam{
    &kBrowserActuator, "BrowserActuatorSendSessionMessagePath",
    "sendSessionMessage"};

const base::FeatureParam<std::string> kWatchSessionsPathParam{
    &kBrowserActuator, "BrowserActuatorWatchSessionsPath", "watchSessions"};

}  // namespace

BASE_FEATURE(kBrowserActuator, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kBrowserActuatorOAuth2ScopeParam{
    &kBrowserActuator, "BrowserActuatorOAuth2Scope",
    "https://www.googleapis.com/auth/chrome.autobrowse.actuator"};

GURL GetSendSessionMessageEndpoint() {
  return GURL(base::StrCat({kBrowserActuatorEndpointUrlParam.Get(),
                            kSendSessionMessagePathParam.Get()}));
}

GURL GetWatchSessionsEndPoint() {
  return GURL(base::StrCat(
      {kBrowserActuatorEndpointUrlParam.Get(), kWatchSessionsPathParam.Get()}));
}

}  // namespace browser_actuator
