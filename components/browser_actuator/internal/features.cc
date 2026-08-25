// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/features.h"

#include <string>

#include "base/strings/strcat.h"
#include "components/browser_actuator/public/features.h"
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

BASE_FEATURE(kBrowserActuatorChannelEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBrowserActuatorProtoStreamTransport,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kProtoStreamBaseReconnectionTime,
                   &kBrowserActuatorProtoStreamTransport,
                   base::Seconds(3));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kProtoStreamMaxReconnectionTime,
                   &kBrowserActuatorProtoStreamTransport,
                   base::Minutes(5));

// TODO(crbug.com/534573786): Align the default with the production
// server's noop cadence once that is known.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kProtoStreamStallTimeout,
                   &kBrowserActuatorProtoStreamTransport,
                   base::Minutes(2));

BASE_FEATURE_PARAM(int,
                   kMaxTransportSessions,
                   &kBrowserActuatorProtoStreamTransport,
                   3);

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
