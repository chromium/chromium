// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_handler.h"

#include <map>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/linux_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/tracing/arc_graphics_jank_detector.h"
#include "chrome/browser/ash/arc/tracing/arc_system_model.h"
#include "chrome/browser/ash/arc/tracing/arc_system_stat_collector.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_model.h"
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
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

namespace {

constexpr char kLastTracingModelName[] = "last_tracing_model.json";

// Maximum interval to display in full mode.
constexpr base::TimeDelta kMaxIntervalToDisplayInFullMode = base::Seconds(5.0);

base::FilePath GetLastTracingModelPath(Profile* profile) {
  DCHECK(profile);
  return file_manager::util::GetDownloadsFolderForProfile(profile).AppendASCII(
      kLastTracingModelName);
}

std::pair<base::Value, std::string> MaybeLoadLastGraphicsModel(
    const base::FilePath& last_model_path) {
  std::string json_content;
  if (!base::ReadFileToString(last_model_path, &json_content))
    return std::make_pair(base::Value(), std::string());

  absl::optional<base::Value> model = base::JSONReader::Read(json_content);
  if (!model || !model->is_dict())
    return std::make_pair(base::Value(), "Failed to read last tracing model");

  arc::ArcTracingGraphicsModel graphics_model;
  if (!graphics_model.LoadFromValue(model->GetDict())) {
    return std::make_pair(base::Value(), "Failed to load last tracing model");
  }

  return std::make_pair(std::move(*model), "Loaded last tracing model");
}

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
    ArcGraphicsTracingMode mode,
    const std::string& title,
    const std::vector<unsigned char>& icon_png,
    base::Time timestamp,
    std::unique_ptr<arc::ArcSystemStatCollector> system_stat_collector,
    const base::TimeTicks& time_min,
    const base::TimeTicks& time_max,
    const base::FilePath& model_path) {
  DCHECK(system_stat_collector);

  if (base::FeatureList::IsEnabled(arc::kSaveRawFilesOnTracing)) {
    const base::FilePath raw_path =
        model_path.DirName().Append(model_path.BaseName().value() + "_raw");
    const base::FilePath system_path =
        model_path.DirName().Append(model_path.BaseName().value() + "_system");
    if (!base::WriteFile(base::FilePath(raw_path), data)) {
      LOG(ERROR) << "Failed to save raw trace model to " << raw_path.value();
    }
    const std::string system_raw = system_stat_collector->SerializeToJson();
    if (!base::WriteFile(base::FilePath(system_path), system_raw)) {
      LOG(ERROR) << "Failed to save system model to " << system_path.value();
    }
  }

  arc::ArcTracingModel common_model;
  const base::TimeTicks time_min_clamped =
      std::max(time_min, time_max - system_stat_collector->max_interval());
  common_model.SetMinMaxTime(
      (time_min_clamped - base::TimeTicks()).InMicroseconds(),
      (time_max - base::TimeTicks()).InMicroseconds());

  if (!common_model.Build(data)) {
    return std::make_pair(base::Value(), "Failed to process tracing data");
  }

  system_stat_collector->Flush(time_min, time_max,
                               &common_model.system_model());

  arc::ArcTracingGraphicsModel graphics_model;
  if (mode != ArcGraphicsTracingMode::kFull)
    graphics_model.set_skip_structure_validation();
  if (!graphics_model.Build(common_model)) {
    return std::make_pair(base::Value(), "Failed to build tracing model");
  }

  UpdateThreads(&graphics_model.system_model().thread_map());
  graphics_model.set_app_title(title);
  graphics_model.set_app_icon_png(icon_png);
  graphics_model.set_platform(base::GetLinuxDistro());
  graphics_model.set_timestamp(timestamp);
  base::Value::Dict model = graphics_model.Serialize();

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
    ArcGraphicsTracingMode mode,
    const std::string& json_text) {
  arc::ArcTracingGraphicsModel graphics_model;
  if (mode != ArcGraphicsTracingMode::kFull)
    graphics_model.set_skip_structure_validation();
  if (!graphics_model.LoadFromJson(json_text)) {
    return std::make_pair(base::Value(), "Failed to load tracing model");
  }

  base::Value::Dict model = graphics_model.Serialize();
  return std::make_pair(base::Value(std::move(model)),
                        "Tracing model is loaded");
}

