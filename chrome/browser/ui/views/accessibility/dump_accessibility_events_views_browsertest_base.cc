// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window_tree_host.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/accessibility/platform/inspect/ax_event_recorder_win.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_win_uia.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace views {
class WidgetAXManager;
std::unique_ptr<ui::AXEventRecorder> CreateViewsAXEventRecorderAuraLinux(
    base::ProcessId pid,
    const ui::AXTreeSelector& selector,
    WidgetAXManager* widget_ax_manager);
void CleanupViewsAXEventRecorderAuraLinux();
}  // namespace views
#endif

#if BUILDFLAG(IS_MAC)
namespace views {
std::unique_ptr<ui::AXEventRecorder> CreateViewsAXEventRecorderMac(
    base::ProcessId pid,
    const ui::AXTreeSelector& selector,
    gfx::NativeViewAccessible root_element,
    WidgetAXManager* widget_ax_manager);
void CleanupViewsAXEventRecorderMac();
}  // namespace views
#endif

namespace views {

// --- EventRecordingSession implementation ---

EventRecordingSession::EventRecordingSession() = default;

EventRecordingSession::EventRecordingSession(
    DumpAccessibilityEventsViewsTestBase* test,
    std::string test_name)
    : test_(test), test_name_(std::move(test_name)) {}

EventRecordingSession::~EventRecordingSession() {
  if (test_ && !compared_) {
    test_->StopRecordingAndCompare(test_name_);
  }
}

EventRecordingSession::EventRecordingSession(EventRecordingSession&& other)
    : test_(other.test_),
      test_name_(std::move(other.test_name_)),
      compared_(other.compared_) {
  other.test_ = nullptr;
  other.compared_ = true;
}

EventRecordingSession& EventRecordingSession::operator=(
    EventRecordingSession&& other) {
  if (this != &other) {
    // If this session hasn't compared yet, do it now before overwriting.
    if (test_ && !compared_) {
      test_->StopRecordingAndCompare(test_name_);
    }
    test_ = other.test_;
    test_name_ = std::move(other.test_name_);
    compared_ = other.compared_;
    other.test_ = nullptr;
    other.compared_ = true;
  }
  return *this;
}

void EventRecordingSession::StopAndCompare() {
  CHECK(test_) << "Cannot StopAndCompare on an invalid session.";
  CHECK(!compared_) << "StopAndCompare already called on this session.";
  test_->StopRecordingAndCompare(test_name_);
  compared_ = true;
}

// --- DumpAccessibilityEventsViewsTestBase implementation ---

DumpAccessibilityEventsViewsTestBase::DumpAccessibilityEventsViewsTestBase()
    : test_helper_(GetParam().api_type) {}

DumpAccessibilityEventsViewsTestBase::~DumpAccessibilityEventsViewsTestBase() =
    default;

void DumpAccessibilityEventsViewsTestBase::SetUp() {
  // Each test pass may require custom feature setup.
  test_helper_.InitializeFeatureList();

  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  // We initialize ViewsAX feature based on test parameters separately from the
  // overridable ChooseFeatures function to prevent subclasses from accidentally
  // overriding it and forgetting to call the base implementation.
  if (GetParam().views_ax_enabled) {
    enabled_features.emplace_back(::features::kAccessibilityTreeForViews);
  } else {
    disabled_features.emplace_back(::features::kAccessibilityTreeForViews);
  }

  ChooseFeatures(&enabled_features, &disabled_features);

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  InProcessBrowserTest::SetUp();
}

void DumpAccessibilityEventsViewsTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  scoped_ax_mode_ = content::BrowserAccessibilityState::GetInstance()
                        ->CreateScopedModeForProcess(ui::AXMode::kNativeAPIs);

  ui::AXTreeManager::AlwaysFailFast();

  SetUpTestWidget();
  SetUpTestViews();

