// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHORTCUTS_SHORTCUT_INTEGRATION_INTERACTION_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_SHORTCUTS_SHORTCUT_INTEGRATION_INTERACTION_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"

namespace shortcuts {

class ShortcutIntegrationInteractionTestPrivate;

// API class that provides both base browser Kombucha functionality and
// additional logic to facilitate writing tests for the "Create Shortcut"
// feature.
class ShortcutIntegrationInteractionTestApi : public InteractiveBrowserTestApi {
 public:
  ShortcutIntegrationInteractionTestApi();
  ~ShortcutIntegrationInteractionTestApi() override;

  // Triggers the "create shortcut" dialog and waits for the dialog to show.
  [[nodiscard]] MultiStep ShowCreateShortcutDialog();

  // Triggers and accepts the "create shortcut" dialog.
  [[nodiscard]] MultiStep ShowAndAcceptCreateShortcutDialog();

  // Same as `ShowAndAcceptCreateShortcutDialog()`, but sets the title in the
  // dialog to `title` before accepting the dialog.
  [[nodiscard]] MultiStep ShowCreateShortcutDialogSetTitleAndAccept(
      const std::u16string& title);

  // Gives the next shortcut to be created an identifier, to allow interacting
  // with it in subsequent steps. Make sure to not trigger a second copy of this
  // step until the shortcut for the first copy has been created.
  [[nodiscard]] StepBuilder InstrumentNextShortcut(
      ui::ElementIdentifier identifier);

  // Launches the given shortcut.
  [[nodiscard]] StepBuilder LaunchShortcut(ui::ElementIdentifier identifier);

  // Check that `matcher` matches (the base::FilePath for) the shortcut
  // identified by `identifier`.
  //
  // Note that like `CheckElement()`, unless you add
  // .SetMustBeVisibleAtStart(true), this test step will wait for `identifier`
  // to be shown (i.e. created) before proceeding.
  template <typename M>
  [[nodiscard]] static StepBuilder CheckShortcut(
      ui::ElementIdentifier identifier,
      M&& matcher) {
    return InAnyContext(CheckElement(identifier, &GetShortcutPath, matcher));
  }

  // Gets the path from a tracked element as identified by
  // `InstrumentNextShortcut`. Can for example be used with `CheckElement()` to
  // check properties of the shortcut.
  static base::FilePath GetShortcutPath(ui::TrackedElement* element);

 private:
  base::test::ScopedFeatureList feature_list_{features::kShortcutsNotApps};

  ShortcutIntegrationInteractionTestPrivate& test_impl();
};

// Template for adding ShortcutIntegrationInteractionTestApi to any test fixture
// which is derived from InProcessBrowserTest.
//
// If you don't need to derive from some existing test class, prefer to use
// ShortcutIntegrationInteractionTestBase.
template <typename T>
  requires std::derived_from<T, InProcessBrowserTest>
class ShortcutIntegrationInteractionTestT
    : public T,
      public ShortcutIntegrationInteractionTestApi {
 public:
  template <typename... Args>
  explicit ShortcutIntegrationInteractionTestT(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~ShortcutIntegrationInteractionTestT() override = default;

 protected:
  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    private_test_impl().DoTestSetUp();
    if (Browser* browser = T::browser()) {
      SetContextWidget(
          BrowserView::GetBrowserViewForBrowser(browser)->GetWidget());
    }
    ASSERT_TRUE(T::embedded_https_test_server().Start());
  }

  void TearDownOnMainThread() override {
    private_test_impl().DoTestTearDown();
    T::TearDownOnMainThread();
  }
};

// Convenience test fixture for shortcut integration tests. This is the
// preferred base class for Kombucha tests unless you specifically need
// something else.
using ShortcutIntegrationInteractionTestBase =
    ShortcutIntegrationInteractionTestT<InProcessBrowserTest>;

}  // namespace shortcuts

#endif  // CHROME_BROWSER_UI_VIEWS_SHORTCUTS_SHORTCUT_INTEGRATION_INTERACTION_TEST_BASE_H_
