// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/test/arc_task_window_builder.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class TestHandler : public ArcGraphicsTracingHandler {
 public:
  using content::WebUIMessageHandler::set_web_ui;

  void StartTracingOnControllerRespond() {
    DCHECK(after_start_);
    std::move(after_start_).Run();
  }

  void StopTracingOnControllerRespond(std::unique_ptr<std::string> trace_data) {
    DCHECK(after_stop_);
    std::move(after_stop_).Run(std::move(trace_data));
  }

  void set_downloads_folder(const base::FilePath& downloads_folder) {
    downloads_folder_ = downloads_folder;
  }

  void set_now(base::Time now) { now_ = now; }
  base::Time Now() override { return now_; }
  base::TimeTicks SystemTicksNow() override {
    return now_ - trace_time_base_ + base::TimeTicks();
  }

  void set_trace_time_base(base::Time trace_time_base) {
    trace_time_base_ = trace_time_base;
  }

 private:
  void StartTracingOnController(
      const base::trace_event::TraceConfig& trace_config,
      content::TracingController::StartTracingDoneCallback after_start)
      override {
    after_start_ = std::move(after_start);
  }

  void StopTracingOnController(
      content::TracingController::CompletionCallback after_stop) override {
    after_stop_ = std::move(after_stop);
  }

  void ActivateWebUIWindow() override {
    // TODO(matvore): See if we can make the default implementation for this
    // method run in tests.
  }

  base::FilePath GetDownloadsFolder() override { return downloads_folder_; }

  content::TracingController::StartTracingDoneCallback after_start_;
  content::TracingController::CompletionCallback after_stop_;

  base::Time trace_time_base_;
  base::FilePath downloads_folder_;
  base::Time now_;
};

class ArcGraphicsTracingHandlerTest : public ChromeAshTestBase {
 public:
  ArcGraphicsTracingHandlerTest()
      : ChromeAshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

  ~ArcGraphicsTracingHandlerTest() override = default;

  ArcGraphicsTracingHandlerTest(const ArcGraphicsTracingHandlerTest&) = delete;
  ArcGraphicsTracingHandlerTest& operator=(
      const ArcGraphicsTracingHandlerTest&) = delete;

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();
    arc_app_test_.SetUp(profile_.get());

    // WMHelper constructor sets a global instance which the Handler constructor
    // requires.
    wm_helper_ = std::make_unique<exo::WMHelper>();
    download_path_ = base::GetTempDirForTesting();
    web_ui_ = std::make_unique<content::TestWebUI>();
    handler_ = std::make_unique<TestHandler>();
    handler_->set_downloads_folder(download_path_);
    handler_->set_web_ui(web_ui_.get());

    local_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        local_pref_service_.get());
    arc::prefs::RegisterLocalStatePrefs(local_pref_service_->registry());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kEnableArcVm);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    local_pref_service_.reset();

    handler_.reset();
    web_ui_.reset();
    wm_helper_.reset();

    arc_app_test_.TearDown();

    profile_.reset();

    ChromeAshTestBase::TearDown();
  }

 protected:
  void FastForwardClockAndTaskQueue(base::TimeDelta delta) {
    handler_->set_now(handler_->Now() + delta);
    task_environment()->FastForwardBy(delta);
  }

  void SendStartStopKey() {
    ui::KeyEvent ev{ui::ET_KEY_RELEASED, ui::VKEY_G,
                    ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN};
    handler_->OnKeyEvent(&ev);
  }

  // The time relative to which trace tick timestamps are calculated. This is
  // typically when the system was booted.
  base::Time trace_time_base_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
  base::FilePath download_path_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<TestHandler> handler_;

  std::unique_ptr<TestingPrefServiceSimple> local_pref_service_;
};

TEST_F(ArcGraphicsTracingHandlerTest, ModelName) {
  base::FilePath download_path = base::FilePath::FromASCII("/mnt/downloads");
  handler_->set_downloads_folder(download_path);

  handler_->set_now(base::Time::UnixEpoch() + base::Seconds(1));
  EXPECT_EQ(download_path.AppendASCII(
                "overview_tracing_test_title_1_11644473601.json"),
            handler_->GetModelPathFromTitle("Test Title #:1"));
  EXPECT_EQ(
      download_path.AppendASCII(
          "overview_tracing_0123456789012345678901234567890_11644473601.json"),
      handler_->GetModelPathFromTitle(
          "0123456789012345678901234567890123456789"));

  handler_->set_now(base::Time::UnixEpoch() + base::Days(50));
  EXPECT_EQ(
      download_path.AppendASCII("overview_tracing_xyztitle_11648793600.json"),
      handler_->GetModelPathFromTitle("xyztitle"));

  download_path = base::FilePath::FromASCII("/var/DownloadFolder");
  handler_->set_downloads_folder(download_path);
  EXPECT_EQ(
      download_path.AppendASCII("overview_tracing_secret_app_11648793600.json"),
      handler_->GetModelPathFromTitle("Secret App"));
}

