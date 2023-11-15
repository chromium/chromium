// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_handler.h"

#include <map>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/linux_util.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/tracing/arc_system_model.h"
#include "chrome/browser/ash/arc/tracing/arc_system_stat_collector.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_model.h"
#include "chrome/browser/ash/arc/tracing/present_frames_tracer.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/presentation_feedback.h"

namespace ash {

struct ArcGraphicsTracingHandler::ActiveTrace {
  arc::ArcTracingGraphicsModel model;

  // Time filter for tracing, since ARC++ window was activated last until
  // tracing is stopped.
  base::TimeTicks time_min;
  base::TimeTicks time_max;

  // Collects system stat runtime.
  arc::ArcSystemStatCollector system_stat_collector;

  // Information about active task, title and icon.
  std::string task_title;
  std::vector<unsigned char> task_icon_png;
  base::Time timestamp;

  // This must be destructed on the UI thread, so make it manually-destructable
  // with absl::optional.
  absl::optional<base::OneShotTimer> stop_timer;

  arc::PresentFramesTracer present_frames;
};

namespace {

class ProcessFilterPassAll : public base::ProcessFilter {
 public:
  ProcessFilterPassAll() = default;

  ProcessFilterPassAll(const ProcessFilterPassAll&) = delete;
  ProcessFilterPassAll& operator=(const ProcessFilterPassAll&) = delete;

  ~ProcessFilterPassAll() override = default;

