// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_FUZZING_KOMBUCHA_IN_PROCESS_FUZZER_H_
#define CHROME_TEST_FUZZING_KOMBUCHA_IN_PROCESS_FUZZER_H_

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include <google/protobuf/descriptor.h>
#include <stddef.h>
#include <cstdint>
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/kombucha_in_process_fuzzer.pb.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test.h"
class KombuchaInProcessFuzzer
    : virtual public InteractiveBrowserTestT<InProcessFuzzer> {
 public:
  KombuchaInProcessFuzzer();
  ~KombuchaInProcessFuzzer() override;
  void SetUp() override;
  void SetUpOnMainThread() override;

#if BUILDFLAG(IS_WIN)
  void TearDown() override;
#endif

  using FuzzCase = test::fuzzing::ui_fuzzing::FuzzCase;
  int Fuzz(const uint8_t* data, size_t size) override;
  static std::unique_ptr<net::test_server::HttpResponse> HandleHTTPRequest(
      base::WeakPtr<KombuchaInProcessFuzzer> fuzzer_weak,
      const net::test_server::HttpRequest& request);

  FuzzCase current_fuzz_case_;

  // Custom Kombucha Verbs
  auto ShowBookmarksBar() {
    return Steps(PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }
  // Both ClickRight/ClickLeft are handled by ClickAt in protobuf file
  auto ClickRight(ElementSpecifier target) {
    return Steps(MoveMouseTo(target), ClickMouse(ui_controls::RIGHT, true));
  }
  auto ClickLeft(ElementSpecifier target) {
    return Steps(MoveMouseTo(target), ClickMouse(ui_controls::LEFT, true));
  }
  auto DragFromTo(ElementSpecifier source, ElementSpecifier dest) {
    return Steps(MoveMouseTo(source), DragMouseTo(dest));
  }

  // Custom verbs copied from saved_tab_groups_interactive_uitest.cc to interact
  // with tab groups
  auto SaveGroupLeaveEditorBubbleOpen(tab_groups::TabGroupId group_id) {
    return Steps(EnsureNotPresent(kTabGroupEditorBubbleId),
                 ClickTabGroupHeader(group_id, true),
                 WaitForShow(kTabGroupEditorBubbleId),
                 PressButton(kTabGroupEditorBubbleSaveToggleId));
  }

  MultiStep ClickTab(int index, bool right_click) {
    const char kTabToClick[] = "Tab to Click";
    return Steps(
        NameDescendantViewByType<Tab>(kBrowserViewElementId, kTabToClick,
                                      index),
        right_click ? ClickRight(kTabToClick) : ClickLeft(kTabToClick));
  }

  MultiStep ClickTabGroupHeader(tab_groups::TabGroupId group_id,
                                bool right_click) {
    const char kTabGroupToClick[] = "Tab group header to click";
    return Steps(
        WithView(kTabStripElementId,
                 [](TabStrip* tab_strip) { tab_strip->StopAnimating(true); }),
        NameDescendantView(
            kBrowserViewElementId, kTabGroupToClick,
            base::BindRepeating(
                [](tab_groups::TabGroupId group_id, const views::View* view) {
                  const TabGroupHeader* header =
                      views::AsViewClass<TabGroupHeader>(view);
                  if (!header) {
                    return false;
                  }
                  return header->group().value() == group_id;
                },
                group_id)),
        right_click ? ClickRight(kTabGroupToClick)
                    : ClickLeft(kTabGroupToClick));
  }

  auto SaveGroupAndCloseEditorBubble(tab_groups::TabGroupId group_id) {
    return Steps(SaveGroupLeaveEditorBubbleOpen(group_id),
                 ClickTabGroupHeader(group_id, false));
  }

  // Enum descriptors for protobuf messages
  // Allows for a kombucha verb to function independent of what element
  // it's targeting
  raw_ptr<const google::protobuf::EnumDescriptor> accelerator_descriptor =
      raw_ptr(test::fuzzing::ui_fuzzing::Accelerator_descriptor());

  ui::Accelerator current_accelerator_;

 private:
  // Cleans the browser once the fuzzing iteration is over. This helps
  // determinism when trying to reproduce.
  void CleanInProcessBrowserState();
  // List that enables browser startup with custom features
  base::test::ScopedFeatureList scoped_feature_list_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  base::WeakPtrFactory<KombuchaInProcessFuzzer> weak_ptr_factory_{this};
};

#endif  // CHROME_TEST_FUZZING_KOMBUCHA_IN_PROCESS_FUZZER_H_
