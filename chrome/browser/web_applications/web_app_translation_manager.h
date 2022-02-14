// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TRANSLATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TRANSLATION_MANAGER_H_

#include <map>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/proto/web_app_translations.pb.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"

namespace web_app {

using Locale = std::u16string;

class WebAppTranslationManager : public WebAppInstallManagerObserver {
 public:
  using ReadCallback = base::OnceCallback<void(
      const std::map<AppId, blink::Manifest::TranslationItem>& cache)>;
  using WriteCallback = base::OnceCallback<void(bool success)>;

  WebAppTranslationManager(Profile* profile,
                           base::raw_ptr<WebAppInstallManager> install_manager,
                           scoped_refptr<FileUtilsWrapper> utils);
  WebAppTranslationManager(const WebAppTranslationManager&) = delete;
  WebAppTranslationManager& operator=(const WebAppTranslationManager&) = delete;
  ~WebAppTranslationManager() override;

  void Start();

  void WriteTranslations(
      const AppId& app_id,
      const base::flat_map<Locale, blink::Manifest::TranslationItem>&
          translations,
      WriteCallback callback);
  void DeleteTranslations(const AppId& app_id, WriteCallback callback);
  void ReadTranslations(ReadCallback callback);

  // TODO(crbug.com/1259777): Add methods to get the name, short_name and
  // description.

  // WebAppInstallManager:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

 private:
  void OnTranslationsRead(ReadCallback callback, const AllTranslations& proto);

  base::raw_ptr<WebAppInstallManager> install_manager_;
  base::FilePath web_apps_directory_;
  scoped_refptr<FileUtilsWrapper> utils_;
  std::map<AppId, blink::Manifest::TranslationItem> translation_cache_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::WeakPtrFactory<WebAppTranslationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TRANSLATION_MANAGER_H_
