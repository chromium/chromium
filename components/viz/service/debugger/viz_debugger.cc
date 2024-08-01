// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/debugger/viz_debugger.h"

#include <algorithm>
#include <atomic>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

#if VIZ_DEBUGGER_IS_ON()

#include "base/threading/platform_thread.h"
#include "base/threading/thread_id_name_manager.h"

namespace viz {

// Version the protocol in case we ever want or need backwards compatibility
// support.
static const int kVizDebuggerVersion = 1;

std::atomic<bool> VizDebugger::enabled_ = false;

VizDebugger::BufferInfo::BufferInfo() = default;
VizDebugger::BufferInfo::~BufferInfo() = default;
VizDebugger::BufferInfo::BufferInfo(const BufferInfo& a) = default;

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

base::Value::Dict VizDebugger::CallSubmitCommon::GetDictionaryValue() const {
  return base::Value::Dict()
      .Set("drawindex", draw_index)
      .Set("source_index", source_index)
      .Set("thread_id", thread_id)
      .Set("option",
           base::Value::Dict()
               .Set("color", base::StringPrintf("#%02x%02x%02x", option.color_r,
                                                option.color_g, option.color_b))
               .Set("alpha", option.color_a));
}

VizDebugger::StaticSource::StaticSource(const char* anno_name,
                                        const char* file_name,
                                        int file_line,
                                        const char* func_name)
    : anno(anno_name), file(file_name), func(func_name), line(file_line) {
  VizDebugger::GetInstance()->RegisterSource(this);
}

VizDebugger::VizDebugger()
    : gpu_thread_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  enabled_.store(false);
}

VizDebugger::~VizDebugger() = default;

void VizDebugger::SubmitBuffer(int buff_id, VizDebugger::BufferInfo&& buffer) {
  read_write_lock_.WriteLock();
  VizDebugger::Buffer buff;
  buff.id = buff_id;
  buff.buffer_info = std::move(buffer);
  buffers_.emplace_back(buff);
  read_write_lock_.WriteUnLock();
}

base::Value VizDebugger::FrameAsJson(const uint64_t counter,
                                     const gfx::Size& window_pix,
                                     base::TimeTicks time_ticks) {
  // TODO(petermcneeley): When we move to multithread we need to do something
  // like an atomic swap here. Currently all multithreading concerns are handled
  // by having a lock around the |json_frame_output_| object.
  submission_count_ = 0;

  auto global_dict =
      base::Value::Dict()
          .Set("version", kVizDebuggerVersion)
          .Set("frame", base::NumberToString(counter))
          .Set("windowx", window_pix.width())
          .Set("windowy", window_pix.height())
          .Set("time", base::NumberToString(
                           time_ticks.since_origin().InMicroseconds()));

  base::Value::List new_sources;
  for (size_t i = last_sent_source_count_; i < sources_.size(); i++) {
    const StaticSource* each = sources_[i];

    new_sources.Append(base::Value::Dict()
                           .Set("file", each->file)
                           .Set("line", each->line)
                           .Set("func", each->func)
                           .Set("anno", each->anno)
                           .Set("index", each->reg_index));
  }

  // Remote connection will now have acknowledged all the new sources.
  last_sent_source_count_ = sources_.size();
  global_dict.Set("new_sources", std::move(new_sources));

  // We take the minimum between tail index and buffer size to make sure we
  // don't go out of bounds.
  size_t const max_rect_calls_index =
      std::min(static_cast<int>(draw_calls_tail_idx_),
               static_cast<int>(draw_rect_calls_.size()));

  size_t const max_logs_index = std::min(static_cast<int>(logs_tail_idx_),
                                         static_cast<int>(logs_.size()));

  base::Value::List draw_calls;

  // Hash set to keep track of threads that have been registered already.
  base::flat_set<int> registered_threads;
  for (size_t i = 0; i < max_rect_calls_index; ++i) {
    base::Value::Dict dict = draw_rect_calls_[i].GetDictionaryValue();
    dict.Set("size", base::Value::List()
                         .Append(draw_rect_calls_[i].obj_size.width())
                         .Append(draw_rect_calls_[i].obj_size.height()));
    dict.Set("pos",
             base::Value::List()
                 .Append(static_cast<double>(draw_rect_calls_[i].pos.x()))
                 .Append(static_cast<double>(draw_rect_calls_[i].pos.y())));
    if (draw_rect_calls_[i].uv != DBG_DEFAULT_UV) {
      dict.Set(
          "uv_size",
          base::Value::List()
              .Append(static_cast<double>(draw_rect_calls_[i].uv.width()))
              .Append(static_cast<double>(draw_rect_calls_[i].uv.height())));
      dict.Set("uv_pos",
               base::Value::List()
                   .Append(static_cast<double>(draw_rect_calls_[i].uv.x()))
                   .Append(static_cast<double>(draw_rect_calls_[i].uv.y())));
    }
    dict.Set("buff_id", std::move(draw_rect_calls_[i].buff_id));
    if (!draw_rect_calls_[i].text.empty()) {
      dict.Set("text", std::move(draw_rect_calls_[i].text));
    }
    registered_threads.insert(draw_rect_calls_[i].thread_id);
    draw_calls.Append(std::move(dict));
  }

  global_dict.Set("drawcalls", std::move(draw_calls));

  base::Value::Dict buff_map;

  for (auto&& each : buffers_) {
    SkDynamicMemoryWStream stream;
    bool result = SkPngEncoder::Encode(
        &stream, each.buffer_info.bitmap.pixmap(), SkPngEncoder::Options());
    if (!result) {
      DLOG(ERROR) << "encode failed";
      continue;
    }
    sk_sp<SkData> data = stream.detachAsData();
    std::string uri =
        "data:image/png;base64," +
        base::Base64Encode(base::span<const uint8_t>(
            static_cast<const uint8_t*>(data->data()), data->size()));
    buff_map.Set(base::NumberToString(each.id), std::move(uri));
  }

  global_dict.Set("buff_map", std::move(buff_map));

  base::Value::List logs;
  for (size_t i = 0; i < max_logs_index; ++i) {
    base::Value::Dict dict = logs_[i].GetDictionaryValue();
    dict.Set("value", std::move(logs_[i].value));
    logs.Append(std::move(dict));
    registered_threads.insert(logs_[i].thread_id);
  }
  global_dict.Set("logs", std::move(logs));

  // Gather thread name:id for all active threads this frame.
  base::Value::List new_threads;
  for (auto&& thread_id : registered_threads) {
    std::string cur_thread_name =
        base::ThreadIdNameManager::GetInstance()->GetName(thread_id);
    new_threads.Append(base::Value::Dict()
                           .Set("thread_id", thread_id)
                           .Set("thread_name", cur_thread_name));
    registered_threads.insert(thread_id);
  }

  global_dict.Set("threads", std::move(new_threads));

  // Reset index counters for each buffer.
  buffers_.clear();
  draw_calls_tail_idx_ = 0;
  logs_tail_idx_ = 0;

  return base::Value(std::move(global_dict));
}

