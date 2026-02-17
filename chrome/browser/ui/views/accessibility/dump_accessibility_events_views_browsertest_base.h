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
#include "base/memory/raw_ptr.h"
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

class EventRecordingSession;

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

  // Creates an event recording session for the given test. Checks whether an
  // expectation file exists for the current platform and API type; if not,
  // returns an invalid session (operator bool() returns false). Filters should
  // be configured before calling this.
  //
  // Prefer using the BEGIN_RECORDING_EVENTS_OR_SKIP() macro instead of calling
  // this directly.
  [[nodiscard]] EventRecordingSession BeginRecordingEvents(
      const std::string& test_name);

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
  friend class EventRecordingSession;

  void SetUpTestWidget();
  void StopRecordingAndCompare(const std::string& test_name);
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

// RAII class that manages the lifecycle of accessibility event recording.
// Created by DumpAccessibilityEventsViewsTestBase::BeginRecordingEvents().
// When destroyed, automatically stops recording and compares events against
// the expectation file (unless StopAndCompare() was already called).
class EventRecordingSession {
 public:
  // Creates an invalid (empty) session.
  EventRecordingSession();
  ~EventRecordingSession();

  EventRecordingSession(EventRecordingSession&& other);
  EventRecordingSession& operator=(EventRecordingSession&& other);

  EventRecordingSession(const EventRecordingSession&) = delete;
  EventRecordingSession& operator=(const EventRecordingSession&) = delete;

  // Returns true if the session is valid (expectations exist and recording
  // is active).
  explicit operator bool() const { return test_ != nullptr; }

  // Manually stops recording and compares events against the expectation
  // file. Must only be called once. After this, the destructor is a no-op.
  void StopAndCompare();

 private:
  friend class DumpAccessibilityEventsViewsTestBase;
  EventRecordingSession(DumpAccessibilityEventsViewsTestBase* test,
                        std::string test_name);

  raw_ptr<DumpAccessibilityEventsViewsTestBase> test_ = nullptr;
  std::string test_name_;
  bool compared_ = false;
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

// Begins event recording and skips the test if no expectation file exists
// for the current platform. The recording session variable is named
// `event_recording_session_` and is accessible in the test body. Events are
// automatically compared against expectations when the session goes out of
// scope, or earlier via event_recording_session_.StopAndCompare().
#define BEGIN_RECORDING_EVENTS_OR_SKIP(test_name)                         \
  auto event_recording_session_ = BeginRecordingEvents(test_name);        \
  do {                                                                    \
    if (!event_recording_session_) {                                      \
      GTEST_SKIP() << "No expectation file for " << (test_name) << " on " \
                   << GetTestParams().ToString();                         \
    }                                                                     \
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