std::string GetJavascriptDomain(ArcGraphicsTracingMode mode) {
  switch (mode) {
    case ArcGraphicsTracingMode::kFull:
      return "cr.ArcGraphicsTracing.";
    case ArcGraphicsTracingMode::kOverview:
      return "cr.ArcOverviewTracing.";
  }
}

base::trace_event::TraceConfig GetTracingConfig(ArcGraphicsTracingMode mode) {
  switch (mode) {
    case ArcGraphicsTracingMode::kFull: {
      base::trace_event::TraceConfig config(
          "-*,exo,viz,toplevel,gpu,cc,blink,disabled-by-default-android gfx,"
          "disabled-by-default-android view",
          base::trace_event::RECORD_CONTINUOUSLY);
      config.EnableSystrace();
      // By default, systracing starts pre-defined set of categories with
      // predefined set of events in each category. Limit events to what we
      // actually analyze in ArcTracingModel.
      config.EnableSystraceEvent("i915:intel_gpu_freq_change");
      config.EnableSystraceEvent("drm_msm_gpu:msm_gpu_freq_change");
      config.EnableSystraceEvent("power:cpu_idle");
      config.EnableSystraceEvent("sched:sched_wakeup");
      config.EnableSystraceEvent("sched:sched_switch");
      return config;
    }
    case ArcGraphicsTracingMode::kOverview: {
      base::trace_event::TraceConfig config(
          "-*,exo,viz,toplevel,gpu", base::trace_event::RECORD_CONTINUOUSLY);
      config.EnableSystrace();
      config.EnableSystraceEvent("i915:intel_gpu_freq_change");
      config.EnableSystraceEvent("drm_msm_gpu:msm_gpu_freq_change");
      return config;
    }
  }
}

}  // namespace

// static
base::FilePath ArcGraphicsTracingHandler::GetModelPathFromTitle(
    Profile* profile,
    const std::string& title) {
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
  return file_manager::util::GetDownloadsFolderForProfile(profile).AppendASCII(
      base::StringPrintf("overview_tracing_%s_%" PRId64 ".json",
                         normalized_name,
                         (base::Time::Now() - base::Time()).InSeconds()));
}

ArcGraphicsTracingHandler::ArcGraphicsTracingHandler(
    ArcGraphicsTracingMode mode)
    : wm_helper_(exo::WMHelper::HasInstance() ? exo::WMHelper::GetInstance()
                                              : nullptr),
      mode_(mode) {
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

  if (tracing_active_)
    StopTracing();
}

void ArcGraphicsTracingHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "ready", base::BindRepeating(&ArcGraphicsTracingHandler::HandleReady,
                                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadFromText",
      base::BindRepeating(&ArcGraphicsTracingHandler::HandleLoadFromText,
                          base::Unretained(this)));
  switch (mode_) {
    case ArcGraphicsTracingMode::kFull:
      web_ui()->RegisterMessageCallback(
          "setStopOnJank",
          base::BindRepeating(&ArcGraphicsTracingHandler::HandleSetStopOnJank,
                              base::Unretained(this)));
      break;
    case ArcGraphicsTracingMode::kOverview:
      web_ui()->RegisterMessageCallback(
          "setMaxTime",
          base::BindRepeating(&ArcGraphicsTracingHandler::HandleSetMaxTime,
                              base::Unretained(this)));
      break;
  }
}

void ArcGraphicsTracingHandler::OnWindowActivated(ActivationReason reason,
                                                  aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  // Handle ARC current active window if any.
  DiscardActiveArcWindow();

  if (!gained_active)
    return;

  active_task_id_ =
      arc::GetWindowTaskId(gained_active).value_or(arc::kNoTaskId);
  if (active_task_id_ <= 0)
    return;

  arc_active_window_ = gained_active;
  arc_active_window_->AddObserver(this);
  arc_active_window_->AddPreTargetHandler(this);

  // Limit tracing by newly activated window.
  tracing_time_min_ = TRACE_TIME_TICKS_NOW();
  if (mode_ != ArcGraphicsTracingMode::kFull)
    return;

  jank_detector_ =
      std::make_unique<arc::ArcGraphicsJankDetector>(base::BindRepeating(
          &ArcGraphicsTracingHandler::OnJankDetected, base::Unretained(this)));
  exo::Surface* const surface = exo::GetShellRootSurface(arc_active_window_);
  DCHECK(surface);
  surface->AddSurfaceObserver(this);
}