  // When ViewsAX is enabled, view additions and property changes during
  // SetUpTestViews() are queued as async tasks via WidgetAXManager. Flush
  // them now so BrowserAccessibilityManager has the complete initial tree
  // before the test body runs. Without this, the first SendPendingUpdate()
  // would batch initial node additions together with test-triggered changes,
  // and AXEventGenerator wouldn't detect attribute changes on newly-added
  // nodes (since there's no previous state to compare against).
  base::RunLoop().RunUntilIdle();

#if BUILDFLAG(IS_LINUX)
  // On Wayland, widget activation is asynchronous (compositor-controlled).
  // FocusManager::SetFocusedViewWithReason() defers focus changes when
  // Widget::IsActive() returns false, which means no focus events are fired.
  // Widget::IsActive() requires both platform-level (is_active_ in
  // DesktopWindowTreeHostPlatform) and aura-level (wm::IsActiveWindow)
  // activation. SimulateNativeActivate only sets the aura level, leaving
  // is_active_ false on Wayland. Directly invoking OnActivationChanged()
  // on the platform host simulates the compositor's activation response,
  // setting both levels synchronously without requiring a compositor
  // roundtrip.
  if (widget_ && widget_->IsVisible()) {
    static_cast<DesktopWindowTreeHostPlatform*>(
        widget_->GetNativeWindow()->GetHost())
        ->OnActivationChanged(true);
  }
#endif

  // Flush any remaining async events generated during setup (e.g.
  // OnActivationChanged() above queues kWindowActivated via WidgetAXManager
  // when ViewsAX is enabled). Without this, StopRecordingAndCompare()'s
  // RunUntilIdle() would flush stale setup events during recording.
  base::RunLoop().RunUntilIdle();
}

void DumpAccessibilityEventsViewsTestBase::TearDown() {
  InProcessBrowserTest::TearDown();
  scoped_feature_list_.Reset();
  test_helper_.ResetFeatureList();
}

void DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread() {
  // EventRecordingSession objects handle comparison automatically via their
  // destructor (which fires at the end of the test body, before this method).
  // This method only needs to clean up platform state.
  if (event_recorder_) {
    if (recording_events_) {
      // Safety net: if recording is still active (e.g., test exited early
      // without the session being destroyed), stop it.
      event_recorder_->StopListeningToEvents();
      recording_events_ = false;
    }
    event_recorder_.reset();
  }

#if BUILDFLAG(IS_MAC)
  CleanupViewsAXEventRecorderMac();
#elif BUILDFLAG(IS_LINUX)
  CleanupViewsAXEventRecorderAuraLinux();
#endif

  widget_.reset();

  InProcessBrowserTest::TearDownOnMainThread();
}

// static
std::vector<ViewsEventTestParams>
DumpAccessibilityEventsViewsTestBase::EventTestPasses() {
  std::vector<ViewsEventTestParams> params;
  for (ui::AXApiType::Type api_type :
       ui::AXInspectTestHelper::EventTestPasses()) {
    // Test with ViewsAX disabled (current production behavior).
    params.push_back({api_type, false});
    // Test with ViewsAX enabled (new behavior being developed).
    params.push_back({api_type, true});
  }
  return params;
}

void DumpAccessibilityEventsViewsTestBase::SetUpTestWidget() {
  widget_ = std::make_unique<views::Widget>();

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 400, 300);

  widget_->Init(std::move(params));
}

void DumpAccessibilityEventsViewsTestBase::SetUpTestViews() {
  // Default implementation does nothing. Override in derived classes.
}

gfx::NativeWindow DumpAccessibilityEventsViewsTestBase::GetTargetNativeWindow()
    const {
  return widget_ ? widget_->GetNativeWindow() : gfx::NativeWindow();
}

View* DumpAccessibilityEventsViewsTestBase::GetTargetRootView() const {
  return widget_ ? widget_->GetRootView() : nullptr;
}

void DumpAccessibilityEventsViewsTestBase::ChooseFeatures(
    std::vector<base::test::FeatureRef>* enabled_features,
    std::vector<base::test::FeatureRef>* disabled_features) {}

EventRecordingSession
DumpAccessibilityEventsViewsTestBase::BeginRecordingEvents(
    const std::string& test_name) {
  CHECK(!recording_events_) << "Already recording events.";

  // Check whether an expectation file exists for this platform.
  base::FilePath expected_file = GetExpectationFilePath(test_name);
  if (expected_file.empty()) {
    return EventRecordingSession();
  }

  // Create the event recorder if needed.
  if (!event_recorder_) {
    event_recorder_ = CreateEventRecorder();
    if (!event_recorder_) {
      return EventRecordingSession();
    }
  }

  // Configure filters and start listening. Only pass DefaultFilters() to the
  // event recorder — these control how element properties are formatted in
  // EventReceived(). The additional_filters_ (DENY/ALLOW patterns for event
  // names) are applied later in FilterEventLogs() for event-level filtering.
  // Passing event-name filters like DENY:* to the recorder's formatter would
  // suppress all element properties, producing empty element descriptions.
  event_recorder_->SetPropertyFilters(DefaultFilters());
  event_recorder_->ListenToEvents(base::DoNothing());
  recording_events_ = true;

  return EventRecordingSession(this, test_name);
}