  // base::ProcessFilter:
  bool Includes(const base::ProcessEntry& process) const override {
    return true;
  }
};

// Reads name of thread from /proc/pid/task/tid/status.
bool ReadNameFromStatus(pid_t pid, pid_t tid, std::string* out_name) {
  std::string status;
  if (!base::ReadFileToString(base::FilePath(base::StringPrintf(
                                  "/proc/%d/task/%d/status", pid, tid)),
                              &status)) {
    return false;
  }
  base::StringTokenizer tokenizer(status, "\n");
  while (tokenizer.GetNext()) {
    base::StringPiece value_str(tokenizer.token_piece());
    if (!base::StartsWith(value_str, "Name:"))
      continue;
    std::vector<base::StringPiece> split_value_str = base::SplitStringPiece(
        value_str, "\t", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    DCHECK_EQ(2U, split_value_str.size());
    *out_name = std::string(split_value_str[1]);
    return true;
  }

  return false;
}

// Helper that clarifies thread and process names. Tracing events may not have
// enough data for this. Also it determines the process pid the thread belongs
// to.
void UpdateThreads(arc::ArcSystemModel::ThreadMap* threads) {
  ProcessFilterPassAll filter_pass_all;
  base::ProcessIterator process_iterator(&filter_pass_all);

  std::vector<pid_t> tids;
  std::string name;
  for (const auto& process : process_iterator.Snapshot()) {
    tids.clear();
    base::GetThreadsForProcess(process.pid(), &tids);
    bool process_in_use = threads->find(process.pid()) != threads->end();
    for (pid_t tid : tids) {
      if (threads->find(tid) != threads->end()) {
        process_in_use = true;
        (*threads)[tid].pid = process.pid();
        if (!ReadNameFromStatus(process.pid(), tid, &(*threads)[tid].name))
          LOG(WARNING) << "Failed to update thread name " << tid;
      }
    }
    if (process_in_use) {
      (*threads)[process.pid()].pid = process.pid();
      if (!ReadNameFromStatus(process.pid(), process.pid(),
                              &(*threads)[process.pid()].name)) {
        LOG(WARNING) << "Failed to update process name " << process.pid();
      }
    }
  }
}

std::pair<base::Value, std::string> BuildGraphicsModel(
    const std::string& data,
    std::unique_ptr<ArcGraphicsTracingHandler::ActiveTrace> trace,
    const base::FilePath& model_path) {
  DCHECK(trace);

  if (base::FeatureList::IsEnabled(arc::kSaveRawFilesOnTracing)) {
    const base::FilePath raw_path =
        model_path.DirName().Append(model_path.BaseName().value() + "_raw");
    const base::FilePath system_path =
        model_path.DirName().Append(model_path.BaseName().value() + "_system");
    if (!base::WriteFile(base::FilePath(raw_path), data)) {
      LOG(ERROR) << "Failed to save raw trace model to " << raw_path.value();
    }
    const std::string system_raw =
        trace->system_stat_collector.SerializeToJson();
    if (!base::WriteFile(base::FilePath(system_path), system_raw)) {
      LOG(ERROR) << "Failed to save system model to " << system_path.value();
    }
  }

  arc::ArcTracingModel common_model;
  const base::TimeTicks time_min_clamped =
      std::max(trace->time_min,
               trace->time_max - trace->system_stat_collector.max_interval());
  common_model.SetMinMaxTime(
      (time_min_clamped - base::TimeTicks()).InMicroseconds(),
      (trace->time_max - base::TimeTicks()).InMicroseconds());

  if (!common_model.Build(data)) {
    return std::make_pair(base::Value(), "Failed to process tracing data");
  }

  trace->system_stat_collector.Flush(trace->time_min, trace->time_max,
                                     &common_model.system_model());

  trace->model.set_skip_structure_validation();
  if (!trace->model.Build(common_model, trace->present_frames)) {
    return std::make_pair(base::Value(), "Failed to build tracing model");
  }

  UpdateThreads(&trace->model.system_model().thread_map());
  trace->model.set_app_title(trace->task_title);
  trace->model.set_app_icon_png(trace->task_icon_png);
  trace->model.set_platform(base::GetLinuxDistro());
  trace->model.set_timestamp(trace->timestamp);
  base::Value::Dict model = trace->model.Serialize();

  std::string json_content;
  base::JSONWriter::WriteWithOptions(
      model, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_content);
  DCHECK(!json_content.empty());

  if (!base::WriteFile(model_path, json_content)) {
    LOG(ERROR) << "Failed serialize model to " << model_path.value() << ".";
  }

  return std::make_pair(base::Value(std::move(model)),
                        "Tracing model is ready");
}

std::pair<base::Value, std::string> LoadGraphicsModel(
    const std::string& json_text) {
  arc::ArcTracingGraphicsModel graphics_model;
  graphics_model.set_skip_structure_validation();
  if (!graphics_model.LoadFromJson(json_text)) {
    return std::make_pair(base::Value(), "Failed to load tracing model");
  }

  base::Value::Dict model = graphics_model.Serialize();
  return std::make_pair(base::Value(std::move(model)),
                        "Tracing model is loaded");
}

constexpr char kJavascriptDomain[] = "cr.ArcOverviewTracing.";

base::trace_event::TraceConfig GetTracingConfig() {
  base::trace_event::TraceConfig config(
      "-*," TRACE_DISABLED_BY_DEFAULT("display.framedisplayed"),
      base::trace_event::RECORD_CONTINUOUSLY);
  config.EnableSystrace();
  config.EnableSystraceEvent("i915:intel_gpu_freq_change");
  config.EnableSystraceEvent("drm_msm_gpu:msm_gpu_freq_change");
  return config;
}

}  // namespace

base::FilePath ArcGraphicsTracingHandler::GetModelPathFromTitle(
    std::string_view title) {
  constexpr size_t kMaxNameSize = 32;
  char normalized_name[kMaxNameSize];
  size_t index = 0;
  for (char c : title) {
    if (index == kMaxNameSize - 1)
      break;
    c = base::ToLowerASCII(c);
    if (c == ' ') {
      normalized_name[index++] = '_';
      continue;
    }
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
      normalized_name[index++] = c;
  }
  normalized_name[index] = 0;

  const std::string time =
      base::UnlocalizedTimeFormatWithPattern(Now(), "yyyy-MM-dd_HH-mm-ss");
  return GetDownloadsFolder().AppendASCII(base::StringPrintf(
      "overview_tracing_%s_%s.json", normalized_name, time.c_str()));
}

ArcGraphicsTracingHandler::ArcGraphicsTracingHandler()
    : wm_helper_(exo::WMHelper::HasInstance() ? exo::WMHelper::GetInstance()
                                              : nullptr) {
  DCHECK(wm_helper_);

  aura::Window* const current_active = wm_helper_->GetActiveWindow();
  if (current_active) {
    OnWindowActivated(ActivationReason::ACTIVATION_CLIENT /* not used */,
                      current_active, nullptr);
  }
  wm_helper_->AddActivationObserver(this);
}

ArcGraphicsTracingHandler::~ArcGraphicsTracingHandler() {
  wm_helper_->RemoveActivationObserver(this);
  DiscardActiveArcWindow();
}

void ArcGraphicsTracingHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loadFromText",
      base::BindRepeating(&ArcGraphicsTracingHandler::HandleLoadFromText,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setMaxTime",
      base::BindRepeating(&ArcGraphicsTracingHandler::HandleSetMaxTime,
                          base::Unretained(this)));
}

void ArcGraphicsTracingHandler::OnWindowActivated(ActivationReason reason,
                                                  aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  // Handle ARC current active window if any. This stops any ongoing trace.
  DiscardActiveArcWindow();

  if (!gained_active)
    return;

  if (!arc::GetWindowTaskId(gained_active).has_value()) {
    return;
  }

  arc_active_window_ = gained_active;
  arc_active_window_->AddObserver(this);
  arc_active_window_->AddPreTargetHandler(this);

  exo::Surface* const surface = exo::GetShellRootSurface(arc_active_window_);
  CHECK(surface);

  // We observe the _root_ window surface rather than the layer that receives
  // the buffer attachment. This is because it makes locating the surface
  // much easier. We will not have a noticeable delay in monitoring this
  // higher-level window since the wl_surface_commit calls by the client
  // percolate up a single thread's call stack. The root surface's commit is
  // at [0], and the ApplyPending...Changes calls there are committing
  // subsurfaces.
  // [0]
  // https://source.corp.google.com/h/googleplex-android/platform/superproject/base/+/rvc-arc:vendor/google_arc/libs/wayland_service/wayland_layer_container_window.cpp;l=73-79;drc=7bf4887dc1838167c63ae6c0fc514aaab9551e2a
  //
  // We do not need to observe the Attach event since that immediately precedes
  // the commit, and we don't care about the buffer ID.

  // TODO(matvore): can the root surface change after the window is created?
  surface->AddSurfaceObserver(this);
}

void ArcGraphicsTracingHandler::OnWindowPropertyChanged(aura::Window* window,
                                                        const void* key,
                                                        intptr_t old) {
  DCHECK_EQ(arc_active_window_, window);
  if (key != aura::client::kAppIconKey)
    return;

  if (active_trace_) {
    UpdateActiveArcWindowInfo();
  }
}

void ArcGraphicsTracingHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(arc_active_window_, window);
  DiscardActiveArcWindow();
}

void ArcGraphicsTracingHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(arc_active_window_);

  // Only two flags (decorators) must be on.
  constexpr int kFlags = ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN;
  // Four flags must be off (avoids future conflict, and prevents long
  // press from double-activating).
  constexpr int kMask = kFlags | ui::EF_COMMAND_DOWN | ui::EF_ALTGR_DOWN |
                        ui::EF_ALT_DOWN | ui::EF_IS_REPEAT;

  if (event->type() != ui::ET_KEY_PRESSED || event->key_code() != ui::VKEY_G ||
      (event->flags() & kMask) != kFlags) {
    return;
  }
  if (active_trace_) {
    StopTracingAndActivate();
  } else {
    StartTracing();
  }
}

void ArcGraphicsTracingHandler::OnSurfaceDestroying(exo::Surface* surface) {
  DiscardActiveArcWindow();
}

void ArcGraphicsTracingHandler::OnCommit(exo::Surface* surface) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!active_trace_) {
    return;
  }

  active_trace_->present_frames.AddCommit(SystemTicksNow());
  active_trace_->present_frames.ListenForPresent(surface);
}

void ArcGraphicsTracingHandler::UpdateActiveArcWindowInfo() {
  DCHECK(arc_active_window_);
  DCHECK(active_trace_);

  active_trace_->task_title =
      base::UTF16ToASCII(arc_active_window_->GetTitle());
  active_trace_->task_icon_png.clear();

  const gfx::ImageSkia* app_icon =
      arc_active_window_->GetProperty(aura::client::kAppIconKey);
  if (app_icon) {
    gfx::PNGCodec::EncodeBGRASkBitmap(
        app_icon->GetRepresentation(1.0f).GetBitmap(),
        false /* discard_transparency */, &active_trace_->task_icon_png);
  }
}

void ArcGraphicsTracingHandler::DiscardActiveArcWindow() {
  if (active_trace_) {
    StopTracingAndActivate();
  }

  if (!arc_active_window_)
    return;

  exo::Surface* const surface = exo::GetShellRootSurface(arc_active_window_);
  if (surface)
    surface->RemoveSurfaceObserver(this);

  arc_active_window_->RemovePreTargetHandler(this);
  arc_active_window_->RemoveObserver(this);
  arc_active_window_ = nullptr;
}

base::Time ArcGraphicsTracingHandler::Now() {
  return base::Time::Now();
}

