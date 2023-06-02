// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_notes/user_notes_tab_helper.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/user_notes/user_note_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/browser/user_note_service.h"
#include "components/user_notes/interfaces/user_note_service_delegate.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Mock;

namespace user_notes {

namespace {

const char kTestDomain[] = "http://www.google.com/";

// Matcher that verifies whether the page has an attached `UserNoteManager` via
// the navigation handle supplied as arg.
MATCHER(HasUserNoteManager, "") {
  return arg != nullptr && UserNoteManager::GetForPage(
                               arg->GetRenderFrameHost()->GetPage()) != nullptr;
}

}  // namespace

class MockUserNoteService : public UserNoteService {
 public:
  // A service delegate and user note storage are not needed for these tests, so
  // pass nullptr.
  MockUserNoteService()
      : UserNoteService(/*delegate=*/nullptr, /*storage=*/nullptr) {}

  MOCK_METHOD(void,
              OnFrameNavigated,
              (content::RenderFrameHost * rfh),
              (override));
};

class MockUserNotesTabHelper : public UserNotesTabHelper {
 public:
  explicit MockUserNotesTabHelper(content::WebContents* web_contents)
      : UserNotesTabHelper(web_contents) {}

  MOCK_METHOD(void,
              DidFinishNavigation,
              (content::NavigationHandle * navigation_handle),
              (override));
};

class UserNotesTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(user_notes::kUserNotes);

    UserNoteServiceFactory::SetServiceForTesting(&mock_user_note_service_);

    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    UserNoteServiceFactory::SetServiceForTesting(nullptr);
  }

  void AttachTabHelper(std::unique_ptr<UserNotesTabHelper> tab_helper) {
    const auto* user_data_key = tab_helper->UserDataKey();
    web_contents()->SetUserData(user_data_key, std::move(tab_helper));
  }

  void GoBack() { content::NavigationSimulator::GoBack(web_contents()); }

  void GoForward() { content::NavigationSimulator::GoForward(web_contents()); }

  void SimluateNavigation() {
    // Note: Simulating a navigation with the same URL as before causes the
    // navigation to be treated as a refresh, so use a counter to keep the URLs
    // unique.
    static int unique_int = 0;
    NavigateAndCommit(GURL(kTestDomain + base::NumberToString(++unique_int)));
  }

  content::NavigationEntry* GetLastCommittedEntry() {
    return web_contents()->GetController().GetLastCommittedEntry();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  MockUserNoteService mock_user_note_service_;
};

// Tests that by the time `UserNotesTabHelper::DidFinishNavigation` is called,
// a `UserNoteManager` is already attached to the `Page` of the navigated
// frame.
TEST_F(UserNotesTabHelperTest, AttachUserNoteManagerToPage) {
  // Partially mock the `UserNotesTabHelper` to intercept calls to
  // `DidFinishNavigation` and prevent side effects.
  auto mock_tab_helper =
      std::make_unique<MockUserNotesTabHelper>(web_contents());
  EXPECT_CALL(*mock_tab_helper, DidFinishNavigation(HasUserNoteManager()))
      .Times(4);
  AttachTabHelper(std::move(mock_tab_helper));

  // Verify for a normal navigation.
  SimluateNavigation();

  // Verify for a back navigation.
  SimluateNavigation();
  GoBack();

  // Verify for a forward navigation.
  GoForward();
}

// Tests that the `UserNotesTabHelper` properly notifies the `UserNoteService`
// when a frame navigates.
TEST_F(UserNotesTabHelperTest, NotifyServiceOfNavigationWhenNeeded) {
  // Use a real `UserNotesTabHelper` so the service gets noitified.
  std::unique_ptr<UserNotesTabHelper> tab_helper =
      UserNotesTabHelper::CreateForTest(web_contents());
  AttachTabHelper(std::move(tab_helper));

  // Verify for a normal navigation.
  auto* service = static_cast<MockUserNoteService*>(
      UserNoteServiceFactory::GetForContext(nullptr));
  EXPECT_CALL(*service, OnFrameNavigated(_)).Times(1);
  SimluateNavigation();
  Mock::VerifyAndClearExpectations(service);

  // Verify for a back navigation.
  EXPECT_CALL(*service, OnFrameNavigated(_)).Times(2);
  SimluateNavigation();
  GoBack();
  Mock::VerifyAndClearExpectations(service);

  // Verify for a forward navigation.
  EXPECT_CALL(*service, OnFrameNavigated(_)).Times(1);
  GoForward();
  Mock::VerifyAndClearExpectations(service);
}

}  // namespace user_notes
