// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_
#define CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_

#include <compare>
#include <memory>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test_definitions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/interaction/interactive_views_test_internal.h"

class DevToolsAgentCoverageObserver;
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
  void DoTestSetUp() override;
  void DoTestTearDown() override;

  // Starts code coverage if the proper configuration is present.
  void MaybeStartWebUICodeCoverage();

  void AddInstrumentedWebContents(
      std::unique_ptr<WebContentsInteractionTestUtil>
          instrumented_web_contents);

  bool IsInstrumentedWebContents(ui::ElementIdentifier element_id) const;

  // Removes WebContents instrumentation; can allow for re-instrumentation using
  // the same ID later. Returns whether `to_remove` was found and removed.
  bool UninstrumentWebContents(ui::ElementIdentifier to_remove);

  static std::string DeepQueryToString(
      const WebContentsInteractionTestUtil::DeepQuery& deep_query);

 protected:
  // views::test::InteractiveViewsTestPrivate:
  gfx::NativeWindow GetNativeWindowFromElement(
      ui::TrackedElement* el) const override;
  gfx::NativeWindow GetNativeWindowFromContext(
      ui::ElementContext context) const override;
  std::string DebugDescribeContext(ui::ElementContext context) const override;
  DebugTreeNode DebugDumpElement(const ui::TrackedElement* el) const override;

 private:
  friend InteractiveBrowserTestApi;

  // Optional WebUI code coverage tool.
  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_observer_;

  // Stores instrumented WebContents and WebUI.
  std::vector<std::unique_ptr<WebContentsInteractionTestUtil>>
      instrumented_web_contents_;
};

// This class wraps a `base::Value` in a wrapper that allows copying when
// necessary via the `Clone()` method. This is so that Value objects can be
// used with gtest and gmock matchers, much of the logic of which requires copy
// operations in order to work.
class MatchableValue {
 public:
  MatchableValue() noexcept;
  MatchableValue(const base::Value& value) noexcept;  // NOLINT
  MatchableValue(base::Value&& value) noexcept;       // NOLINT
  MatchableValue(const MatchableValue& value) noexcept;
  MatchableValue(MatchableValue&&) noexcept;
  MatchableValue& operator=(const base::Value& value) noexcept;
  MatchableValue& operator=(base::Value&& value) noexcept;
  MatchableValue& operator=(const MatchableValue& value) noexcept;
  MatchableValue& operator=(MatchableValue&&) noexcept;
  ~MatchableValue();

  template <typename T>
  MatchableValue(T value) noexcept  // NOLINT
      : MatchableValue(base::Value(value)) {}

  // These enable implicit comparison between MatchableValue objects and types
  // that a base::Value can be constructed from. This is also required for a lot
  // of gtest and gmock logic to work properly.
  bool operator==(const MatchableValue& other) const;
  bool operator<(const MatchableValue& other) const;
  bool operator>(const MatchableValue& other) const;
  bool operator<=(const MatchableValue& other) const;
  bool operator>=(const MatchableValue& other) const;

  operator std::string() const;  // NOLINT

  const base::Value& value() const { return value_; }

 private:
  base::Value value_;
};

// Matcher that determines whether a particular value is truthy.
//
// Uses an `internal::MatchableValue` because much of the gtest infrastructure
// expects a value that can be copied, and `base::Value` cannot.
class IsTruthyMatcher
    : public testing::MatcherInterface<const internal::MatchableValue&> {
 public:
  IsTruthyMatcher() = default;
  IsTruthyMatcher(const IsTruthyMatcher&) = default;
  IsTruthyMatcher& operator=(const IsTruthyMatcher&) = default;
  ~IsTruthyMatcher() override = default;

  using is_gtest_matcher = void;

  bool MatchAndExplain(const internal::MatchableValue& x,
                       testing::MatchResultListener* listener) const override;
  void DescribeTo(std::ostream* os) const override;
  void DescribeNegationTo(std::ostream* os) const override;
};

extern std::ostream& operator<<(std::ostream& out, const MatchableValue& value);

// Helper class that converts am input into a matcher that can match a
// `base::Value`.
//
// The default implementation wraps a literal value or something that unwraps to
// a literal value to be matched.
template <typename M>
struct MakeValueMatcherHelper {
  static auto MakeValueMatcher(M m) {
    return testing::Matcher<base::Value>(testing::Eq(m));
  }
};

// This specialization handles things that can be directly cast/moved into a
// `testing::Matcher`, which is required since "matcher" is very duck-typed.
template <typename M>
  requires requires(M&& m) {
    testing::Matcher<MatchableValue>(std::forward<M>(m));
  }
struct MakeValueMatcherHelper<M> {
  static auto MakeValueMatcher(M&& m) {
    return testing::Matcher<MatchableValue>(std::forward<M>(m));
  }
};

// Wraps `m` in a `testing::Matcher` that will match a `base::Value`.
// Does not work for all possible inputs, but will work for most.
template <typename M>
auto MakeValueMatcher(M&& m) {
  return MakeValueMatcherHelper<M>::MakeValueMatcher(
      ui::test::internal::UnwrapArgument(std::forward<M>(m)));
}

// Wraps `m` in a `testing::Matcher` that will match a `base::Value`.
// Does not work for all possible inputs, but will work for most.
template <typename M>
auto MakeConstValueMatcher(const M& m) {
  return MakeValueMatcherHelper<M>::MakeValueMatcher(
      ui::test::internal::UnwrapArgument(m));
}

}  // namespace internal

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_
