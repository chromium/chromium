// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/perf/performance_test.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/test_switches.h"
#include "content/public/browser/tracing_controller.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "testing/perf/luci_test_result.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "components/user_manager/user_names.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

constexpr char kTraceDir[] = "trace-dir";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Watches if the wallpaper has been changed and runs a passed callback if so.
class TestWallpaperObserver : public ash::WallpaperControllerObserver {
 public:
  explicit TestWallpaperObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {
    WallpaperControllerClientImpl::Get()->AddObserver(this);
  }

  TestWallpaperObserver(const TestWallpaperObserver&) = delete;
  TestWallpaperObserver& operator=(const TestWallpaperObserver&) = delete;

  ~TestWallpaperObserver() override {
    WallpaperControllerClientImpl::Get()->RemoveObserver(this);
  }

  // ash::WallpaperControllerObserver:
  void OnWallpaperChanged() override { std::move(closure_).Run(); }

 private:
  base::OnceClosure closure_;
};

// Creates a high resolution wallpaper and sets it as the current wallpaper as
// the wallpaper affects many UI tests.
void CreateAndSetWallpaper() {
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(display_size.width(), display_size.height(),
                        /*isOpaque=*/true);
  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorGREEN);
  gfx::ImageSkia image =
      gfx::ImageSkia::CreateFromBitmap(std::move(bitmap), 1.f);

  base::RunLoop run_loop;
  TestWallpaperObserver observer(run_loop.QuitClosure());
  WallpaperControllerClientImpl::Get()->SetThirdPartyWallpaper(
      user_manager::StubAccountId(), /*file_name=*/"dummyfilename",
      ash::WALLPAPER_LAYOUT_CENTER_CROPPED, image);
  run_loop.Run();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

perf_test::LuciTestResult CreateTestResult(
    const base::FilePath& trace_file,
    const std::vector<std::string>& tbm_metrics) {
  perf_test::LuciTestResult result =
      perf_test::LuciTestResult::CreateForGTest();
  result.AddOutputArtifactFile("trace/1.json", trace_file, "application/json");
  for (auto& metric : tbm_metrics)
    result.AddTag("tbmv2", metric);

  return result;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PerformanceTest

PerformanceTest::PerformanceTest()
    : should_start_trace_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(kTraceDir)) {
  if (should_start_trace_) {
    auto* cmd = base::CommandLine::ForCurrentProcess();
    cmd->AppendSwitch(switches::kUseGpuInTests);
    cmd->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
}

PerformanceTest::~PerformanceTest() {
  DCHECK(setup_called_);
}

std::vector<std::string> PerformanceTest::GetUMAHistogramNames() const {
  return {};
}

const std::string PerformanceTest::GetTracingCategories() const {
  return std::string();
}

std::vector<std::string> PerformanceTest::GetTimelineBasedMetrics() const {
  return {};
}

void PerformanceTest::SetUpOnMainThread() {
  setup_called_ = true;
  InProcessBrowserTest::SetUpOnMainThread();
  if (!should_start_trace_)
    return;

  auto* controller = content::TracingController::GetInstance();
  base::trace_event::TraceConfig config(GetTracingCategories(),
                                        base::trace_event::RECORD_CONTINUOUSLY);
  for (const auto& histogram : GetUMAHistogramNames())
    config.EnableHistogram(histogram);

  base::RunLoop runloop;
  bool result = controller->StartTracing(config, runloop.QuitClosure());
  runloop.Run();
  CHECK(result);
}

void PerformanceTest::TearDownOnMainThread() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  if (should_start_trace_) {
    auto* controller = content::TracingController::GetInstance();
    ASSERT_TRUE(controller->IsTracing())
        << "Did you forget to call PerformanceTest::SetUpOnMainThread?";

    base::RunLoop runloop;
    base::FilePath dir = command_line->GetSwitchValuePath(kTraceDir);
    base::FilePath trace_file;
    CHECK(base::CreateTemporaryFileInDir(dir, &trace_file));
    LOG(INFO) << "Created the trace file: " << trace_file;

    auto trace_data_endpoint = content::TracingController::CreateFileEndpoint(
        trace_file, runloop.QuitClosure());
    bool result = controller->StopTracing(trace_data_endpoint);
    runloop.Run();
    CHECK(result);

    base::FilePath report_file =
        trace_file.AddExtension(FILE_PATH_LITERAL("test_result.json"));
    CreateTestResult(trace_file, GetTimelineBasedMetrics())
        .WriteToFile(report_file);
  }
  bool print = command_line->HasSwitch(switches::kPerfTestPrintUmaMeans);
  LOG_IF(INFO, print) << "=== Histogram Means ===";
  for (auto name : GetUMAHistogramNames()) {
    EXPECT_TRUE(HasHistogram(name)) << "missing histogram:" << name;
    LOG_IF(INFO, print) << name << ": " << GetHistogramMean(name);
  }
  LOG_IF(INFO, print) << "=== End Histogram Means ===";

  InProcessBrowserTest::TearDownOnMainThread();
}

float PerformanceTest::GetHistogramMean(const std::string& name) {
  auto* histogram = base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return 0;
  // Use SnapshotFinalDelta() so that it won't contain the samples before the
  // subclass invokes SnapshotDelta() during the test.
  auto samples = histogram->SnapshotFinalDelta();
  DCHECK_NE(0, samples->TotalCount());
  return static_cast<float>(samples->sum()) / samples->TotalCount();
}

bool PerformanceTest::HasHistogram(const std::string& name) {
  return !!base::StatisticsRecorder::FindHistogram(name);
}

////////////////////////////////////////////////////////////////////////////////
// UIPerformanceTest

void UIPerformanceTest::SetUpOnMainThread() {
  PerformanceTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CreateAndSetWallpaper();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

const std::string UIPerformanceTest::GetTracingCategories() const {
  return "benchmark,cc,viz,input,latency,gpu,rail,toplevel,ui,views,viz";
}

std::vector<std::string> UIPerformanceTest::GetTimelineBasedMetrics() const {
  return {"renderingMetric", "umaMetric"};
}
