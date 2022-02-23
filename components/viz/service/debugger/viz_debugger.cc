// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <atomic>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/viz/service/debugger/viz_debugger.h"

#if VIZ_DEBUGGER_IS_ON()

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace viz {

// Version the protocol in case we ever want or need backwards compatibility
// support.
static const int kVizDebuggerVersion = 1;

std::atomic<bool> VizDebugger::enabled_;

VizDebugger* VizDebugger::GetInstance() {
  static VizDebugger g_debugger;
  return &g_debugger;
}

VizDebugger::FilterBlock::FilterBlock(const std::string file_str,
                                      const std::string func_str,
                                      const std::string anno_str,
                                      bool is_active,
                                      bool is_enabled)
    : file(std::move(file_str)),
      func(std::move(func_str)),
      anno(std::move(anno_str)),
      active(is_active),
      enabled(is_enabled) {}

VizDebugger::FilterBlock::~FilterBlock() = default;

VizDebugger::FilterBlock::FilterBlock(const FilterBlock& other) = default;

base::DictionaryValue VizDebugger::CallSubmitCommon::GetDictionaryValue()
    const {
  base::DictionaryValue option_dict;
  option_dict.SetString("color",
                        base::StringPrintf("#%02x%02x%02x", option.color_r,
                                           option.color_g, option.color_b));
  option_dict.SetInteger("alpha", option.color_a);

  base::DictionaryValue dict;
  dict.SetInteger("drawindex", draw_index);
  dict.SetInteger("source_index", source_index);
  dict.SetKey("option", std::move(option_dict));
  return dict;
}

VizDebugger::StaticSource::StaticSource(const char* anno_name,
                                        const char* file_name,
                                        int file_line,
                                        const char* func_name)
    : anno(anno_name), file(file_name), func(func_name), line(file_line) {
  VizDebugger::GetInstance()->RegisterSource(this);
}

VizDebugger::VizDebugger()
    : gpu_thread_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DETACH_FROM_THREAD(viz_compositor_thread_checker_);
  enabled_.store(false);
}

VizDebugger::~VizDebugger() = default;

base::Value VizDebugger::FrameAsJson(const uint64_t counter,
                                     const gfx::Size& window_pix,
                                     base::TimeTicks time_ticks) {
  // TODO(petermcneeley): When we move to multithread we need to do something
  // like an atomic swap here. Currently all multithreading concerns are handled
  // by having a lock around the |json_frame_output_| object.
  common_lock_.AssertAcquired();
  submission_count_ = 0;

  base::DictionaryValue global_dict;
  global_dict.SetInteger("version", kVizDebuggerVersion);
  global_dict.SetString("frame", base::NumberToString(counter));
  global_dict.SetInteger("windowx", window_pix.width());
  global_dict.SetInteger("windowy", window_pix.height());
  global_dict.SetString(
      "time", base::NumberToString(time_ticks.since_origin().InMicroseconds()));

  base::ListValue new_sources;
  for (size_t i = last_sent_source_count_; i < sources_.size(); i++) {
    const StaticSource* each = sources_[i];
    base::DictionaryValue dict;
    dict.SetString("file", each->file);
    dict.SetInteger("line", each->line);
    dict.SetString("func", each->func);
    dict.SetString("anno", each->anno);
    dict.SetInteger("index", each->reg_index);
    new_sources.Append(std::move(dict));
  }

  // Remote connection will now have acknowledged all the new sources.
  last_sent_source_count_ = sources_.size();
  global_dict.SetKey("new_sources", std::move(new_sources));

  base::ListValue draw_calls;
  for (auto&& each : draw_rect_calls_) {
    base::DictionaryValue dict = each.GetDictionaryValue();
    {
      base::ListValue list_xy;
      list_xy.Append(each.obj_size.width());
      list_xy.Append(each.obj_size.height());
      dict.SetKey("size", std::move(list_xy));
    }
    {
      base::ListValue list_xy;
      list_xy.Append(static_cast<double>(each.pos.x()));
      list_xy.Append(static_cast<double>(each.pos.y()));
      dict.SetKey("pos", std::move(list_xy));
    }

    draw_calls.Append(std::move(dict));
  }
  global_dict.SetKey("drawcalls", std::move(draw_calls));

  base::ListValue logs;
  for (auto&& log : logs_) {
    base::DictionaryValue dict = log.GetDictionaryValue();
    dict.SetString("value", std::move(log.value));
    logs.Append(std::move(dict));
  }
  global_dict.SetKey("logs", std::move(logs));

  base::ListValue texts;
  for (auto&& text : draw_text_calls_) {
    base::DictionaryValue dict = text.GetDictionaryValue();
    {
      base::ListValue list_xy;
      list_xy.Append(static_cast<double>(text.pos.x()));
      list_xy.Append(static_cast<double>(text.pos.y()));
      dict.SetKey("pos", std::move(list_xy));
    }
    dict.SetString("text", text.text);
    texts.Append(std::move(dict));
  }
  global_dict.SetKey("text", std::move(texts));

  logs_.clear();
  draw_rect_calls_.clear();
  draw_text_calls_.clear();
  return std::move(global_dict);
}