void VizDebugger::UpdateFilters() {
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
  if (!enabled_) {
    return;
  }
  read_write_lock_.WriteLock();
  UpdateFilters();
  json_frame_output_ = FrameAsJson(counter, window_pix, time_ticks);
  gpu_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizDebugger::AddFrame, base::Unretained(this)));
  read_write_lock_.WriteUnLock();
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
  read_write_lock_.WriteLock();
  int index = sources_.size();
  src->reg_index = index;
  ApplyFilters(src);
  sources_.push_back(src);
  read_write_lock_.WriteUnLock();
}

void VizDebugger::Draw(const gfx::SizeF& obj_size,
                       const gfx::Vector2dF& pos,
                       const VizDebugger::StaticSource* dcs,
                       VizDebugger::DrawOption option,
                       int* id,
                       const gfx::RectF& uv,
                       const std::string& text) {
  DrawInternal(obj_size, pos, dcs, option, id, uv, text);
}

void VizDebugger::DrawInternal(const gfx::SizeF& obj_size,
                               const gfx::Vector2dF& pos,
                               const VizDebugger::StaticSource* dcs,
                               VizDebugger::DrawOption option,
                               int* id,
                               const gfx::RectF& uv,
                               const std::string& text) {
  int local_id_buffer = -1;
  if (id != nullptr) {
    local_id_buffer = buffer_id++;
    *id = local_id_buffer;
  }

  //  Store atomic insertion index in local variable to use to insert into
  //  buffer.
  int insertion_index;

  for (;;) {
    read_write_lock_.ReadLock();
    // Get call insertion index.
    insertion_index = draw_calls_tail_idx_++;
    // If the insertion index is within bounds, insert call into buffer.
    if (static_cast<size_t>(insertion_index) < draw_rect_calls_.size()) {
      int cur_thread_id = base::PlatformThread::CurrentId();
      draw_rect_calls_[insertion_index] = DrawCall{submission_count_++,
                                                   dcs->reg_index,
                                                   cur_thread_id,
                                                   option,
                                                   obj_size,
                                                   pos,
                                                   local_id_buffer,
                                                   uv,
                                                   text};
      // Return when call insertion is successful.
      read_write_lock_.ReadUnlock();
      return;
    }
    read_write_lock_.ReadUnlock();
    // Take write lock to resize and re-adjust buffer tail index after buffer
    // overflow.
    read_write_lock_.WriteLock();
    // If tail index is over buffer size, then resizing is definitely needed.
    // Also re-adjust tail index so it's at the start of the new buffer space.
    if (static_cast<size_t>(draw_calls_tail_idx_) >= draw_rect_calls_.size()) {
      draw_calls_tail_idx_ = draw_rect_calls_.size();
      draw_rect_calls_.resize(draw_rect_calls_.size() * 2);
    }
    read_write_lock_.WriteUnLock();
  }
}

