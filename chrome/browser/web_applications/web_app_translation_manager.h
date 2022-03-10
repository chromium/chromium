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
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

using Locale = std::u16string;

class WebAppTranslationManager {
 public:
  using ReadCallback = base::OnceCallback<void(
      const std::map<AppId, blink::Manifest::TranslationItem>& cache)>;
  using WriteCallback = base::OnceCallback<void(bool success)>;

  WebAppTranslationManager(Profile* profile,
                           scoped_refptr<FileUtilsWrapper> utils);
  WebAppTranslationManager(const WebAppTranslationManager&) = delete;
  WebAppTranslationManager& operator=(const WebAppTranslationManager&) = delete;
  ~WebAppTranslationManager();

  void SetSubsystems(base::raw_ptr<WebAppRegistrar> registrar);

  void Start();

  void WriteTranslations(
      const AppId& app_id,
      const base::flat_map<Locale, blink::Manifest::TranslationItem>&
          translations,
      WriteCallback callback);
  void DeleteTranslations(const AppId& app_id, WriteCallback callback);
  void ReadTranslations(ReadCallback callback);

  std::string GetName(const AppId& app_id);
  std::string GetDescription(const AppId& app_id);
  // TODO(crbug.com/1212519): Add a method to get the short_name.

 private:
  void OnTranslationsRead(ReadCallback callback, const AllTranslations& proto);

  base::raw_ptr<WebAppRegistrar> registrar_;
  base::FilePath web_apps_directory_;
  scoped_refptr<FileUtilsWrapper> utils_;
  // Cache of the translations on disk for the current device language.
  std::map<AppId, blink::Manifest::TranslationItem> translation_cache_;

  base::WeakPtrFactory<WebAppTranslationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TRANSLATION_MANAGER_H_
