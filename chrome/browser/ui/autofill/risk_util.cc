// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/risk_util.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/content/browser/risk/fingerprint.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/base/base_window.h"
#endif

namespace autofill {

namespace risk_util {

namespace {

void PassRiskData(base::OnceCallback<void(const std::string&)> callback,
                  std::unique_ptr<risk::Fingerprint> fingerprint) {
  std::string proto_data;
  fingerprint->SerializeToString(&proto_data);
  std::move(callback).Run(base::Base64Encode(proto_data));
}

#if !BUILDFLAG(IS_ANDROID)
// Returns the containing window for the given |web_contents|. The containing
// window might be a browser window for a Chrome tab, or it might be an app
// window for a platform app.
ui::BaseWindow* GetBaseWindowForWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser)
    return browser->window();

  gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();
  extensions::AppWindow* app_window =
      AppWindowRegistryUtil::GetAppWindowForNativeWindowAnyProfile(
          native_window);
  return app_window ? app_window->GetBaseWindow() : nullptr;
}
#endif

}  // namespace

void LoadRiskData(uint64_t obfuscated_gaia_id,
                  content::WebContents* web_contents,
                  base::OnceCallback<void(const std::string&)> callback) {
  // No easy way to get window bounds on Android, and that signal isn't very
  // useful anyway (given that we're also including the bounds of the web
  // contents).
  gfx::Rect window_bounds;
#if !BUILDFLAG(IS_ANDROID)
  if (ui::BaseWindow* base_window = GetBaseWindowForWebContents(web_contents)) {
    window_bounds = base_window->GetBounds();
  }
#endif

  PrefService* user_prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();

  LoadRiskDataHelper(obfuscated_gaia_id, user_prefs, std::move(callback),
                     web_contents, window_bounds);
}

void LoadRiskDataHelper(uint64_t obfuscated_gaia_id,
                        PrefService* user_prefs,
                        base::OnceCallback<void(const std::string&)> callback,
                        content::WebContents* web_contents,
                        gfx::Rect window_bounds) {
  std::string charset = user_prefs->GetString(::prefs::kDefaultCharset);
  std::string accept_languages =
      user_prefs->GetString(::language::prefs::kAcceptLanguages);
  base::Time install_time = base::Time::FromTimeT(
      g_browser_process->local_state()->GetInt64(metrics::prefs::kInstallDate));

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  risk::GetFingerprint(obfuscated_gaia_id, window_bounds, web_contents,
                       std::string(version_info::GetVersionNumber()), charset,
                       accept_languages, install_time,
                       g_browser_process->GetApplicationLocale(),
                       embedder_support::GetUserAgent(),
                       base::BindOnce(PassRiskData, std::move(callback)));
}

}  // namespace risk_util

}  // namespace autofill
