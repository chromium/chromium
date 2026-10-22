// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/features.h"

#include <string>

#include "base/strings/strcat.h"
#include "net/base/url_util.h"
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
BASE_FEATURE(kBrowserActuatorInternals, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableBrowserActuatorForGlicExperimentalTriggering,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kBrowserActuatorOAuth2ScopeParam{
    &kBrowserActuator, "BrowserActuatorOAuth2Scope",
    "https://www.googleapis.com/auth/chrome.autobrowse.actuator"};

GURL GetSendSessionMessageEndpoint() {
  GURL base_url(base::StrCat({kBrowserActuatorEndpointUrlParam.Get(),
                              kSendSessionMessagePathParam.Get()}));
  return net::AppendQueryParameter(base_url, "alt", "proto");
}

GURL GetWatchSessionsEndPoint() {
  GURL base_url(base::StrCat(
      {kBrowserActuatorEndpointUrlParam.Get(), kWatchSessionsPathParam.Get()}));
  return net::AppendQueryParameter(base_url, "alt", "proto");
}

}  // namespace browser_actuator
