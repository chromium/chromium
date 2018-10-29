// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"

class Profile;
enum class WebappInstallSource;
struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace web_app {
class WebAppDataRetriever;
enum class InstallResultCode;
}

namespace extensions {

class BookmarkAppHelper;
class BookmarkAppInstaller;
class Extension;

// Class to install a BookmarkApp-based Shortcut or WebApp from a WebContents
// or WebApplicationInfo. Can only be called from the UI thread.
class BookmarkAppInstallationTask {
 public:
  struct Result {
    Result(web_app::InstallResultCode code, base::Optional<std::string> app_id);
    Result(Result&&);
    ~Result();

    const web_app::InstallResultCode code;
    const base::Optional<std::string> app_id;

    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  using ResultCallback = base::OnceCallback<void(Result)>;
  using BookmarkAppHelperFactory =
      base::RepeatingCallback<std::unique_ptr<BookmarkAppHelper>(
          Profile*,
          const WebApplicationInfo&,
          content::WebContents*,
          WebappInstallSource)>;

  // Ensures the tab helpers necessary for installing an app are present.
  static void CreateTabHelpers(content::WebContents* web_contents);

  // Constructs a task that will install a BookmarkApp-based Shortcut or Web App
  // for |profile|. |app_info| will be used to decide some of the
  // properties of the installed app e.g. open in a tab vs. window, installed by
  // policy, etc.
  explicit BookmarkAppInstallationTask(
      Profile* profile,
      web_app::PendingAppManager::AppInfo app_info);

  virtual ~BookmarkAppInstallationTask();

  virtual void InstallWebAppOrShortcutFromWebContents(
      content::WebContents* web_contents,
      ResultCallback callback);

  const web_app::PendingAppManager::AppInfo& app_info() { return app_info_; }

  void SetBookmarkAppHelperFactoryForTesting(
      BookmarkAppHelperFactory helper_factory);
  void SetDataRetrieverForTesting(
      std::unique_ptr<web_app::WebAppDataRetriever> data_retriever);
  void SetInstallerForTesting(std::unique_ptr<BookmarkAppInstaller> installer);

 protected:
  web_app::WebAppDataRetriever& data_retriever() { return *data_retriever_; }
  BookmarkAppInstaller& installer() { return *installer_; }

 private:
  void OnGetWebApplicationInfo(
      ResultCallback result_callback,
      content::WebContents* web_contents,
      std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnInstalled(ResultCallback result_callback,
                   const Extension* extension,
                   const WebApplicationInfo& web_app_info);

  Profile* profile_;

  const web_app::PendingAppManager::AppInfo app_info_;

  // We temporarily use a BookmarkAppHelper until the WebApp and WebShortcut
  // installation tasks reach feature parity with BookmarkAppHelper.
  std::unique_ptr<BookmarkAppHelper> helper_;
  BookmarkAppHelperFactory helper_factory_;

  std::unique_ptr<web_app::WebAppDataRetriever> data_retriever_;
  std::unique_ptr<BookmarkAppInstaller> installer_;

  base::WeakPtrFactory<BookmarkAppInstallationTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallationTask);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_
