// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_TEST_NACL_BROWSER_DELEGATE_H_
#define COMPONENTS_NACL_BROWSER_TEST_NACL_BROWSER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/nacl/browser/nacl_browser_delegate.h"

// This is a base test implementation of NaClBrowserDelegate which
// does nothing. Individual tests can override the methods further.
// To use the test delegate:
//
//   NaClBrowser::SetDelegate(new RefinedTestNaClBrowserDelegate);
//
// and
//
//   NaClBrowser::ClearAndDeleteDelegateForTest();
class TestNaClBrowserDelegate : public NaClBrowserDelegate {
 public:
  TestNaClBrowserDelegate();
  ~TestNaClBrowserDelegate() override;
  void ShowMissingArchInfobar(int render_process_id,
                              int render_view_id) override;
  bool DialogsAreSuppressed() override;
  bool GetCacheDirectory(base::FilePath* cache_dir) override;
  bool GetPluginDirectory(base::FilePath* plugin_dir) override;
  bool GetPnaclDirectory(base::FilePath* pnacl_dir) override;
  bool GetUserDirectory(base::FilePath* user_dir) override;
  std::string GetVersionString() const override;
  ppapi::host::HostFactory* CreatePpapiHostFactory(
      content::BrowserPpapiHost* ppapi_host) override;
  MapUrlToLocalFilePathCallback GetMapUrlToLocalFilePathCallback(
      const base::FilePath& profile_directory) override;
  void SetDebugPatterns(const std::string& debug_patterns) override;
  bool URLMatchesDebugPatterns(const GURL& manifest_url) override;
  bool IsNonSfiModeAllowed(const base::FilePath& profile_directory,
                           const GURL& manifest_url) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNaClBrowserDelegate);
};

#endif  // COMPONENTS_NACL_BROWSER_TEST_NACL_BROWSER_DELEGATE_H_
