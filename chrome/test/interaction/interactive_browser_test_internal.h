// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_
#define CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_

#include <map>
#include <memory>

#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/interactive_views_test_internal.h"

class InteractiveBrowserTestApi;

namespace internal {

// Class that provides functionality needed by InteractiveBrowserTestApi but
// which should not be directly visible to tests inheriting from the API class.
class InteractiveBrowserTestPrivate
    : public views::test::internal::InteractiveViewsTestPrivate {
 public:
  explicit InteractiveBrowserTestPrivate(
      std::unique_ptr<InteractionTestUtilBrowser> test_util);
  ~InteractiveBrowserTestPrivate() override;

  // views::test::internal::InteractiveViewsTestPrivate:
  void DoTestTearDown() override;

 private:
  friend InteractiveBrowserTestApi;

  // Stores instrumented WebContents and WebUI for lookup.
  std::map<ui::ElementIdentifier,
           std::unique_ptr<WebContentsInteractionTestUtil>>
      instrumented_web_contents_;
};

}  // namespace internal

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_
