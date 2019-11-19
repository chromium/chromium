// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::WebContents;
using content::WebContentsTester;

class FindBackendTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    FindTabHelper::CreateForWebContents(web_contents());
  }
};

namespace {

base::string16 FindPrepopulateText(WebContents* contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  return FindBarStateFactory::GetLastPrepopulateText(profile);
}

}  // end namespace

// This test takes two WebContents objects, searches in both of them and
// tests the internal state for find_text and find_prepopulate_text.
TEST_F(FindBackendTest, InternalState) {
  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents());
  // Initial state for the WebContents is blank strings.
  EXPECT_EQ(base::string16(), FindPrepopulateText(web_contents()));
  EXPECT_EQ(base::string16(), find_tab_helper->find_text());

  // Get another WebContents object ready.
  std::unique_ptr<WebContents> contents2(
      WebContentsTester::CreateTestWebContents(profile(), nullptr));
  FindTabHelper::CreateForWebContents(contents2.get());
  FindTabHelper* find_tab_helper2 =
      FindTabHelper::FromWebContents(contents2.get());

  // No search has still been issued, strings should be blank.
  EXPECT_EQ(base::string16(), FindPrepopulateText(web_contents()));
  EXPECT_EQ(base::string16(), find_tab_helper->find_text());
  EXPECT_EQ(base::string16(), FindPrepopulateText(contents2.get()));
  EXPECT_EQ(base::string16(), find_tab_helper2->find_text());

  base::string16 search_term1 = base::ASCIIToUTF16(" I had a 401K    ");
  base::string16 search_term2 = base::ASCIIToUTF16(" but the economy ");
  base::string16 search_term3 = base::ASCIIToUTF16(" eated it.       ");

  // Start searching in the first WebContents, searching forwards but not case
  // sensitive (as indicated by the last two params).
  find_tab_helper->StartFinding(search_term1, true, false);

  // Pre-populate string should always match between the two, but find_text
  // should not.
  EXPECT_EQ(search_term1, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term1, find_tab_helper->find_text());
  EXPECT_EQ(search_term1, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(base::string16(), find_tab_helper2->find_text());

  // Now search in the other WebContents, searching forwards but not case
  // sensitive (as indicated by the last two params).
  find_tab_helper2->StartFinding(search_term2, true, false);

  // Again, pre-populate string should always match between the two, but
  // find_text should not.
  EXPECT_EQ(search_term2, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term1, find_tab_helper->find_text());
  EXPECT_EQ(search_term2, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(search_term2, find_tab_helper2->find_text());

  // Search again in the first WebContents, searching forwards but not case
  // find_tab_helper (as indicated by the last two params).
  find_tab_helper->StartFinding(search_term3, true, false);

  // Once more, pre-populate string should always match between the two, but
  // find_text should not.
  EXPECT_EQ(search_term3, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term3, find_tab_helper->find_text());
  EXPECT_EQ(search_term3, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(search_term2, find_tab_helper2->find_text());
}
