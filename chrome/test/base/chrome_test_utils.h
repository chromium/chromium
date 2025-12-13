// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_UTILS_H_
#define CHROME_TEST_BASE_CHROME_TEST_UTILS_H_

#include "build/build_config.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

// This namespace contains test utilities that function for both Android and
// desktop browser tests.
namespace chrome_test_utils {

// Returns the active WebContents. On desktop this is in the first browser
// window created by tests, more specific behaviour requires other means.
// Takes a const PlatformBrowserTest so it can be called from other const
// methods:
// void MyConstMemberFunction() const {
//   auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
//   ...
content::WebContents* GetActiveWebContents(
    const PlatformBrowserTest* browser_test);

// Returns the active Tab. On desktop this is in the first browser
// window created by tests, more specific behaviour requires other means.
// Takes a const PlatformBrowserTest so it can be called from other const
// methods:
// void MyConstMemberFunction() const {
//   auto* tab = chrome_test_utils::GetActiveTab(this);
//   ...
tabs::TabInterface* GetActiveTab(const PlatformBrowserTest* browser_test);

// Returns the WebContents at the specific index. On Android, this is the
// specific content from active model.
content::WebContents* GetWebContentsAt(const PlatformBrowserTest* browser_test,
                                       int index);

// Returns the active Profile. On desktop this is in the first browser
// window created by tests, more specific behaviour requires other means.
Profile* GetProfile(const PlatformBrowserTest* browser_test);

// Navigates `web_contents` to a `url` in and waits until the load stops.
// If the URL redirects it waits until the last destination is reached.
// It returns true if the last navigation was successful and false otherwise.
//
// Unlike content::NavigateToURL, the caller of this function doesn't have
// to specify the expected commit URL for URLs causing redirects.
[[nodiscard]] bool NavigateToURL(content::WebContents* web_contents,
                                 const GURL& url);

// Returns the test data path used by the embedded test server.
base::FilePath GetChromeTestDataDir();

// Overrides the path chrome::DIR_TEST_DATA. Used early in test startup so the
// value is available in constructors and SetUp methods.
void OverrideChromeTestDataDir();

// Generate the file path for testing a particular test.
// The file for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is base::FilePath format.
base::FilePath GetTestFilePath(const base::FilePath& dir,
                               const base::FilePath& file);

// Generate the URL for testing a particular test.
// HTML for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is GURL format.
GURL GetTestUrl(const base::FilePath& dir, const base::FilePath& file);

}  // namespace chrome_test_utils

#endif  // CHROME_TEST_BASE_CHROME_TEST_UTILS_H_
