// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/split_tab_collection.h"
#include "chrome/browser/ui/tabs/split_tab_visual_data.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/test/split_tabs_interactive_test_mixin.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_observer.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents2Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents3Id);
}  // namespace

class SplitTabButtonInteractiveTest
    : public SplitTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    SplitTabsInteractiveTestMixin::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  auto UpdateSplitTabButtonPinState(bool should_pin) {
    return Do([=, this]() {
      browser()->profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton,
                                                   should_pin);
    });
  }

  auto CheckSplitTabButtonIcon(const gfx::VectorIcon& expected_icon) {
    return CheckView(
        kToolbarSplitTabsToolbarButtonElementId,
        [](SplitTabsToolbarButton* button) {
          auto vector_icons = button->GetIconsForTesting();
          CHECK(vector_icons.has_value());
          return vector_icons->icon.name;
        },
        expected_icon.name);
  }
};

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, PinSplitTabButton) {
  RunTestSequence(EnsureNotPresent(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(false),
                  WaitForHide(kToolbarSplitTabsToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest,
                       UnpinSplitTabWhileActive) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(InstrumentTab(kWebContents1Id),
                  NavigateWebContents(kWebContents1Id, url1),
                  AddInstrumentedTab(kWebContents2Id, url1),
                  AddInstrumentedTab(kWebContents3Id, url1),
                  UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
                  EnsurePresent(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(false),
                  EnsurePresent(kToolbarSplitTabsToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, DefaultButtonIcon) {
  RunTestSequence(UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  CheckSplitTabButtonIcon(kSplitSceneIcon));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, ButtonIconUpdates) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(InstrumentTab(kWebContents1Id),
                  NavigateWebContents(kWebContents1Id, url1),
                  AddInstrumentedTab(kWebContents2Id, url1),
                  SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  CheckSplitTabButtonIcon(kSplitSceneLeftIcon),
                  FocusInactiveTabInSplit(),
                  EnsurePresent(kToolbarSplitTabsToolbarButtonElementId),
                  CheckSplitTabButtonIcon(kSplitSceneRightIcon));
}
