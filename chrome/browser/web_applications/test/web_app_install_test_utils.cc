// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

AppId InstallDummyWebApp(Profile* profile,
                         const std::string& app_name,
                         const GURL& start_url) {
  DCHECK(base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions));
  const AppId app_id = GenerateAppIdFromURL(start_url);
  WebApplicationInfo web_app_info;

  web_app_info.start_url = start_url;
  web_app_info.scope = start_url;
  web_app_info.title = base::UTF8ToUTF16(app_name);
  web_app_info.description = base::UTF8ToUTF16(app_name);
  web_app_info.open_as_window = true;

  InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::EXTERNAL_DEFAULT;

  // In unit tests, we do not have Browser or WebContents instances.
  // Hence we use FinalizeInstall instead of InstallWebAppFromManifest
  // to install the web app.
  base::RunLoop run_loop;
  WebAppProviderBase::GetProviderBase(profile)
      ->install_finalizer()
      .FinalizeInstall(
          web_app_info, options,
          base::BindLambdaForTesting(
              [&](const AppId& installed_app_id, InstallResultCode code) {
                EXPECT_EQ(installed_app_id, app_id);
                EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
                run_loop.Quit();
              }));
  run_loop.Run();

  return app_id;
}

}  // namespace web_app
