// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <stddef.h>
#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/numerics/clamped_math.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/autofill_assistant/browser/base_browsertest.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/selector_observer.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {

using ::testing::AnyOf;
using ::testing::IsEmpty;
using ::testing::Return;

class BatchElementCheckerBrowserTest
    : public autofill_assistant::BaseBrowserTest,
      public content::WebContentsObserver {
 public:
  BatchElementCheckerBrowserTest() {}

  BatchElementCheckerBrowserTest(const BatchElementCheckerBrowserTest&) =
      delete;
  BatchElementCheckerBrowserTest& operator=(
      const BatchElementCheckerBrowserTest&) = delete;

  ~BatchElementCheckerBrowserTest() override {}

  void SetUpOnMainThread() override {
    BaseBrowserTest::SetUpOnMainThread();
    web_controller_ = WebController::CreateForWebContents(
        shell()->web_contents(), &user_data_, &log_info_,
        /* annotate_dom_model_service= */ nullptr,
        /* enable_full_stack_traces= */ false);
    Observe(shell()->web_contents());
  }

  static ElementConditionProto AllOfConditions(
      std::vector<ElementConditionProto> conditions) {
    ElementConditionProto proto;
    for (auto& condition : conditions) {
      proto.mutable_any_of()->mutable_conditions()->Add(std::move(condition));
    }
    return proto;
  }

  static ElementConditionProto AnyOfConditions(
      std::vector<ElementConditionProto> conditions) {
    ElementConditionProto proto;
    for (auto& condition : conditions) {
      proto.mutable_any_of()->mutable_conditions()->Add(std::move(condition));
    }
    return proto;
  }

  static ElementConditionProto NoneOfConditions(
      std::vector<ElementConditionProto> conditions) {
    ElementConditionProto proto;
    for (auto& condition : conditions) {
      proto.mutable_none_of()->mutable_conditions()->Add(std::move(condition));
    }
    return proto;
  }

  static ElementConditionProto Match(Selector selector,
                                     bool strict = false,
                                     std::string payload = "") {
    ElementConditionProto proto;
    *proto.mutable_match() = selector.proto;
    proto.set_require_unique_element(strict);
    if (!payload.empty()) {
      proto.set_payload(payload);
    }
    return proto;
  }

  static SelectorObserver::Settings SelectorObserverDefaultSettings(
      base::TimeDelta max_wait_time) {
    return {max_wait_time, base::Seconds(1), base::Seconds(15),
            base::Milliseconds(100)};
  }

  // Run Observer BatchElementChecker on the provided conditions. The second
  // value in the pairs (bool) is the match expectation.
  void RunObserverBatchElementChecker(
      const std::vector<std::pair<ElementConditionProto, bool>>& conditions,
      base::flat_set<std::string> expected_payloads = {},
      base::TimeDelta max_wait_time = base::Seconds(30)) {
    base::RunLoop run_loop;
    BatchElementChecker checker;
    std::vector<bool> actual_results(conditions.size(), false);
    std::vector<bool> expected_results(conditions.size(), false);

    for (size_t i = 0; i < conditions.size(); ++i) {
      expected_results[i] = conditions[i].second;
      checker.AddElementConditionCheck(
          conditions[i].first,
          base::BindOnce(&BatchElementCheckerBrowserTest::
                             ObserverBatchElementCheckerElementCallback,
                         &actual_results, i, &expected_payloads));
    }
    checker.AddAllDoneCallback(base::BindOnce(
        &BatchElementCheckerBrowserTest::
            ObserverBatchElementCheckerAllDoneCallback,
        run_loop.QuitClosure(), &expected_results, &actual_results));

    checker.EnableObserver(SelectorObserverDefaultSettings(max_wait_time));
    checker.Run(web_controller_.get());
    run_loop.Run();
    EXPECT_EQ(expected_payloads.size(), 0u);
    EXPECT_EQ(web_controller_->pending_workers_.size(), 0u);
  }

  static void ObserverBatchElementCheckerElementCallback(
      std::vector<bool>* res,
      size_t i,
      base::flat_set<std::string>* missing_payloads,
      const ClientStatus& status,
      const std::vector<std::string>& payloads,
      const std::vector<std::string>& tags,
      const base::flat_map<std::string, DomObjectFrameStack>& elms) {
    (*res)[i] = status.ok();
    for (const std::string& payload : payloads) {
      EXPECT_EQ(missing_payloads->erase(payload), 1u)
          << "Got unexpected payload " << payload;
    }
  }

  static void ObserverBatchElementCheckerAllDoneCallback(
      base::OnceClosure on_done,
      const std::vector<bool>* expected,
      const std::vector<bool>* actual) {
    for (size_t i = 0; i < expected->size(); ++i) {
      EXPECT_EQ(actual->at(i), expected->at(i)) << "condition number " << i;
    }
    std::move(on_done).Run();
  }

 protected:
  std::unique_ptr<WebController> web_controller_;
  UserData user_data_;
  UserModel user_model_;
  ProcessedActionStatusDetailsProto log_info_;
};

