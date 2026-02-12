// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_DUMP_ACCESSIBILITY_EVENTS_VIEWS_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_DUMP_ACCESSIBILITY_EVENTS_VIEWS_BROWSERTEST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/accessibility/platform/inspect/ax_inspect_test_helper.h"
#include "ui/views/widget/widget.h"

namespace views {

// Test parameters combining platform API type and ViewsAX feature state.
struct ViewsEventTestParams {
  ui::AXApiType::Type api_type;
  bool views_ax_enabled;

  std::string ToString() const {
    return std::string(api_type) +
           (views_ax_enabled ? "_ViewsAXEnabled" : "_ViewsAXDisabled");
  }
};

// Base class for testing platform accessibility events fired by Views.
// Inspired by DumpAccessibilityEventsTest but adapted for the Views layer.
// Uses InProcessBrowserTest for proper platform event recorder integration.
class DumpAccessibilityEventsViewsTestBase
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<ViewsEventTestParams> {
 public:
  DumpAccessibilityEventsViewsTestBase();
  ~DumpAccessibilityEventsViewsTestBase() override;

  // InProcessBrowserTest:
  void SetUp() final;
  void SetUpOnMainThread() final;
  void TearDown() override;
  void TearDownOnMainThread() override;

  // Returns platform-dependent test parameters (API types × ViewsAX states).
  static std::vector<ViewsEventTestParams> EventTestPasses();

 protected:
  // Override to configure feature flags for tests. The parameters are
  // out-params: implementations should add features to enable/disable.
  virtual void ChooseFeatures(
      std::vector<base::test::FeatureRef>* enabled_features,
      std::vector<base::test::FeatureRef>* disabled_features);

  void BeginRecordingEvents();

  // Stops recording, compares events against the expectation file, and
  // reports any differences as test failures.
  void EndTestAndCompareEvents(const std::string& test_name);

  // Runs action, then compares recorded events against expectation file.
  void RunEventTest(const std::string& test_name, base::OnceClosure action);

  virtual std::unique_ptr<ui::AXEventRecorder> CreateEventRecorder();

  // Override to set up the view hierarchy. Called before BeginRecordingEvents.
  virtual void SetUpTestViews();

  // Override to target a different window for event recording. By default,
  // returns the test widget's native window. Subclasses can override to target
  // a different window (e.g., the browser window for omnibox tests).
  virtual gfx::NativeWindow GetTargetNativeWindow() const;

  // Override to return the root view for end-of-test event notification.
  // By default, returns the test widget's root view.
  virtual View* GetTargetRootView() const;

  base::FilePath GetExpectationDirectory() const;

  virtual std::vector<ui::AXPropertyFilter> DefaultFilters() const;

  Widget* widget() { return widget_.get(); }
  const ViewsEventTestParams& GetTestParams() const { return GetParam(); }
  ui::AXApiType::Type GetApiType() const { return GetParam().api_type; }
  bool IsViewsAXEnabled() const { return GetParam().views_ax_enabled; }

  void AddPropertyFilter(const std::string& filter_str,
                         ui::AXPropertyFilter::Type type);
  void AddAllowFilter(const std::string& filter_str);
  void AddDenyFilter(const std::string& filter_str);

  // Parses a multi-line string of filter directives using the same format
  // as accessibility test expectation files (parsed via AXInspectScenario).
  // Only directives matching the current platform are applied. Example:
  //   @MAC-ALLOW:AXRole
  //   @WIN-DENY:EVENT_OBJECT_LOCATIONCHANGE*
  void SetFilters(const std::string& directives);

  void SetSortEvents(bool sort_events) { sort_events_ = sort_events; }

  virtual void OnDiffFailed();

 private:
  void SetUpTestWidget();
  base::FilePath GetExpectationFilePath(const std::string& test_name) const;
  std::vector<std::string> CollectEventLogs();
  std::vector<std::string> FilterEventLogs(
      const std::vector<std::string>& event_logs) const;
  bool ValidateAgainstExpectation(const std::string& test_name,
                                  const std::vector<std::string>& actual_lines);

  std::unique_ptr<Widget> widget_;
  std::unique_ptr<ui::AXEventRecorder> event_recorder_;
  mutable ui::AXInspectTestHelper test_helper_;
  std::vector<ui::AXPropertyFilter> additional_filters_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_ax_mode_;
  bool recording_events_ = false;
  bool sort_events_ = true;
};

struct EventTestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<ViewsEventTestParams>& info) const {
    return info.param.ToString();
  }
};

// Macros for conditionally skipping tests based on ViewsAX feature state.
// These must be macros (not functions) so GTEST_SKIP() returns from the test.

// Skip the test when ViewsAX is enabled. Use when a test is not yet compatible
// with the ViewsAX feature.
#define SKIP_IF_VIEWS_AX_ENABLED()                     \
  do {                                                 \
    if (IsViewsAXEnabled()) {                          \
      GTEST_SKIP() << "Test skipped: ViewsAX enabled"; \
    }                                                  \
  } while (0)

// Skip the test when ViewsAX is disabled. Use when a test only applies to
// ViewsAX behavior.
#define SKIP_IF_VIEWS_AX_DISABLED()                     \
  do {                                                  \
    if (!IsViewsAXEnabled()) {                          \
      GTEST_SKIP() << "Test skipped: ViewsAX disabled"; \
    }                                                   \
  } while (0)

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_DUMP_ACCESSIBILITY_EVENTS_VIEWS_BROWSERTEST_BASE_H_
