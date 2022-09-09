// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/find_in_page/find_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::WebContents;
using content::WebContentsTester;

class FindBackendTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    FindBarState::ConfigureWebContents(web_contents());
  }
};

namespace {

std::u16string FindPrepopulateText(WebContents* contents) {
  return FindBarStateFactory::GetForBrowserContext(
             contents->GetBrowserContext())
      ->GetSearchPrepopulateText();
}

}  // end namespace

// This test takes two WebContents objects, searches in both of them and
// tests the internal state for find_text and find_prepopulate_text.
TEST_F(FindBackendTest, InternalState) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());
  // Initial state for the WebContents is blank strings.
  EXPECT_EQ(std::u16string(), FindPrepopulateText(web_contents()));
  EXPECT_EQ(std::u16string(), find_tab_helper->find_text());

  // Get another WebContents object ready.
  std::unique_ptr<WebContents> contents2(
      WebContentsTester::CreateTestWebContents(profile(), nullptr));
  FindBarState::ConfigureWebContents(contents2.get());
  find_in_page::FindTabHelper* find_tab_helper2 =
      find_in_page::FindTabHelper::FromWebContents(contents2.get());

  // No search has still been issued, strings should be blank.
  EXPECT_EQ(std::u16string(), FindPrepopulateText(web_contents()));
  EXPECT_EQ(std::u16string(), find_tab_helper->find_text());
  EXPECT_EQ(std::u16string(), FindPrepopulateText(contents2.get()));
  EXPECT_EQ(std::u16string(), find_tab_helper2->find_text());

  std::u16string search_term1 = u" I had a 401K    ";
  std::u16string search_term2 = u" but the economy ";
  std::u16string search_term3 = u" eated it.       ";

  // Start searching in the first WebContents.
  find_tab_helper->StartFinding(search_term1, true /* forward_direction */,
                                false /* case_sensitive */,
                                true /* find_match */);

  // Pre-populate string should always match between the two, but find_text
  // should not.
  EXPECT_EQ(search_term1, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term1, find_tab_helper->find_text());
  EXPECT_EQ(search_term1, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(std::u16string(), find_tab_helper2->find_text());

  // Now search in the other WebContents.
  find_tab_helper2->StartFinding(search_term2, true /* forward_direction */,
                                 false /* case_sensitive */,
                                 true /* find_match */);

  // Again, pre-populate string should always match between the two, but
  // find_text should not.
  EXPECT_EQ(search_term2, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term1, find_tab_helper->find_text());
  EXPECT_EQ(search_term2, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(search_term2, find_tab_helper2->find_text());

  // Search again in the first WebContents
  find_tab_helper->StartFinding(search_term3, true /* forward_direction */,
                                false /* case_sensitive */,
                                true /* find_match */);

  // The fallback search term for the first WebContents will be the original
  // search.
  EXPECT_EQ(search_term3, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term3, find_tab_helper->find_text());
  EXPECT_EQ(search_term3, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(search_term2, find_tab_helper2->find_text());
}
