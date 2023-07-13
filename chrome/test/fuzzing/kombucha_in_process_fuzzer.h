// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_FUZZING_KOMBUCHA_IN_PROCESS_FUZZER_H_
#define CHROME_TEST_FUZZING_KOMBUCHA_IN_PROCESS_FUZZER_H_

#include <google/protobuf/descriptor.h>
#include <stddef.h>
#include <cstdint>
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "chrome/test/fuzzing/kombucha_in_process_fuzzer.pb.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/element_identifier.h"
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

  // Step could be a StepBuilder or a Multistep
  // Returns step if the given target is visible
  // Otherwise returns a Log statement
  template <typename T>
  auto CheckStep(ui::ElementIdentifier target, T step, std::string step_name) {
    return Steps(IfElement(
        target,
        [](const ui::TrackedElement* element) { return element != nullptr; },
        Steps(std::move(step), Log("[KOMB] Added ", step_name, " with target ",
                                   target.GetName())),
        Steps(Log("[KOMB] Failed to add step: ", step_name, ". ",
                  target.GetName(), " was not visible"))));
  }

  // Used primarily for DragFromTo which requires two ElementIdentifiers
  // Check for dest first. If there is no source, we convert to DragMouseTo
  template <typename T>
  auto CheckStep(ui::ElementIdentifier source,
                 ui::ElementIdentifier dest,
                 T step,
                 std::string step_name) {
    return Steps(IfElement(
        dest,
        [](const ui::TrackedElement* element) { return element != nullptr; },
        Steps(IfElement(
            source,
            [](const ui::TrackedElement* element) {
              return element != nullptr;
            },
            Steps(std::move(step), Log("[KOMB] Added ", step_name,
                                       " with targets: ", dest.GetName(), " ",
                                       source.GetName())),
            Steps(DragMouseTo(dest),  // Dest but no source
                  Log("Added DragMouseTo with target: ", dest.GetName())))),
        Steps(Log("[KOMB] Failed to add step: ", step_name,
                  " Dest:", dest.GetName(), " was not visible"))));
  }

  // Custom Kombucha Verbs
  auto ShowBookmarksBar() {
    return Steps(PressButton(kAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }
  // Both ClickRight/ClickLeft are handled by ClickAt in protobuf file
  auto ClickRight(ui::ElementIdentifier target) {
    return Steps(MoveMouseTo(target), ClickMouse(ui_controls::RIGHT));
  }
  auto ClickLeft(ui::ElementIdentifier target) {
    return Steps(MoveMouseTo(target), ClickMouse(ui_controls::LEFT));
  }
  auto DragFromTo(ui::ElementIdentifier source, ui::ElementIdentifier dest) {
    return Steps(MoveMouseTo(source), DragMouseTo(dest));
  }

  // Enum descriptors for protobuf messages
  // Allows for a kombucha verb to function independent of what element
  // it's targeting
  raw_ptr<const google::protobuf::EnumDescriptor> target_descriptor =
      raw_ptr(test::fuzzing::ui_fuzzing::Target_descriptor());
  raw_ptr<const google::protobuf::EnumDescriptor> accelerator_descriptor =
      raw_ptr(test::fuzzing::ui_fuzzing::Accelerator_descriptor());

  ui::Accelerator current_accelerator_;

 private:
  // List that enables browser startup with custom features
  base::test::ScopedFeatureList scoped_feature_list_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  base::WeakPtrFactory<KombuchaInProcessFuzzer> weak_ptr_factory_{this};
};

#endif  // CHROME_TEST_FUZZING_KOMBUCHA_IN_PROCESS_FUZZER_H_