aura::Window* ArcGraphicsTracingHandler::GetWebUIWindow() {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

void ArcGraphicsTracingHandler::StartTracingOnController(
    const base::trace_event::TraceConfig& trace_config,
    content::TracingController::StartTracingDoneCallback after_start) {
  content::TracingController::GetInstance()->StartTracing(
      trace_config, std::move(after_start));
}

void ArcGraphicsTracingHandler::StopTracingOnController(
    content::TracingController::CompletionCallback after_stop) {
  auto* const controller = content::TracingController::GetInstance();

  if (!controller->IsTracing()) {
    LOG(WARNING) << "TracingController has already stopped tracing";
    return;
  }

  controller->StopTracing(
      content::TracingController::CreateStringEndpoint(std::move(after_stop)));
}

base::FilePath ArcGraphicsTracingHandler::GetDownloadsFolder() {
  return file_manager::util::GetDownloadsFolderForProfile(
      Profile::FromWebUI(web_ui()));
}

void ArcGraphicsTracingHandler::ActivateWebUIWindow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* const window = GetWebUIWindow();
  if (!window) {
    LOG(ERROR) << "Failed to activate, no top level window.";
    return;
  }

  platform_util::ActivateWindow(window);
}

void ArcGraphicsTracingHandler::StartTracing() {
  SetStatus("Collecting samples...");

  active_trace_ = std::make_unique<ActiveTrace>();

  active_trace_->system_stat_collector.Start(max_tracing_time_);
  active_trace_->timestamp = Now();
  UpdateActiveArcWindowInfo();
  active_trace_->time_min = SystemTicksNow();

  StartTracingOnController(
      GetTracingConfig(),
      base::BindOnce(&ArcGraphicsTracingHandler::OnTracingStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::StopTracing() {
  SetStatus("Building model...");

  active_trace_->stop_timer->Stop();
  active_trace_->stop_timer.reset();

  active_trace_->time_max = SystemTicksNow();
  active_trace_->system_stat_collector.Stop();

  StopTracingOnController(
      base::BindOnce(&ArcGraphicsTracingHandler::OnTracingStopped,
                     weak_ptr_factory_.GetWeakPtr(), std::move(active_trace_)));
}

void ArcGraphicsTracingHandler::StopTracingAndActivate() {
  StopTracing();
  // If we are running in response to a window activation from within an
  // observer, activating the web UI immediately will cause a DCHECK failure.
  // Post as a UI task so we activate the web UI after the observer has
  // returned.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ArcGraphicsTracingHandler::ActivateWebUIWindow,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::SetStatus(const std::string& status) {
  AllowJavascript();
  CallJavascriptFunction(kJavascriptDomain + std::string("setStatus"),
                         base::Value(status.empty() ? "Idle" : status));
}

base::TimeTicks ArcGraphicsTracingHandler::SystemTicksNow() {
  return TRACE_TIME_TICKS_NOW();
}

void ArcGraphicsTracingHandler::OnTracingStarted() {
  // This is an asynchronous call and it may arrive after tracing is actually
  // stopped.
  if (!active_trace_) {
    return;
  }

  active_trace_->stop_timer.emplace();
  active_trace_->stop_timer->Start(
      FROM_HERE, active_trace_->system_stat_collector.max_interval(),
      base::BindOnce(&ArcGraphicsTracingHandler::StopTracingAndActivate,
                     base::Unretained(this)));
}

void ArcGraphicsTracingHandler::OnTracingStopped(
    std::unique_ptr<ActiveTrace> trace,
    std::unique_ptr<std::string> trace_data) {
  std::string string_data;
  string_data.swap(*trace_data);

  const base::FilePath model_path = GetModelPathFromTitle(trace->task_title);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BuildGraphicsModel, std::move(string_data),
                     std::move(trace), model_path),
      base::BindOnce(&ArcGraphicsTracingHandler::OnGraphicsModelReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::OnGraphicsModelReady(
    std::pair<base::Value, std::string> result) {
  SetStatus(result.second);

  if (!result.first.is_dict())
    return;

  CallJavascriptFunction(kJavascriptDomain + std::string("setModel"),
                         std::move(result.first));
}

void ArcGraphicsTracingHandler::HandleSetMaxTime(
    const base::Value::List& args) {
  if (args.size() != 1) {
    LOG(ERROR) << "Expect 1 numeric arg";
    return;
  }

  auto new_time = args[0].GetIfDouble();
  if (!new_time.has_value() || *new_time < 1.0) {
    LOG(ERROR) << "Interval too small or not a number: " << args[0];
    return;
  }

  max_tracing_time_ = base::Seconds(*new_time);
}

void ArcGraphicsTracingHandler::HandleLoadFromText(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  if (!args[0].is_string()) {
    LOG(ERROR) << "Invalid input";
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadGraphicsModel, args[0].GetString()),
      base::BindOnce(&ArcGraphicsTracingHandler::OnGraphicsModelReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