IN_PROC_BROWSER_TEST_F(BatchElementCheckerBrowserTest,
                       ObserverBatchElementCheckerStaticConditions) {
  RunObserverBatchElementChecker({
      {Match(Selector({"#button"})), true},        // A visible element.
      {Match(Selector({"#hidden"})), true},        // A hidden element.
      {Match(Selector({"#doesnotexist"})), false}  // A nonexistent element.
  });

  RunObserverBatchElementChecker({{
      AllOfConditions({
          Match(Selector({"#button"})),  // A visible element.
          Match(Selector({"#hidden"})),  // A hidden element.
      }),
      true  // Expected to match.
  }});

  RunObserverBatchElementChecker({{
      AnyOfConditions({
          Match(Selector({"#button"})),       // A visible element.
          Match(Selector({"#doesnotexist"}))  // A nonexistent element.
      }),
      true  // Expected to match.
  }});

  RunObserverBatchElementChecker({{
      NoneOfConditions({// A nonexistent element.
                        Match(Selector({"#doesnotexist"})),
                        // A non-existent element inside an iFrame.
                        Match(Selector({"#iframe", "#doesnotexists"}))}),
      true  // Expected to match.
  }});

  RunObserverBatchElementChecker({{
      AllOfConditions(
          {Match(Selector({"#iframe"})),          // An iFrame.
           Match(Selector({"#iframeExternal"})),  // An OOPIF.
           Match(Selector(
               {"#iframe", "#button"})),  // An element in a same-origin iFrame.
           Match(Selector(
               {"#iframeExternal", "#button"})),  // An element in an OOPIF.
           NoneOfConditions(
               {// A non-existent element in an OOPIF.
                Match(Selector({"#iframeExternal", "#doesnotexist"})),
                // A non-existent element in a same-origin iFrame.
                Match(Selector({"#iframe", "#doesnotexist"}))})}),
      true  // Expected to match.
  }});

  RunObserverBatchElementChecker(
      {{
          AnyOfConditions({// A visible element.
                           Match(Selector({"#button"}), false, "a"),
                           // A nonexistent element.
                           Match(Selector({"#doesnotexist"}), false, "b")}),
          true,  // Expected to match.
      }},
      /* expected_payloads= */ {"a"});

  RunObserverBatchElementChecker(
      {{
          NoneOfConditions({// A visible element.
                            Match(Selector({"#button"}), false, "a"),
                            // A nonexistent element.
                            Match(Selector({"#doesnotexist"}), false, "b")}),
          false,  // Expected to not match.
      }},
      // Payload should be there even if root condition doesn't match.
      /* expected_payloads= */ {"a"},
      // Condition will never match, so no need to wait a long time for it.
      /* max_wait_time= */ base::Milliseconds(1));
}

IN_PROC_BROWSER_TEST_F(BatchElementCheckerBrowserTest,
                       ObserverBatchElementCheckerDynamicElements) {
  RunObserverBatchElementChecker({{
      // A selector that only matches for ~200ms.
      Match(Selector({".dynamic.about-2-seconds"})),
      true  // Expected to match.
  }});

  RunObserverBatchElementChecker({{
      // A selector that only matches for ~200ms inside of an iFrame.
      Match(Selector({"#iframe", ".dynamic.about-2-seconds"})),
      true  // Expected to match.
  }});

  RunObserverBatchElementChecker({{
      // A selector that only matches for ~200ms inside of an external iFrame.
      Match(Selector({"#iframeExternal", ".dynamic.about-2-seconds"})),
      true  // Expected to match.
  }});
}