void ArcGraphicsTracingHandler::OnJankDetected(const base::Time& timestamp) {
  VLOG(1) << "Jank detected " << timestamp;
  if (tracing_active_ && stop_on_jank_)
    StopTracingAndActivate();
}

base::TimeDelta ArcGraphicsTracingHandler::GetMaxInterval() const {
  switch (mode_) {
    case ArcGraphicsTracingMode::kFull:
      return kMaxIntervalToDisplayInFullMode;
    case ArcGraphicsTracingMode::kOverview:
      return max_tracing_time_;
  }
}

void ArcGraphicsTracingHandler::OnWindowPropertyChanged(aura::Window* window,
                                                        const void* key,
                                                        intptr_t old) {
  DCHECK_EQ(arc_active_window_, window);
  if (key != aura::client::kAppIconKey)
    return;

  UpdateActiveArcWindowInfo();
}

void ArcGraphicsTracingHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(arc_active_window_, window);
  DiscardActiveArcWindow();
}

void ArcGraphicsTracingHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(arc_active_window_);
  if (event->type() != ui::ET_KEY_RELEASED || event->key_code() != ui::VKEY_G ||
      !event->IsControlDown() || !event->IsShiftDown()) {
    return;
  }
  if (tracing_active_)
    StopTracingAndActivate();
  else
    StartTracing();
}

void ArcGraphicsTracingHandler::OnSurfaceDestroying(exo::Surface* surface) {
  DiscardActiveArcWindow();
}

void ArcGraphicsTracingHandler::OnCommit(exo::Surface* surface) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  jank_detector_->OnSample();
}

void ArcGraphicsTracingHandler::UpdateActiveArcWindowInfo() {
  DCHECK(arc_active_window_);

  active_task_title_ = base::UTF16ToASCII(arc_active_window_->GetTitle());
  active_task_icon_png_.clear();

  const gfx::ImageSkia* app_icon =
      arc_active_window_->GetProperty(aura::client::kAppIconKey);
  if (app_icon) {
    gfx::PNGCodec::EncodeBGRASkBitmap(
        app_icon->GetRepresentation(1.0f).GetBitmap(),
        false /* discard_transparency */, &active_task_icon_png_);
  }
}

void ArcGraphicsTracingHandler::DiscardActiveArcWindow() {
  if (tracing_active_)
    StopTracingAndActivate();

  if (!arc_active_window_)
    return;

  exo::Surface* const surface = exo::GetShellRootSurface(arc_active_window_);
  if (surface)
    surface->RemoveSurfaceObserver(this);

  arc_active_window_->RemovePreTargetHandler(this);
  arc_active_window_->RemoveObserver(this);
  jank_detector_.reset();
  arc_active_window_ = nullptr;
}

void ArcGraphicsTracingHandler::Activate() {
  aura::Window* const window =
      web_ui()->GetWebContents()->GetTopLevelNativeWindow();
  if (!window) {
    LOG(ERROR) << "Failed to activate, no top level window.";
    return;
  }

  platform_util::ActivateWindow(window);
}