void DumpAccessibilityEventsViewsTestBase::StopRecordingAndCompare(
    const std::string& test_name) {
  if (recording_events_) {
    // Fire the end-of-test event on the root view. This is needed for UIA
    // tests which wait for this event before collecting results.
    views::View* root_view = GetTargetRootView();
    CHECK(root_view);
    root_view->GetViewAccessibility().NotifyEvent(ax::mojom::Event::kEndOfTest,
                                                  true);

    // Flush any pending async WidgetAXManager updates so that
    // BrowserAccessibilityManager fires the corresponding platform events
    // before we stop listening.
    base::RunLoop().RunUntilIdle();

    event_recorder_->StopListeningToEvents();
    event_recorder_->WaitForDoneRecording();
    recording_events_ = false;
  }

  EXPECT_TRUE(ValidateAgainstExpectation(test_name, CollectEventLogs()));
}

std::unique_ptr<ui::AXEventRecorder>
DumpAccessibilityEventsViewsTestBase::CreateEventRecorder() {
  gfx::NativeWindow native_window = GetTargetNativeWindow();
  if (!native_window) {
    return nullptr;
  }

  ui::AXTreeSelector selector;

#if BUILDFLAG(IS_WIN)
#if defined(USE_AURA)
  // On Windows, we need to provide the native widget handle to scope event
  // recording to our test window. Mac and Linux event recorders use different
  // mechanisms (e.g., Mac uses the root accessible element directly, Linux
  // AT-SPI uses process ID filtering).
  selector.widget = reinterpret_cast<gfx::AcceleratedWidget>(
      native_window->GetHost()->GetAcceleratedWidget());
#endif

  switch (GetApiType()) {
    case ui::AXApiType::kWinIA2:
      return std::make_unique<ui::AXEventRecorderWin>(
          base::GetCurrentProcId(), selector, ui::AXEventRecorderWin::kSync);
    case ui::AXApiType::kWinUIA:
      return std::make_unique<ui::AXEventRecorderWinUia>(selector);
    default:
      return nullptr;
  }
#elif BUILDFLAG(IS_MAC)
  if (GetApiType() != ui::AXApiType::kMac) {
    return nullptr;
  }
  gfx::NativeViewAccessible root_element =
      widget_->GetRootView()->GetNativeViewAccessible();
  return CreateViewsAXEventRecorderMac(base::GetCurrentProcId(), selector,
                                       root_element, widget_->ax_manager());
#elif BUILDFLAG(IS_LINUX)
  if (GetApiType() != ui::AXApiType::kLinux) {
    return nullptr;
  }
  return CreateViewsAXEventRecorderAuraLinux(base::GetCurrentProcId(), selector,
                                             widget_->ax_manager());
#else
  return nullptr;
#endif
}

base::FilePath DumpAccessibilityEventsViewsTestBase::GetExpectationDirectory()
    const {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  return test_data_dir.AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("accessibility")
      .AppendASCII("events");
}

std::vector<ui::AXPropertyFilter>
DumpAccessibilityEventsViewsTestBase::DefaultFilters() const {
  std::vector<ui::AXPropertyFilter> filters;

#if BUILDFLAG(IS_WIN)
  // Suppress noisy location change events.
  filters.emplace_back("EVENT_OBJECT_LOCATIONCHANGE*",
                       ui::AXPropertyFilter::DENY);
  // Suppress system events that may be noisy.
  filters.emplace_back("EVENT_SYSTEM_*", ui::AXPropertyFilter::DENY);
  // Allow show/hide and state changes by default.
  filters.emplace_back("EVENT_OBJECT_SHOW*", ui::AXPropertyFilter::ALLOW);
  filters.emplace_back("EVENT_OBJECT_HIDE*", ui::AXPropertyFilter::ALLOW);
  filters.emplace_back("EVENT_OBJECT_STATECHANGE*",
                       ui::AXPropertyFilter::ALLOW);
  filters.emplace_back("EVENT_OBJECT_FOCUS*", ui::AXPropertyFilter::ALLOW);
  filters.emplace_back("IA2_EVENT_*", ui::AXPropertyFilter::ALLOW);
#elif BUILDFLAG(IS_MAC)
  filters.emplace_back("AXFocusedUIElementChanged*",
                       ui::AXPropertyFilter::ALLOW);
  filters.emplace_back("AXSelectedChildrenChanged*",
                       ui::AXPropertyFilter::ALLOW);
  filters.emplace_back("AXValueChanged*", ui::AXPropertyFilter::ALLOW);
#elif BUILDFLAG(IS_LINUX)
  filters.emplace_back("STATE-CHANGE:*", ui::AXPropertyFilter::ALLOW);
  filters.emplace_back("FOCUS-EVENT:*", ui::AXPropertyFilter::ALLOW);
#endif

  return filters;
}