void VizDebugger::AddFrame() {
  // TODO(petermcneeley): This code has duel thread entry. One to launch the
  // task and one for the task to run. We should improve on this design in the
  // future and have a better multithreaded frame data aggregation system.
  read_write_lock_.WriteLock();
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  if (debug_output_.is_bound()) {
    debug_output_->LogFrame(std::move(json_frame_output_));
  }
  read_write_lock_.WriteUnLock();
}

void VizDebugger::FilterDebugStream(base::Value::Dict json) {
  read_write_lock_.WriteLock();
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  const base::Value::List* filters = json.FindList("filters");

  if (!filters) {
    LOG(ERROR) << "Missing filter list in json: " << json;
    return;
  }

  new_filters_.clear();

  for (const auto& entry : *filters) {
    const auto& filter = entry.GetDict();
    const base::Value* file = filter.FindByDottedPath("selector.file");
    const base::Value* func = filter.FindByDottedPath("selector.func");
    const base::Value* anno = filter.FindByDottedPath("selector.anno");
    const base::Value* active = filter.Find("active");

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

    std::optional<bool> enabled = filter.FindBool("enabled");
    new_filters_.emplace_back(check_str(file), check_str(func), check_str(anno),
                              active->GetBool(), enabled.value_or(true));
  }

  apply_new_filters_next_frame_ = true;
  read_write_lock_.WriteUnLock();
}

void VizDebugger::StartDebugStream(
    mojo::PendingRemote<mojom::VizDebugOutput> pending_debug_output) {
  read_write_lock_.WriteLock();
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  debug_output_.Bind(std::move(pending_debug_output));
  debug_output_.reset_on_disconnect();
  last_sent_source_count_ = 0;

  // Reset our filters for our new connection. By default the client will send
  // along the new filters after establishing the connection.
  new_filters_.clear();
  apply_new_filters_next_frame_ = true;

  debug_output_->LogFrame(
      base::Value(base::Value::Dict().Set("connection", "ok")));
  enabled_.store(true);
  read_write_lock_.WriteUnLock();
}

void VizDebugger::StopDebugStream() {
  read_write_lock_.WriteLock();
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  debug_output_.reset();
  enabled_.store(false);
  read_write_lock_.WriteUnLock();
}

void VizDebugger::AddLogMessage(std::string log,
                                const VizDebugger::StaticSource* dcs,
                                DrawOption option) {
  //  Store atomic insertion index in local variable to use to insert into
  //  buffer.
  int insertion_index;

  for (;;) {
    read_write_lock_.ReadLock();
    // Get call insertion index.
    insertion_index = logs_tail_idx_++;
    // If the insertion index is within bounds, insert call into buffer.
    if (static_cast<size_t>(insertion_index) < logs_.size()) {
      int cur_thread_id = base::PlatformThread::CurrentId();
      logs_[insertion_index] = LogCall{submission_count_++, dcs->reg_index,
                                       cur_thread_id, option, std::move(log)};
      // Return when call insertion is successful.
      read_write_lock_.ReadUnlock();
      return;
    }
    read_write_lock_.ReadUnlock();
    // Take write lock to resize and re-adjust buffer tail index after buffer
    // overflow.
    read_write_lock_.WriteLock();
    // If tail index is over buffer size, then resizing is definitely needed.
    // Also re-adjust tail index so it's at the start of the new buffer space.
    if (static_cast<size_t>(logs_tail_idx_) >= logs_.size()) {
      logs_tail_idx_ = logs_.size();
      logs_.resize(logs_.size() * 2);
    }
    read_write_lock_.WriteUnLock();
  }
}

}  // namespace viz
#else  // !VIZ_DEBUGGER_IS_ON()
namespace viz {
VizDebugger::BufferInfo::BufferInfo() = default;
VizDebugger::BufferInfo::~BufferInfo() = default;
VizDebugger::BufferInfo::BufferInfo(const BufferInfo& a) = default;

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
DrawRectToTraceValue(const gfx::Vector2dF& pos,
                     const gfx::SizeF& size,
                     const std::string& text) {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->SetString("pos_x", base::NumberToString(pos.x()));
  state->SetString("pos_y", base::NumberToString(pos.y()));
  state->SetString("size_x", base::NumberToString(size.width()));
  state->SetString("size_y", base::NumberToString(size.height()));
  state->SetString("text", text);
  return state;
}

}  // namespace viz
#endif