IN_PROC_BROWSER_TEST_F(BatchElementCheckerBrowserTest,
                       ObserverBatchElementCheckerDifferentFilters) {
  Selector non_empty_bounding_box = Selector({"#button"});
  non_empty_bounding_box.proto.add_filters()
      ->mutable_bounding_box()
      ->set_require_nonempty(true);

  // Matches exactly one visible element.
  auto with_inner_text =
      Selector({"#with_inner_text span"}).MatchingInnerText("hello, world");

  Selector match_css_selector({"label"});
  match_css_selector.MatchingInnerText("terms and conditions");
  match_css_selector.proto.add_filters()->mutable_labelled();
  match_css_selector.proto.add_filters()->set_match_css_selector(
      "input[type='checkbox']");

  RunObserverBatchElementChecker({{
      AllOfConditions(
          {// A visible element.
           Match(Selector({"#button"}).MustBeVisible()),
           // An element in a same-origin iFrame.
           Match(Selector({"#iframe", "#button"}).MustBeVisible()),
           Match(non_empty_bounding_box), Match(with_inner_text),
           Match(with_inner_text.MustBeVisible()), Match(match_css_selector),
           NoneOfConditions(
               {// A hidden element.
                Match(Selector({"#hidden"}).MustBeVisible()),
                // A non-existent element.
                Match(Selector({"#doesnotexist"}).MustBeVisible())})}),
      true  // Expected to match.
  }});
}

IN_PROC_BROWSER_TEST_F(BatchElementCheckerBrowserTest, SelectorObserver) {
  base::RunLoop run_loop;
  // Selector ids can be any number as long as unique.
  const SelectorObserver::SelectorId button_id(11);
  const SelectorObserver::SelectorId iframe_button_id(1234);
  const SelectorObserver::SelectorId dynamic_id(0);

  std::vector<base::flat_set<std::pair<SelectorObserver::SelectorId, bool>>>
      expected_updates = {
          {
              // Initial state.
              std::make_pair(button_id, /* match = */ true),
              std::make_pair(iframe_button_id, /* match = */ true),
              std::make_pair(dynamic_id, /* match = */ false),
          },
          {
              // Dynamic element matches about 2s in.
              std::make_pair(dynamic_id, /* match = */ true),
          },
          {
              // Then shortly stops matching.
              std::make_pair(dynamic_id, /* match = */ false),
          }};

  auto element_callback = base::BindLambdaForTesting(
      [&](const ClientStatus& status,
          const base::flat_map<SelectorObserver::SelectorId,
                               DomObjectFrameStack>& elements) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(elements.size(), 1u);
        const auto& entry = elements.find(iframe_button_id);
        ASSERT_NE(entry, elements.end());
        EXPECT_TRUE(entry->second.render_frame_id);
        run_loop.Quit();
      });

  int button_element_id = -1;
  SelectorObserver::Callback update_callback = base::BindLambdaForTesting(
      [&](const ClientStatus& status,
          const std::vector<SelectorObserver::Update>& updates,
          SelectorObserver* observer) {
        EXPECT_TRUE(status.ok());
        EXPECT_FALSE(expected_updates.empty());
        for (auto& update : updates) {
          auto removed = expected_updates[0].erase(
              std::make_pair(update.selector_id, update.match));
          EXPECT_EQ(removed, 1u);
          if (update.selector_id == iframe_button_id) {
            button_element_id = update.element_id;
          }
        }
        if (expected_updates[0].empty()) {
          expected_updates.erase(expected_updates.begin());
        }
        if (expected_updates.empty()) {
          // Done receiving updates.
          observer->GetElementsAndStop(
              {{SelectorObserver::SelectorId(iframe_button_id),
                /* element_id */ button_element_id}},
              std::move(element_callback));
        } else {
          observer->Continue();
        }
      });

  web_controller_->ObserveSelectors(
      {{/* selector_id = */ button_id,
        /* proto = */ Selector({"#button"}).proto,
        /* strict = */ true},
       {/* selector_id = */ iframe_button_id,
        /* proto = */ Selector({"#iframe", "#button"}).proto,
        /* strict = */ true},
       {/* selector_id = */ dynamic_id,
        /* proto = */
        Selector({"#iframeExternal", ".dynamic.about-2-seconds"}).proto,
        /* strict = */ true}},
      SelectorObserverDefaultSettings(base::Seconds(30)), update_callback);

  run_loop.Run();
  ASSERT_TRUE(expected_updates.empty());
}

