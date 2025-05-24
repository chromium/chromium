// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/test_nacl_browser_delegate.h"
#include "base/functional/callback.h"

TestNaClBrowserDelegate::TestNaClBrowserDelegate() = default;

TestNaClBrowserDelegate::~TestNaClBrowserDelegate() = default;

void TestNaClBrowserDelegate::ShowMissingArchInfobar(int render_process_id,
                                                     int render_frame_id) {}

bool TestNaClBrowserDelegate::DialogsAreSuppressed() {
  return false;
}

bool TestNaClBrowserDelegate::GetCacheDirectory(base::FilePath* cache_dir) {
  return false;
}

bool TestNaClBrowserDelegate::GetPluginDirectory(base::FilePath* plugin_dir) {
  return false;
}

bool TestNaClBrowserDelegate::GetPnaclDirectory(base::FilePath* pnacl_dir) {
  return false;
}

bool TestNaClBrowserDelegate::GetUserDirectory(base::FilePath* user_dir) {
  return false;
}

std::string TestNaClBrowserDelegate::GetVersionString() const {
  return std::string();
}

ppapi::host::HostFactory* TestNaClBrowserDelegate::CreatePpapiHostFactory(
    content::BrowserPpapiHost* ppapi_host) {
  return NULL;
}

NaClBrowserDelegate::MapUrlToLocalFilePathCallback
TestNaClBrowserDelegate::GetMapUrlToLocalFilePathCallback(
    const base::FilePath& profile_directory) {
  return base::BindRepeating([](const GURL& url, bool use_blocking_api,
                                base::FilePath* file_path) { return false; });
}

void TestNaClBrowserDelegate::SetDebugPatterns(
    const std::string& debug_patterns) {}

bool TestNaClBrowserDelegate::URLMatchesDebugPatterns(
    const GURL& manifest_url) {
  return false;
}
