// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/shortcuts/shortcut_integration_interaction_test_base.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace shortcuts {

namespace {

// Identifier for the initially created tab.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInitialTabId);
// Identifier for the tab opened as a result of launching a shortcut.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);
// Identifier for the shortcut created in these tests.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewShortcutId);

}  // namespace

using ShortcutIntegrationInteractiveUiTest =
    ShortcutIntegrationInteractionTestBase;

IN_PROC_BROWSER_TEST_F(ShortcutIntegrationInteractiveUiTest, CreateAndLaunch) {
  const GURL kPageWithIconsUrl =
      embedded_https_test_server().GetURL("/shortcuts/page_icons.html");
  RunTestSequence(
      InstrumentTab(kInitialTabId),
      NavigateWebContents(kInitialTabId, kPageWithIconsUrl),
      InstrumentNextShortcut(kNewShortcutId),
      ShowAndAcceptCreateShortcutDialog(),
      CheckShortcut(kNewShortcutId, IsShortcutForUrl(kPageWithIconsUrl)),
      InstrumentNextTab(kNewTabId), LaunchShortcut(kNewShortcutId),
      WaitForWebContentsNavigation(kNewTabId, kPageWithIconsUrl));
}

IN_PROC_BROWSER_TEST_F(ShortcutIntegrationInteractiveUiTest, CustomTitle) {
  const GURL kPageWithIconsUrl =
      embedded_https_test_server().GetURL("/shortcuts/page_icons.html");
  RunTestSequence(
      InstrumentTab(kInitialTabId),
      NavigateWebContents(kInitialTabId, kPageWithIconsUrl),
      InstrumentNextShortcut(kNewShortcutId),
      ShowCreateShortcutDialogSetTitleAndAccept(u"Hello World!"),
      CheckShortcut(kNewShortcutId, IsShortcutWithTitle(u"Hello World!")));
}

IN_PROC_BROWSER_TEST_F(ShortcutIntegrationInteractiveUiTest,
                       MultipleShortcuts) {
  const GURL kPageWithIconsUrl =
      embedded_https_test_server().GetURL("/shortcuts/page_icons.html");
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondShortcutId);

  base::FilePath shortcut_path;
  RunTestSequence(
      InstrumentTab(kInitialTabId),
      NavigateWebContents(kInitialTabId, kPageWithIconsUrl),

      InstrumentNextShortcut(kNewShortcutId),
      ShowAndAcceptCreateShortcutDialog(),
      CheckShortcut(kNewShortcutId, IsShortcutForUrl(kPageWithIconsUrl)),

      InstrumentNextShortcut(kSecondShortcutId),
      ShowAndAcceptCreateShortcutDialog(),
      CheckShortcut(kSecondShortcutId, IsShortcutForUrl(kPageWithIconsUrl)),

      CheckShortcut(kNewShortcutId,
                    IsShortcutWithTitle(u"Page with icon links")),
      CheckShortcut(kSecondShortcutId,
                    IsShortcutWithTitle(u"Page with icon links")),

      // Sanity check that the two created shortcuts are in fact separate
      // shortcuts, despite being for the same page and profile.
      InAnyContext(WithElement(kNewShortcutId,
                               [&](ui::TrackedElement* shortcut) {
                                 shortcut_path = GetShortcutPath(shortcut);
                               })),
      CheckShortcut(kSecondShortcutId, testing::Ne(std::cref(shortcut_path))));
}

namespace {
// Initial tab in profile 1.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfile1TabId);
// Initial tab in profile 2.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfile2TabId);
// Shortcut created in profile 1.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfile1ShortcutId);
// Shortcut created in profile 2.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfile2ShortcutId);
}  // namespace