void VizDebugger::UpdateFilters() {
  common_lock_.AssertAcquired();
  if (apply_new_filters_next_frame_) {
    cached_filters_ = new_filters_;
    for (auto&& source : sources_) {
      ApplyFilters(source);
    }
    new_filters_.clear();
    apply_new_filters_next_frame_ = false;
  }
}

void VizDebugger::CompleteFrame(uint64_t counter,
                                const gfx::Size& window_pix,
                                base::TimeTicks time_ticks) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  base::AutoLock scoped_lock(common_lock_);
  UpdateFilters();
  json_frame_output_ = FrameAsJson(counter, window_pix, time_ticks);
  gpu_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizDebugger::AddFrame, base::Unretained(this)));
}

void VizDebugger::ApplyFilters(VizDebugger::StaticSource* src) {
  // In the case of no filters we disable this source.
  src->active = false;
  src->enabled = false;
  // TODO(petermcneeley): We should probably make this string filtering more
  // optimal. However, for the most part it the cost is only paid on the
  // application of new filters.
  auto simple_match = [](const char* source_str,
                         const std::string& filter_match) {
    if (filter_match.empty() || source_str == nullptr) {
      return true;
    }
    return std::strstr(source_str, filter_match.c_str()) != nullptr;
  };

  for (const auto& filter_block : cached_filters_) {
    if (simple_match(src->file, filter_block.file) &&
        simple_match(src->func, filter_block.func) &&
        simple_match(src->anno, filter_block.anno)) {
      src->active = filter_block.active;
      src->enabled = filter_block.enabled;
    }
  }
}

void VizDebugger::RegisterSource(StaticSource* src) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  int index = sources_.size();
  src->reg_index = index;
  ApplyFilters(src);
  sources_.push_back(src);
}

void VizDebugger::Draw(const gfx::SizeF& obj_size,
                       const gfx::Vector2dF& pos,
                       const VizDebugger::StaticSource* dcs,
                       VizDebugger::DrawOption option) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  Draw(gfx::Size(obj_size.width(), obj_size.height()), pos, dcs, option);
}

void VizDebugger::Draw(const gfx::Size& obj_size,
                       const gfx::Vector2dF& pos,
                       const VizDebugger::StaticSource* dcs,
                       VizDebugger::DrawOption option) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  DrawInternal(obj_size, pos, dcs, option);
}

void VizDebugger::DrawInternal(const gfx::Size& obj_size,
                               const gfx::Vector2dF& pos,
                               const VizDebugger::StaticSource* dcs,
                               VizDebugger::DrawOption option) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  draw_rect_calls_.emplace_back(submission_count_++, dcs->reg_index, option,
                                obj_size, pos);
}