void DumpAccessibilityEventsViewsTestBase::AddPropertyFilter(
    const std::string& filter_str,
    ui::AXPropertyFilter::Type type) {
  additional_filters_.emplace_back(filter_str, type);
}

void DumpAccessibilityEventsViewsTestBase::AddAllowFilter(
    const std::string& filter_str) {
  AddPropertyFilter(filter_str, ui::AXPropertyFilter::ALLOW);
}

void DumpAccessibilityEventsViewsTestBase::AddDenyFilter(
    const std::string& filter_str) {
  AddPropertyFilter(filter_str, ui::AXPropertyFilter::DENY);
}

void DumpAccessibilityEventsViewsTestBase::SetFilters(
    const std::string& directives) {
  std::vector<std::string> lines = base::SplitString(
      directives, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ui::AXInspectScenario scenario =
      test_helper_.ParseScenario(lines, DefaultFilters());
  // Extract only the filters that were added beyond the defaults.
  for (size_t i = DefaultFilters().size(); i < scenario.property_filters.size();
       ++i) {
    additional_filters_.push_back(std::move(scenario.property_filters[i]));
  }
}

void DumpAccessibilityEventsViewsTestBase::OnDiffFailed() {}

base::FilePath DumpAccessibilityEventsViewsTestBase::GetExpectationFilePath(
    const std::string& test_name) const {
  base::FilePath expectation_dir = GetExpectationDirectory();
  base::FilePath test_file = expectation_dir.AppendASCII(test_name + ".txt");
  return test_helper_.GetExpectationFilePath(test_file);
}

std::vector<std::string>
DumpAccessibilityEventsViewsTestBase::CollectEventLogs() {
  if (!event_recorder_) {
    return {};
  }

  std::vector<std::string> event_logs = event_recorder_->GetEventLogs();

  // Apply any scenario-based filtering.
  event_logs = FilterEventLogs(event_logs);

  // Sort if configured to do so.
  if (sort_events_) {
    std::sort(event_logs.begin(), event_logs.end());
  }

  return event_logs;
}

std::vector<std::string> DumpAccessibilityEventsViewsTestBase::FilterEventLogs(
    const std::vector<std::string>& event_logs) const {
  // Build the combined filter list: default filters + any filters added via
  // AddPropertyFilter() in individual tests.
  std::vector<ui::AXPropertyFilter> filters = DefaultFilters();
  filters.insert(filters.end(), additional_filters_.begin(),
                 additional_filters_.end());

  std::vector<std::string> filtered;
  for (const auto& event_log : event_logs) {
    if (ui::AXTreeFormatter::MatchesPropertyFilters(filters, event_log, true)) {
      filtered.push_back(base::EscapeNonASCII(event_log));
    }
  }
  return filtered;
}

bool DumpAccessibilityEventsViewsTestBase::ValidateAgainstExpectation(
    const std::string& test_name,
    const std::vector<std::string>& actual_lines) {
  base::FilePath expected_file = GetExpectationFilePath(test_name);

  if (expected_file.empty()) {
    return true;
  }

  std::optional<std::vector<std::string>> expected_lines =
      ui::AXInspectTestHelper::LoadExpectationFile(expected_file);

  if (!expected_lines) {
    return true;
  }

  base::FilePath test_file =
      GetExpectationDirectory().AppendASCII(test_name + ".txt");

  bool matches = ui::AXInspectTestHelper::ValidateAgainstExpectation(
      test_file, expected_file, actual_lines, *expected_lines);

  if (!matches) {
    OnDiffFailed();
  }

  return matches;
}

}  // namespace views