TEST_F(ArcGraphicsTracingHandlerTest, FilterSystemTraceByTimestamp) {
  handler_->set_now(base::Time::FromJavaTime(1'500'088'880'000));
  handler_->set_trace_time_base(base::Time::FromJavaTime(1'500'000'000'000));

  auto arc_widget = arc::ArcTaskWindowBuilder().BuildOwnsNativeWidget();
  arc_widget->Show();
  SendStartStopKey();
  handler_->StartTracingOnControllerRespond();

  // Fast forward past the max tracing interval.
  FastForwardClockAndTaskQueue(handler_->max_tracing_time() +
                               base::Milliseconds(500));

  // Pass results from trace controller to handler. First and last events should
  // not be in the model.
  handler_->StopTracingOnControllerRespond(std::make_unique<std::string>(
      R"(
{
    "traceEvents": [],
    "systemTraceEvents":
"          <idle>-0     [000] d..0 88879.800000: sched_wakeup: comm=foo pid=99 prio=115 target_cpu=000
          <idle>-0     [000] d..0 88882.000001: cpu_idle: state=0 cpu_id=0
          <idle>-0     [000] dn.0 88883.000002: cpu_idle: state=4294967295 cpu_id=0
          <idle>-0     [000] dnh3 88884.000003: sched_wakeup: comm=foo pid=25821 prio=115 target_cpu=000
          <idle>-0     [000] d..3 88884.500004: sched_switch: prev_comm=bar prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=baz next_pid=25891 next_prio=115
          <idle>-0     [000] d..3 88885.500004: sched_switch: prev_comm=baz prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=foo next_pid=33921 next_prio=115
"
})"));

  {
    const auto& set_status = web_ui_->call_data().back();
    ASSERT_EQ("cr.ArcOverviewTracing.setStatus", set_status->function_name());
    ASSERT_EQ("Building model...", set_status->arg1()->GetString());
  }
  web_ui_->ClearTrackedCalls();

  task_environment()->RunUntilIdle();

  {
    const auto& set_model = web_ui_->call_data().back();
    ASSERT_EQ("cr.ArcOverviewTracing.setModel", set_model->function_name());
    const auto& dict = set_model->arg1()->GetDict();
    const auto* events_by_cpu = dict.FindListByDottedPath("system.cpu");
    ASSERT_TRUE(events_by_cpu);
    // Only one CPU in log.
    ASSERT_EQ(1UL, events_by_cpu->size());

    const auto& cpu_events = (*events_by_cpu)[0].GetList();
    ASSERT_EQ(4UL, cpu_events.size()) << cpu_events;

    EXPECT_EQ(25821.0, cpu_events[2].GetList()[2].GetDouble());
    EXPECT_EQ(25891.0, cpu_events[3].GetList()[2].GetDouble());
  }
}

TEST_F(ArcGraphicsTracingHandlerTest, SwitchWindowDuringModelBuild) {
  handler_->set_now(base::Time::FromJavaTime(1'600'044'440'000));
  handler_->set_trace_time_base(base::Time::FromJavaTime(1'600'000'000'000));

  exo::Surface s;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&s)
                        .BuildOwnsNativeWidget();

  auto other_arc_widget = arc::ArcTaskWindowBuilder()
                              .SetTaskId(88)
                              .SetPackageName("net.differentapp")
                              .SetShellRootSurface(&s)
                              .BuildOwnsNativeWidget();

  arc_widget->Show();
  other_arc_widget->ShowInactive();
  SendStartStopKey();
  handler_->StartTracingOnControllerRespond();

  // Fast forward past the max tracing interval. This will stop the trace at the
  // end of the fast-forward, which is 400ms after the timeout.
  FastForwardClockAndTaskQueue(handler_->max_tracing_time() +
                               base::Milliseconds(400));

  // While model is being built, switch to the ARC window to change
  // min_tracing_time_. This sets the min trace time to 300ms after the end of
  // the trace.
  FastForwardClockAndTaskQueue(base::Milliseconds(300));
  other_arc_widget->Activate();

  // Pass results from trace controller to handler.
  handler_->StopTracingOnControllerRespond(std::make_unique<std::string>(
      "{\"traceEvents\":[],\"systemTraceEvents\":\""
      // clang-format off
      "          <idle>-0     [003] d..0 44442.000001: cpu_idle: state=0 cpu_id=3\n"
      // clang-format on
      "\"}"));

  task_environment()->RunUntilIdle();

  {
    const auto& dict = web_ui_->call_data().back()->arg1()->GetDict();
    const auto* events_by_cpu = dict.FindListByDottedPath("system.cpu");
    ASSERT_TRUE(events_by_cpu);
    ASSERT_EQ(4UL, events_by_cpu->size());

    const auto& cpu_events = (*events_by_cpu)[3].GetList();
    ASSERT_EQ(1UL, cpu_events.size()) << cpu_events;
  }
}

}  // namespace

}  // namespace ash