class ShortcutIntegrationMultiProfileInteractiveUiTest
    : public ShortcutIntegrationInteractiveUiTest {
 public:
  void SetUpOnMainThread() override {
    ShortcutIntegrationInteractiveUiTest::SetUpOnMainThread();

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    profile2_ = profiles::testing::CreateProfileSync(profile_manager, new_path)
                    .GetWeakPtr();
    profile2_browser_ =
        chrome::OpenEmptyWindow(profile2(),
                                /*should_trigger_session_restore=*/false)
            ->AsWeakPtr();
  }

  Profile* profile1() { return browser()->profile(); }
  Profile* profile2() { return profile2_.get(); }
  Browser* profile1_browser() { return browser(); }
  Browser* profile2_browser() { return profile2_browser_.get(); }

  GURL profile1_shortcut_url() {
    return embedded_https_test_server().GetURL("/shortcuts/page_icons.html");
  }
  GURL profile2_shortcut_url() {
    return embedded_https_test_server().GetURL("/shortcuts/no_icons_page.html");
  }

  // Returns a step that behaves as if the user deleted the given profile.
  [[nodiscard]] static MultiStep DeleteProfile(Profile* profile) {
    base::FilePath profile_path = profile->GetPath();
    return Steps(Do([profile_path] {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
          profile_path, base::DoNothing(),
          ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
    }));
  }

  // Returns a step that locks the given profile.
  [[nodiscard]] static MultiStep LockProfile(Profile* profile) {
    base::FilePath profile_path = profile->GetPath();
    return Steps(Do([profile_path] {
      ProfileAttributesEntry* entry =
          g_browser_process->profile_manager()
              ->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path);
      ASSERT_NE(entry, nullptr);
      entry->LockForceSigninProfile(true);
      EXPECT_TRUE(entry->IsSigninRequired());
    }));
  }

  // Creates shortcuts in both profiles.
  [[nodiscard]] MultiStep CreateShortcuts() {
    return Steps(InstrumentTab(kProfile1TabId, /*tab_index=*/std::nullopt,
                               profile1_browser()),
                 InstrumentTab(kProfile2TabId, /*tab_index=*/std::nullopt,
                               profile2_browser()),
                 NavigateWebContents(kProfile1TabId, profile1_shortcut_url()),
                 NavigateWebContents(kProfile2TabId, profile2_shortcut_url()),

                 InstrumentNextShortcut(kProfile2ShortcutId),
                 InContext(profile2_browser()->window()->GetElementContext(),
                           ShowAndAcceptCreateShortcutDialog()),
                 InAnyContext(WaitForShow(kProfile2ShortcutId)),

                 InstrumentNextShortcut(kProfile1ShortcutId),
                 InContext(profile1_browser()->window()->GetElementContext(),
                           ShowAndAcceptCreateShortcutDialog()),
                 InAnyContext(WaitForShow(kProfile1ShortcutId)));
  }

  static base::FilePath ProfilePathFromWebContents(ui::TrackedElement* te) {
    auto* const wc =
        ShortcutIntegrationInteractiveUiTest::AsInstrumentedWebContents(te);
    return wc->web_contents()->GetBrowserContext()->GetPath();
  }

 private:
  base::WeakPtr<Profile> profile2_;
  base::WeakPtr<Browser> profile2_browser_;
};

IN_PROC_BROWSER_TEST_F(ShortcutIntegrationMultiProfileInteractiveUiTest,
                       CreatedForCorrectProfile) {
  RunTestSequence(
      CreateShortcuts(),

      CheckShortcut(kProfile1ShortcutId,
                    IsShortcutWithTitle(u"Page with icon links (Person 1)")),
      CheckShortcut(kProfile1ShortcutId,
                    IsShortcutForProfile(profile1()->GetPath())),
      CheckShortcut(kProfile2ShortcutId,
                    IsShortcutWithTitle(u"Page without icons (Person 2)")),
      CheckShortcut(kProfile2ShortcutId,
                    IsShortcutForProfile(profile2()->GetPath())));
}

IN_PROC_BROWSER_TEST_F(ShortcutIntegrationMultiProfileInteractiveUiTest,
                       LaunchInCorrectProfile) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfile1NewTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfile2NewTabId);

  RunTestSequence(
      CreateShortcuts(),

      // Shortcut from profile 1 should open in profile 1.
      InstrumentNextTab(kProfile1NewTabId, AnyBrowser()),
      LaunchShortcut(kProfile1ShortcutId),
      WaitForWebContentsNavigation(kProfile1NewTabId, profile1_shortcut_url()),
      InAnyContext(CheckElement(kProfile1NewTabId, &ProfilePathFromWebContents,
                                ::testing::Eq(profile1()->GetPath()))),

      // Similarly, shortcut from profile 2 should open in profile 2.
      InstrumentNextTab(kProfile2NewTabId, AnyBrowser()),
      LaunchShortcut(kProfile2ShortcutId),
      WaitForWebContentsNavigation(kProfile2NewTabId, profile2_shortcut_url()),
      InAnyContext(CheckElement(kProfile2NewTabId, &ProfilePathFromWebContents,
                                ::testing::Eq(profile2()->GetPath()))),

      // Now delete profile 2; launching shortcut from profile 2 should now open
      // in profile 1.
      DeleteProfile(profile2()), InstrumentNextTab(kNewTabId, AnyBrowser()),
      LaunchShortcut(kProfile2ShortcutId),
      WaitForWebContentsNavigation(kNewTabId, profile2_shortcut_url()),
      InAnyContext(CheckElement(kNewTabId, &ProfilePathFromWebContents,
                                ::testing::Eq(profile1()->GetPath()))));
}

IN_PROC_BROWSER_TEST_F(ShortcutIntegrationMultiProfileInteractiveUiTest,
                       DontLaunchInLockedProfile) {
  signin_util::ScopedForceSigninSetterForTesting signin_setter(true);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfilePickerViewId);
  ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
      base::BindLambdaForTesting([kProfilePickerViewId] {
        ProfilePicker::GetViewForTesting()->SetProperty(
            views::kElementIdentifierKey, kProfilePickerViewId);
      }));

  RunTestSequence(
      CreateShortcuts(),

      // Shortcut from locked profile 2 should cause profile picker to open.
      LockProfile(profile2()), LaunchShortcut(kProfile2ShortcutId),
      InAnyContext(WaitForShow(kProfilePickerViewId)));
}

}  // namespace shortcuts