IN_PROC_BROWSER_TEST_F(BatchElementCheckerBrowserTest,
                       SelectorObserverRedirectIframe) {
  base::RunLoop run_loop;
  bool received_no_match_update = false;
  const SelectorObserver::SelectorId button_id(15);

  auto element_callback = base::BindLambdaForTesting(
      [&](const ClientStatus& status,
          const base::flat_map<SelectorObserver::SelectorId,
                               DomObjectFrameStack>& elements) {
        EXPECT_TRUE(status.ok());
        run_loop.Quit();
      });

  SelectorObserver::Callback update_callback = base::BindLambdaForTesting(
      [&](const ClientStatus& status,
          const std::vector<SelectorObserver::Update>& updates,
          SelectorObserver* observer) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(updates.size(), 1u);
        EXPECT_EQ(updates[0].selector_id, button_id);
        if (updates[0].match) {
          EXPECT_TRUE(received_no_match_update);
          observer->GetElementsAndStop({}, std::move(element_callback));
        } else {
          received_no_match_update = true;
          observer->Continue();
        }
      });

  web_controller_->ObserveSelectors(
      {{/* selector_id = */ button_id,
        /* proto = */ Selector({"#iframeRedirecting", "#button"}).proto,
        /* strict = */ true}},
      SelectorObserverDefaultSettings(base::Seconds(30)), update_callback);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BatchElementCheckerBrowserTest,
                       SelectorObserverTimeout) {
  base::RunLoop run_loop;

  base::MockCallback<base::RepeatingCallback<void(
      const ClientStatus& status,
      const std::vector<SelectorObserver::Update>& updates,
      SelectorObserver* observer)>>
      mock_callback;
  EXPECT_CALL(mock_callback, Run)
      .WillOnce([](const ClientStatus& status,
                   const std::vector<SelectorObserver::Update>& updates,
                   SelectorObserver* observer) {
        // First call informs of the initial state of the selectors.
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(updates.size(), 1u);
        observer->Continue();
      })
      .WillOnce([&](const ClientStatus& status,
                    const std::vector<SelectorObserver::Update>& updates,
                    SelectorObserver* observer) {
        // Second call when it timeouts.
        EXPECT_EQ(status.proto_status(), ELEMENT_RESOLUTION_FAILED);
        EXPECT_EQ(updates.size(), 0u);
        run_loop.Quit();
      });

  web_controller_->ObserveSelectors(
      {{/* selector_id = */ SelectorObserver::SelectorId(1),
        /* proto = */ Selector({"#does_not_exist"}).proto,
        /* strict = */ true}},
      SelectorObserverDefaultSettings(base::Milliseconds(300)),
      mock_callback.Get());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BatchElementCheckerBrowserTest,
                       SelectorObserverShortMaxWaitTime) {
  base::RunLoop run_loop;
  const SelectorObserver::SelectorId button_id(15);
  base::MockCallback<base::OnceCallback<void(
      const ClientStatus& status,
      const base::flat_map<SelectorObserver::SelectorId, DomObjectFrameStack>&
          elements)>>
      element_callback;
  EXPECT_CALL(element_callback, Run)
      .WillOnce([&](const ClientStatus& status,
                    const base::flat_map<SelectorObserver::SelectorId,
                                         DomObjectFrameStack>& elements) {
        EXPECT_TRUE(status.ok());
        run_loop.Quit();
      });

  base::MockCallback<base::RepeatingCallback<void(
      const ClientStatus& status,
      const std::vector<SelectorObserver::Update>& updates,
      SelectorObserver* observer)>>
      update_callback;
  EXPECT_CALL(update_callback, Run)
      .WillOnce([&](const ClientStatus& status,
                    const std::vector<SelectorObserver::Update>& updates,
                    SelectorObserver* observer) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(updates.size(), 1u);
        EXPECT_EQ(updates[0].selector_id, button_id);
        EXPECT_TRUE(updates[0].match);
        observer->GetElementsAndStop({}, element_callback.Get());
      });

  web_controller_->ObserveSelectors(
      {{/* selector_id = */ button_id,
        /* proto = */ Selector({"#iframe", "#button"}).proto,
        /* strict = */ true}},
      SelectorObserverDefaultSettings(base::Milliseconds(1)),
      update_callback.Get());

  run_loop.Run();
}

}  // namespace autofill_assistant