void VizDebugger::DrawText(const gfx::PointF& pos,
                           const std::string& text,
                           const VizDebugger::StaticSource* dcs,
                           VizDebugger::DrawOption option) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  DrawText(gfx::Vector2dF(pos.OffsetFromOrigin()), text, dcs, option);
}

void VizDebugger::DrawText(const gfx::Point& pos,
                           const std::string& text,
                           const VizDebugger::StaticSource* dcs,
                           VizDebugger::DrawOption option) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  DrawText(gfx::Vector2dF(pos.x(), pos.y()), text, dcs, option);
}

void VizDebugger::DrawText(const gfx::Vector2dF& pos,
                           const std::string& text,
                           const VizDebugger::StaticSource* dcs,
                           VizDebugger::DrawOption option) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  draw_text_calls_.emplace_back(submission_count_++, dcs->reg_index, option,
                                pos, text);
}

void VizDebugger::AddFrame() {
  // TODO(petermcneeley): This code has duel thread entry. One to launch the
  // task and one for the task to run. We should improve on this design in the
  // future and have a better multithreaded frame data aggregation system.
  base::AutoLock scoped_lock(common_lock_);
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  if (debug_output_.is_bound()) {
    debug_output_->LogFrame(std::move(json_frame_output_));
  }
}

void VizDebugger::FilterDebugStream(base::Value json) {
  base::AutoLock scoped_lock(common_lock_);
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  const base::Value* value = &(json);
  const base::Value* filterlist = value->FindPath("filters");

  if (!filterlist || !filterlist->is_list()) {
    LOG(ERROR) << "Missing filter list in json: " << json;
    return;
  }

  new_filters_.clear();

  for (const auto& filter : filterlist->GetListDeprecated()) {
    const base::Value* file = filter.FindPath("selector.file");
    const base::Value* func = filter.FindPath("selector.func");
    const base::Value* anno = filter.FindPath("selector.anno");
    const base::Value* active = filter.FindPath("active");
    const base::Value* enabled = filter.FindPath("enabled");

    if (!active) {
      LOG(ERROR) << "Missing filter props in json: " << json;
      return;
    }

    if ((file && !file->is_string()) || (func && !func->is_string()) ||
        (anno && !anno->is_string()) || !active->is_bool()) {
      LOG(ERROR) << "Filter props wrong type in json: " << json;
      continue;
    }

    auto check_str = [](const base::Value* filter_str) {
      return (filter_str ? filter_str->GetString() : std::string());
    };

    new_filters_.emplace_back(
        check_str(file), check_str(func), check_str(anno), active->GetBool(),
        (enabled && enabled->is_bool()) ? enabled->GetBool() : true);
  }

  apply_new_filters_next_frame_ = true;
}

void VizDebugger::StartDebugStream(
    mojo::PendingRemote<mojom::VizDebugOutput> pending_debug_output) {
  base::AutoLock scoped_lock(common_lock_);
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  debug_output_.Bind(std::move(pending_debug_output));
  debug_output_.reset_on_disconnect();
  last_sent_source_count_ = 0;

  // Reset our filters for our new connection. By default the client will send
  // along the new filters after establishing the connection.
  new_filters_.clear();
  apply_new_filters_next_frame_ = true;

  base::DictionaryValue dict;
  dict.SetString("connection", "ok");
  debug_output_->LogFrame(std::move(dict));

  enabled_.store(true);
}

void VizDebugger::StopDebugStream() {
  base::AutoLock scoped_lock(common_lock_);
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  debug_output_.reset();
  enabled_.store(false);
}

void VizDebugger::AddLogMessage(std::string log,
                                const VizDebugger::StaticSource* dcs,
                                DrawOption option) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_compositor_thread_checker_);
  logs_.emplace_back(submission_count_++, dcs->reg_index, option,
                     std::move(log));
}

}  // namespace viz

#endif  // VIZ_DEBUGGER_IS_ON()
