// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_SERIALIZATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_SERIALIZATION_H_

#include <memory>
#include <string>

#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebApp;
namespace proto {
class WebApp;
}  // namespace proto

std::unique_ptr<WebApp> ParseWebAppProtoForTesting(const webapps::AppId& app_id,
                                                   const std::string& value);
std::unique_ptr<WebApp> ParseWebAppProto(const proto::WebApp& proto);
std::unique_ptr<proto::WebApp> WebAppToProto(const WebApp& web_app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_SERIALIZATION_H_