void ArcGraphicsTracingHandler::StartTracing() {
  SetStatus("Collecting samples...");

  tracing_active_ = true;
  if (jank_detector_)
    jank_detector_->Reset();
  system_stat_collector_ = std::make_unique<arc::ArcSystemStatCollector>();
  system_stat_collector_->Start(GetMaxInterval());

  // Timestamp and app information would be updated when |OnTracingStarted| is
  // called.
  timestamp_ = base::Time::Now();
  UpdateActiveArcWindowInfo();

  content::TracingController::GetInstance()->StartTracing(
      GetTracingConfig(mode_),
      base::BindOnce(&ArcGraphicsTracingHandler::OnTracingStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::StopTracing() {
  SetStatus("Building model...");

  tracing_active_ = false;
  stop_tracing_timer_.Stop();

  tracing_time_max_ = TRACE_TIME_TICKS_NOW();

  if (system_stat_collector_)
    system_stat_collector_->Stop();

  content::TracingController* const controller =
      content::TracingController::GetInstance();

  if (!controller->IsTracing())
    return;

  controller->StopTracing(content::TracingController::CreateStringEndpoint(
      base::BindOnce(&ArcGraphicsTracingHandler::OnTracingStopped,
                     weak_ptr_factory_.GetWeakPtr())));
}

void ArcGraphicsTracingHandler::StopTracingAndActivate() {
  StopTracing();
  Activate();
}

void ArcGraphicsTracingHandler::SetStatus(const std::string& status) {
  AllowJavascript();
  CallJavascriptFunction(GetJavascriptDomain(mode_) + "setStatus",
                         base::Value(status.empty() ? "Idle" : status));
}

void ArcGraphicsTracingHandler::OnTracingStarted() {
  // This is an asynchronous call and it may arrive after tracing is actually
  // stopped.
  if (!tracing_active_)
    return;

  timestamp_ = base::Time::Now();
  UpdateActiveArcWindowInfo();

  tracing_time_min_ = TRACE_TIME_TICKS_NOW();
  if (mode_ == ArcGraphicsTracingMode::kOverview) {
    stop_tracing_timer_.Start(
        FROM_HERE, system_stat_collector_->max_interval(),
        base::BindOnce(&ArcGraphicsTracingHandler::StopTracingAndActivate,
                       base::Unretained(this)));
  }
}

void ArcGraphicsTracingHandler::OnTracingStopped(
    std::unique_ptr<std::string> trace_data) {
  std::string string_data;
  string_data.swap(*trace_data);

  Profile* const profile = Profile::FromWebUI(web_ui());
  const base::FilePath model_path =
      mode_ == ArcGraphicsTracingMode::kFull
          ? GetLastTracingModelPath(profile)
          : GetModelPathFromTitle(profile, active_task_title_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BuildGraphicsModel, std::move(string_data), mode_,
                     active_task_title_, active_task_icon_png_, timestamp_,
                     std::move(system_stat_collector_), tracing_time_min_,
                     tracing_time_max_, model_path),
      base::BindOnce(&ArcGraphicsTracingHandler::OnGraphicsModelReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::OnGraphicsModelReady(
    std::pair<base::Value, std::string> result) {
  SetStatus(result.second);

  if (!result.first.is_dict())
    return;

  CallJavascriptFunction(GetJavascriptDomain(mode_) + "setModel",
                         std::move(result.first));
}

void ArcGraphicsTracingHandler::HandleReady(const base::Value::List& args) {
  if (mode_ != ArcGraphicsTracingMode::kFull)
    return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&MaybeLoadLastGraphicsModel,
                     GetLastTracingModelPath(Profile::FromWebUI(web_ui()))),
      base::BindOnce(&ArcGraphicsTracingHandler::OnGraphicsModelReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::HandleSetStopOnJank(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  DCHECK_EQ(ArcGraphicsTracingMode::kFull, mode_);
  if (!args[0].is_bool()) {
    LOG(ERROR) << "Invalid input";
    return;
  }
  stop_on_jank_ = args[0].GetBool();
}

void ArcGraphicsTracingHandler::HandleSetMaxTime(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  DCHECK_EQ(ArcGraphicsTracingMode::kOverview, mode_);

  if (!args[0].is_int()) {
    LOG(ERROR) << "Invalid input";
    return;
  }
  max_tracing_time_ = base::Seconds(args[0].GetInt());
  DCHECK_GE(max_tracing_time_, base::Seconds(1));
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
      base::BindOnce(&LoadGraphicsModel, mode_, args[0].GetString()),
      base::BindOnce(&ArcGraphicsTracingHandler::OnGraphicsModelReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
