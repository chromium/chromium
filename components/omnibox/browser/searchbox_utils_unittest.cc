// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/searchbox_utils.h"

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Return;

namespace searchbox {

class SearchboxUtilsTest : public testing::Test {
 protected:
  SearchboxUtilsTest() : controller_(&task_environment_) {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeAutocompleteController controller_;
  TestOmniboxClient client_;
};

class MockOmniboxAction : public OmniboxAction {
 public:
  explicit MockOmniboxAction(GURL url)
      : OmniboxAction(OmniboxAction::LabelStrings(), url) {}
  OmniboxActionId ActionId() const override { return OmniboxActionId::PEDAL; }
  MOCK_METHOD(void, Execute, (ExecutionContext & context), (const, override));

 private:
  ~MockOmniboxAction() override = default;
};

TEST_F(SearchboxUtilsTest, OpenMatchNormal) {
  AutocompleteMatch match(nullptr, 1000, false,
                          AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  match.destination_url = GURL("https://example.com");

  controller_.internal_result_.AppendMatches({match});

  // Set timestamps to satisfy DCHECK:
  // first_modification_timestamp <= last_time_default_match_changed_
  base::TimeTicks now = base::TimeTicks::Now();
  controller_.last_time_default_match_changed_ = now - base::Seconds(1);
  base::TimeTicks first_modification_timestamp = now - base::Seconds(2);
  base::TimeTicks searchbox_focused_timestamp = now - base::Seconds(3);
  base::TimeTicks match_selection_timestamp = now;

  EXPECT_CALL(client_, OnAutocompleteAccept(GURL("https://example.com"), _,
                                            WindowOpenDisposition::CURRENT_TAB,
                                            _, _, _, _, _, _, _, _))
      .Times(1);

  OpenMatch(&controller_, &client_, OmniboxPopupSelection(0), match,
            WindowOpenDisposition::CURRENT_TAB, searchbox_focused_timestamp,
            first_modification_timestamp, match_selection_timestamp,
            metrics::OmniboxEventProto::INVALID);
}

TEST_F(SearchboxUtilsTest, OpenMatchWithAction) {
  AutocompleteMatch match(nullptr, 1000, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  match.destination_url = GURL("https://example.com");
  scoped_refptr<MockOmniboxAction> action =
      base::MakeRefCounted<MockOmniboxAction>(GURL("chrome://settings"));
  match.actions.push_back(action);

  controller_.internal_result_.AppendMatches({match});

  // Set timestamps to satisfy DCHECK:
  // first_modification_timestamp <= last_time_default_match_changed_
  base::TimeTicks now = base::TimeTicks::Now();
  controller_.last_time_default_match_changed_ = now - base::Seconds(1);
  base::TimeTicks first_modification_timestamp = now - base::Seconds(2);
  base::TimeTicks searchbox_focused_timestamp = now - base::Seconds(3);
  base::TimeTicks match_selection_timestamp = now;

  EXPECT_CALL(client_, OnAutocompleteAccept(_, _, _, _, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*action, Execute(_)).Times(1);

  OmniboxPopupSelection selection(
      0, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION, 0);

  OpenMatch(&controller_, &client_, selection, match,
            WindowOpenDisposition::CURRENT_TAB, searchbox_focused_timestamp,
            first_modification_timestamp, match_selection_timestamp,
            metrics::OmniboxEventProto::INVALID);
}

TEST_F(SearchboxUtilsTest, ComputeOpenDispositionFromModifiers) {
  struct TestCase {
    bool shift;
    bool control;
    bool alt;
    bool command;
    WindowOpenDisposition expected_disposition;
    int expected_metric_value;
  } test_cases[] = {
      // {shift, control, alt, command, expected_disposition,
      // expected_metric_value}
      {false, false, false, false, WindowOpenDisposition::CURRENT_TAB, 0},
      {false, true, false, false, WindowOpenDisposition::CURRENT_TAB, 1},
      {false, false, true, false, WindowOpenDisposition::NEW_FOREGROUND_TAB, 2},
      {false, true, true, false, WindowOpenDisposition::NEW_FOREGROUND_TAB, 3},
      {true, false, false, true, WindowOpenDisposition::NEW_FOREGROUND_TAB, 4},
      {true, true, false, true, WindowOpenDisposition::NEW_FOREGROUND_TAB, 5},
      {true, false, true, false, WindowOpenDisposition::NEW_BACKGROUND_TAB, 6},
      {true, true, true, false, WindowOpenDisposition::NEW_BACKGROUND_TAB, 7},
      {false, false, false, true, WindowOpenDisposition::NEW_BACKGROUND_TAB, 8},
      {false, true, false, true, WindowOpenDisposition::NEW_BACKGROUND_TAB, 9},
      {true, false, false, false, WindowOpenDisposition::NEW_WINDOW, 10},
      {true, true, false, false, WindowOpenDisposition::NEW_WINDOW, 11},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    WindowOpenDisposition disposition =
        ComputeOpenDispositionFromModifiersAndLogToUma(
            test_case.shift, test_case.control, test_case.alt,
            test_case.command);

    EXPECT_EQ(disposition, test_case.expected_disposition);
    histogram_tester.ExpectUniqueSample(
        "Omnibox.OpenMatchWithKeyboardModifiers",
        test_case.expected_metric_value, 1);
  }
}

}  // namespace searchbox
