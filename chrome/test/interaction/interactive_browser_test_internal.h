// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_
#define CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
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

  static std::string DeepQueryToString(
      const WebContentsInteractionTestUtil::DeepQuery& deep_query);

 protected:
  // views::test::InteractiveViewsTestPrivate:
  gfx::NativeWindow GetNativeWindowFromElement(
      ui::TrackedElement* el) const override;
  gfx::NativeWindow GetNativeWindowFromContext(
      ui::ElementContext context) const override;

 private:
  friend InteractiveBrowserTestApi;

  // Optional WebUI code coverage tool.
  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_observer_;

  // Stores instrumented WebContents and WebUI.
  std::vector<std::unique_ptr<WebContentsInteractionTestUtil>>
      instrumented_web_contents_;
};

// Provides a template-specialized method for converting base::Value objects to
// an expected type for a testing::Matcher<T>.
template <typename T>
struct JsValueExtractor {};

template <>
struct JsValueExtractor<base::Value> {
  static base::Value Extract(base::Value v) { return v; }
};

template <>
struct JsValueExtractor<int> {
  static int Extract(base::Value v) {
    if (v.is_double()) {
      LOG(ERROR) << "Result of Js function is a double; if there is any chance "
                    "your function will return a floating-point result, please "
                    "use a double value or matcher.";
    }
    return v.GetInt();
  }
};

template <>
struct JsValueExtractor<double> {
  static double Extract(base::Value v) { return v.GetDouble(); }
};

template <>
struct JsValueExtractor<bool> {
  static bool Extract(base::Value v) { return v.GetBool(); }
};

template <>
struct JsValueExtractor<std::string> {
  static std::string Extract(base::Value v) { return v.GetString(); }
};

// Provides implementations for CheckJsResult[At]().
//
// The default implementation assumes an arbitrary value or and builds an
// equality testing::Matcher<T> to use for the check; e.g.:
//
//   const std::string kExpectedHtmlContents = "...";
//   ...
//   CheckJsResult(id, "() => document.innerHtml", kExpectedHtmlContents)
//   CheckJsResult(id, "() => getFooCount()", 3)
//
template <typename T>
struct JsResultChecker {
  using V = std::remove_cvref_t<T>;
  using M = testing::Matcher<V>;
  static ui::InteractionSequence::StepBuilder CheckJsResult(
      ui::ElementIdentifier webcontents_id,
      const std::string& function,
      T&& value) {
    return JsResultChecker<M>::CheckJsResult(webcontents_id, function,
                                             M(std::move(value)));
  }

  static ui::InteractionSequence::StepBuilder CheckJsResultAt(
      ui::ElementIdentifier webcontents_id,
      WebContentsInteractionTestUtil::DeepQuery where,
      const std::string& function,
      T&& value) {
    return JsResultChecker<M>::CheckJsResultAt(webcontents_id, where, function,
                                               M(std::move(value)));
  }
};

// This implementation allows for a javascript string to be matched against am
// inline string literal, e.g.:
//
//   CheckJsResultAt(id, {"#id"}, "el => el.innerText", "The Quick Brown Fox")
//
template <int N>
struct JsResultChecker<const char (&)[N]>
    : public JsResultChecker<std::string> {};

// This implementation allows for a javascript string ot be matched against a
// constant C-style string, e.g.:
//
//   const char* const kExpectedString = "The Quick Brown Fox";
//   ...
//   CheckJsResultAt(id, {#id}m "el => el.innerText", kExpectedString)
//
template <>
struct JsResultChecker<const char*> : public JsResultChecker<std::string> {};

// This implementation allows for a testing::Matcher of any kind to be passed
// in directly; e.g.:
//
//   CheckJsResult(id,
//                 "() => document.documentElement.clientWidth",
//                 testing::Gt(500))
//
template <template <typename...> typename M, typename T>
struct JsResultChecker<M<T>> {
  using E = JsValueExtractor<std::remove_cvref_t<T>>;

  static ui::InteractionSequence::StepBuilder CheckJsResult(
      ui::ElementIdentifier webcontents_id,
      const std::string& function,
      M<T> matcher) {
    ui::InteractionSequence::StepBuilder builder;
    builder.SetDescription(
        base::StringPrintf("CheckJsResult(\"\n%s\n\")", function.c_str()));
    builder.SetElementID(webcontents_id);
    builder.SetStartCallback(base::BindOnce(
        [](std::string function, testing::Matcher<T> matcher,
           ui::InteractionSequence* seq, ui::TrackedElement* el) {
          std::string error_msg;
          base::Value result =
              el->AsA<TrackedElementWebContents>()->owner()->Evaluate(
                  function, &error_msg);
          if (!error_msg.empty()) {
            LOG(ERROR) << "CheckJsResult() failed: " << error_msg;
            seq->FailForTesting();
          } else if (!ui::test::internal::MatchAndExplain(
                         "CheckJsResult()", matcher,
                         E::Extract(std::move(result)))) {
            seq->FailForTesting();
          }
        },
        function, testing::Matcher<T>(std::move(matcher))));
    return builder;
  }

  static ui::InteractionSequence::StepBuilder CheckJsResultAt(
      ui::ElementIdentifier webcontents_id,
      WebContentsInteractionTestUtil::DeepQuery where,
      const std::string& function,
      M<T> matcher) {
    ui::InteractionSequence::StepBuilder builder;
    builder.SetDescription(base::StringPrintf(
        "CheckJsResultAt( %s, \"\n%s\n\")",
        InteractiveBrowserTestPrivate::DeepQueryToString(where).c_str(),
        function.c_str()));
    builder.SetElementID(webcontents_id);
    builder.SetStartCallback(base::BindOnce(
        [](WebContentsInteractionTestUtil::DeepQuery where,
           std::string function, testing::Matcher<T> matcher,
           ui::InteractionSequence* seq, ui::TrackedElement* el) {
          const auto full_function = base::StringPrintf(
              R"(
              (el, err) => {
                if (err) {
                  throw err;
                }
                return (%s)(el);
              }
            )",
              function.c_str());
          std::string error_msg;
          base::Value result =
              el->AsA<TrackedElementWebContents>()->owner()->EvaluateAt(
                  where, full_function, &error_msg);
          if (!error_msg.empty()) {
            LOG(ERROR) << "CheckJsResult() failed: " << error_msg;
            seq->FailForTesting();
          } else if (!ui::test::internal::MatchAndExplain(
                         "CheckJsResultAt()", matcher,
                         E::Extract(std::move(result)))) {
            seq->FailForTesting();
          }
        },
        where, function, testing::Matcher<T>(std::move(matcher))));
    return builder;
  }
};

}  // namespace internal

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_INTERNAL_H_
