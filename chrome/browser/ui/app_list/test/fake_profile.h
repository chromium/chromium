// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/domain_reliability/clear_mode.h"
#include "content/public/browser/browser_context.h"

class ResourceContext;

namespace net {
class URLRequestContextGetter;
}

namespace content {
class DownloadManagerDelegate;
class ResourceContext;
class SSLHostStateDelegate;
class ZoomLevelDelegate;
}

class FakeProfile : public Profile {
 public:
  explicit FakeProfile(const std::string& name);
  FakeProfile(const std::string& name, const base::FilePath& path);

  // Profile overrides.
  std::string GetProfileUserName() const override;
  ProfileType GetProfileType() const override;
  base::FilePath GetPath() const override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  Profile* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override;
  bool HasOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  const Profile* GetOriginalProfile() const override;
  bool IsSupervised() const override;
  bool IsChild() const override;
  bool IsLegacySupervised() const override;
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  PrefService* GetOffTheRecordPrefs() override;
  net::URLRequestContextGetter* GetRequestContext() override;
  base::OnceCallback<net::CookieStore*()> GetExtensionsCookieStoreGetter()
      override;
  bool IsSameProfile(Profile* profile) override;
  base::Time GetStartTime() const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  void ChangeAppLocale(const std::string& locale,
                       AppLocaleChangedVia via) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;

  GURL GetHomePage() override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() override;

 private:
  std::string name_;
  base::FilePath path_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_
