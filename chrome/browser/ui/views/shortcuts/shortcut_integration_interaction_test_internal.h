// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHORTCUTS_SHORTCUT_INTEGRATION_INTERACTION_TEST_INTERNAL_H_
#define CHROME_BROWSER_UI_VIEWS_SHORTCUTS_SHORTCUT_INTEGRATION_INTERACTION_TEST_INTERNAL_H_

#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "chrome/test/interaction/interactive_browser_test_internal.h"

namespace shortcuts {

// Class that provides functionality needed by
// ShortcutIntegrationInteractionTestApi but which should not be directly
// visible to tests inheriting from the API class.
class ShortcutIntegrationInteractionTestPrivate
    : public internal::InteractiveBrowserTestPrivate {
 public:
  ShortcutIntegrationInteractionTestPrivate();
  ~ShortcutIntegrationInteractionTestPrivate() override;

  // internal::InteractiveBrowserTestPrivate:
  void DoTestSetUp() override;
  void DoTestTearDown() override;

  // Associates the next newly created file to be detected in the monitored
  // directory with `identifier`. For now we only support one pending "next"
  // shortcut. This shortcut needs to be created (and detected as having been
  // created) before another "next" shortcut can be identified.
  void SetNextShortcutIdentifier(ui::ElementIdentifier identifier);

  // Gets the path from a tracked element as identified by
  // `SetNextShortcutIdentifier`.
  static base::FilePath GetShortcutPath(ui::TrackedElement* element);

 private:
  class ShortcutTracker;

  std::unique_ptr<ShortcutCreationTestSupport> test_support_;
  std::unique_ptr<ShortcutTracker> shortcut_tracker_;
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_UI_VIEWS_SHORTCUTS_SHORTCUT_INTEGRATION_INTERACTION_TEST_INTERNAL_H_
