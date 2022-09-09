// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_notes/user_note_service_delegate_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace user_notes {

namespace {

const char kBaseUrl[] = "http://www.google.com/";

}  // namespace

class MockUserNotesUI : public UserNotesUI {
 public:
  MOCK_METHOD(void, InvalidateIfVisible, (), (override));
  MOCK_METHOD(void,
              FocusNote,
              (const base::UnguessableToken& guid),
              (override));
  MOCK_METHOD(void,
              StartNoteCreation,
              (UserNoteInstance * instance),
              (override));
  MOCK_METHOD(void, Show, (), (override));
};

class UserNoteServiceDelegateImplTest : public BrowserWithTestWindowTest {};

TEST_F(UserNoteServiceDelegateImplTest, GetUICoordinatorForFrame) {
  // Prepare two browsers.
  Browser* browser1 = browser();
  std::unique_ptr<BrowserWindow> window2 = CreateBrowserWindow();
  std::unique_ptr<Browser> browser2 =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, false, window2.get());

  GURL url = GURL(kBaseUrl);
  AddTab(browser1, url);
  AddTab(browser2.get(), url);

  // Attach a mock UserNotesUI implementation to the browsers.
  auto mock_ui1 = std::make_unique<MockUserNotesUI>();
  MockUserNotesUI* mock1 = mock_ui1.get();
  EXPECT_CALL(*mock_ui1, InvalidateIfVisible).Times(0);
  EXPECT_CALL(*mock_ui1, FocusNote).Times(0);
  EXPECT_CALL(*mock_ui1, StartNoteCreation).Times(0);
  EXPECT_CALL(*mock_ui1, Show).Times(0);

  auto mock_ui2 = std::make_unique<MockUserNotesUI>();
  MockUserNotesUI* mock2 = mock_ui2.get();
  EXPECT_CALL(*mock_ui2, InvalidateIfVisible).Times(0);
  EXPECT_CALL(*mock_ui2, FocusNote).Times(0);
  EXPECT_CALL(*mock_ui2, StartNoteCreation).Times(0);
  EXPECT_CALL(*mock_ui2, Show).Times(0);

  browser1->SetUserData(UserNotesUI::UserDataKey(), std::move(mock_ui1));
  browser2->SetUserData(UserNotesUI::UserDataKey(), std::move(mock_ui2));

  // Ensure the attached UI implementations can be retrieved by the delegate.
  auto delegate = std::make_unique<UserNoteServiceDelegateImpl>(profile());
  UserNotesUI* impl1 = delegate->GetUICoordinatorForFrame(
      browser1->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame());
  UserNotesUI* impl2 = delegate->GetUICoordinatorForFrame(
      browser2->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame());

  EXPECT_EQ(mock1, impl1);
  EXPECT_EQ(mock2, impl2);

  // Cleanup.
  browser2->tab_strip_model()->CloseAllTabs();
  browser2.reset();
  window2.reset();
}

TEST_F(UserNoteServiceDelegateImplTest, GetAllFramesForUserNotes) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(browser_list->size(), 1u);

  Browser* browser1 = browser();
  std::unique_ptr<BrowserWindow> window2 = CreateBrowserWindow();
  std::unique_ptr<Browser> browser2 =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, false, window2.get());
  EXPECT_EQ(browser_list->size(), 2u);

  GURL url1 = GURL(kBaseUrl + base::NumberToString(1));
  GURL url2 = GURL(kBaseUrl + base::NumberToString(2));
  GURL url3 = GURL(kBaseUrl + base::NumberToString(3));

  AddTab(browser1, url1);
  AddTab(browser1, url1);
  AddTab(browser1, url2);
  AddTab(browser2.get(), url1);
  AddTab(browser2.get(), url2);
  AddTab(browser2.get(), url3);

  std::vector<content::RenderFrameHost*> expected_frames = {
      browser1->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame(),
      browser1->tab_strip_model()->GetWebContentsAt(1)->GetPrimaryMainFrame(),
      browser1->tab_strip_model()->GetWebContentsAt(2)->GetPrimaryMainFrame(),
      browser2->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame(),
      browser2->tab_strip_model()->GetWebContentsAt(1)->GetPrimaryMainFrame(),
      browser2->tab_strip_model()->GetWebContentsAt(2)->GetPrimaryMainFrame()};

  auto delegate = std::make_unique<UserNoteServiceDelegateImpl>(profile());
  std::vector<content::RenderFrameHost*> frames =
      delegate->GetAllFramesForUserNotes();

  EXPECT_EQ(frames.size(), 6u);
  for (content::RenderFrameHost* frame : frames) {
    EXPECT_TRUE(base::Contains(expected_frames, frame));
  }
  for (content::RenderFrameHost* frame : expected_frames) {
    EXPECT_TRUE(base::Contains(frames, frame));
  }

  browser2->tab_strip_model()->CloseAllTabs();
  browser2.reset();
  window2.reset();
}

}  // namespace user_notes
