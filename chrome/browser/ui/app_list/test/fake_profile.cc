// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/app_list/test/fake_profile.h"

FakeProfile::FakeProfile(const std::string& name)
    : name_(name) {
  BrowserContext::Initialize(this, base::FilePath());
}

FakeProfile::FakeProfile(const std::string& name, const base::FilePath& path)
    : name_(name),
      path_(path) {
  BrowserContext::Initialize(this, path_);
}

std::string FakeProfile::GetProfileUserName() const {
  return name_;
}

Profile::ProfileType FakeProfile::GetProfileType() const {
  return REGULAR_PROFILE;
}

base::FilePath FakeProfile::GetPath() const {
  return path_;
}

std::unique_ptr<content::ZoomLevelDelegate>
FakeProfile::CreateZoomLevelDelegate(const base::FilePath& partition_path) {
  return nullptr;
}

bool FakeProfile::IsOffTheRecord() const {
  return false;
}

content::DownloadManagerDelegate* FakeProfile::GetDownloadManagerDelegate() {
  return nullptr;
}

content::ResourceContext* FakeProfile::GetResourceContext() {
  return nullptr;
}

content::BrowserPluginGuestManager* FakeProfile::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* FakeProfile::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService* FakeProfile::GetPushMessagingService() {
  return nullptr;
}

content::SSLHostStateDelegate* FakeProfile::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
FakeProfile::GetPermissionControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate* FakeProfile::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController* FakeProfile::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
FakeProfile::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* FakeProfile::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return nullptr;
}

net::URLRequestContextGetter*
FakeProfile::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return nullptr;
}

net::URLRequestContextGetter* FakeProfile::CreateMediaRequestContext() {
  return nullptr;
}

net::URLRequestContextGetter*
FakeProfile::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return nullptr;
}

scoped_refptr<base::SequencedTaskRunner>
FakeProfile::GetIOTaskRunner() {
  return scoped_refptr<base::SequencedTaskRunner>();
}

Profile* FakeProfile::GetOffTheRecordProfile() {
  return nullptr;
}

void FakeProfile::DestroyOffTheRecordProfile() {}

bool FakeProfile::HasOffTheRecordProfile() {
  return false;
}

Profile* FakeProfile::GetOriginalProfile() {
  return this;
}

const Profile* FakeProfile::GetOriginalProfile() const {
  return this;
}

bool FakeProfile::IsSupervised() const {
  return false;
}

bool FakeProfile::IsChild() const {
  return false;
}

bool FakeProfile::IsLegacySupervised() const {
  return false;
}

ExtensionSpecialStoragePolicy* FakeProfile::GetExtensionSpecialStoragePolicy() {
  return nullptr;
}

PrefService* FakeProfile::GetPrefs() {
  return nullptr;
}

const PrefService* FakeProfile::GetPrefs() const {
  return nullptr;
}

PrefService* FakeProfile::GetOffTheRecordPrefs() {
  return nullptr;
}

net::URLRequestContextGetter* FakeProfile::GetRequestContext() {
  return nullptr;
}

base::OnceCallback<net::CookieStore*()>
FakeProfile::GetExtensionsCookieStoreGetter() {
  return base::BindOnce([]() -> net::CookieStore* { return nullptr; });
}

bool FakeProfile::IsSameProfile(Profile* profile) {
  return false;
}

base::Time FakeProfile::GetStartTime() const {
  return base::Time();
}

base::FilePath FakeProfile::last_selected_directory() {
  return base::FilePath();
}

void FakeProfile::set_last_selected_directory(const base::FilePath& path) {}

void FakeProfile::ChangeAppLocale(
    const std::string& locale, AppLocaleChangedVia via) {}
void FakeProfile::OnLogin() {}
void FakeProfile::InitChromeOSPreferences() {}

GURL FakeProfile::GetHomePage() {
  return GURL();
}

bool FakeProfile::WasCreatedByVersionOrLater(const std::string& version) {
  return false;
}

void FakeProfile::SetExitType(ExitType exit_type) {
}

Profile::ExitType FakeProfile::GetLastSessionExitType() {
  return EXIT_NORMAL;
}
