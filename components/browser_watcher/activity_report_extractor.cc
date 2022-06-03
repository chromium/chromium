// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/activity_report_extractor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/debug/activity_analyzer.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/browser_watcher/activity_data_names.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"

namespace browser_watcher {

using ActivitySnapshot = base::debug::ThreadActivityAnalyzer::Snapshot;
using base::PersistentMemoryAllocator;
using base::debug::ActivityUserData;
using base::debug::GlobalActivityAnalyzer;
using base::debug::GlobalActivityTracker;
using base::debug::ThreadActivityAnalyzer;

namespace {

// Collects stability user data from the recorded format to the collected
// format.
void CollectUserData(
    const ActivityUserData::Snapshot& recorded_map,
    google::protobuf::Map<std::string, TypedValue>* collected_map,
    StabilityReport* report) {
  DCHECK(collected_map);

  for (const auto& name_and_value : recorded_map) {
    const std::string& key = name_and_value.first;
    const ActivityUserData::TypedValue& recorded_value = name_and_value.second;
    TypedValue collected_value;

    switch (recorded_value.type()) {
      case ActivityUserData::END_OF_VALUES:
        NOTREACHED();
        break;
      case ActivityUserData::RAW_VALUE: {
        base::StringPiece raw = recorded_value.Get();
        collected_value.set_bytes_value(raw.data(), raw.size());
        break;
      }
      case ActivityUserData::RAW_VALUE_REFERENCE: {
        base::StringPiece recorded_ref = recorded_value.GetReference();
        TypedValue::Reference* collected_ref =
            collected_value.mutable_bytes_reference();
        collected_ref->set_address(
            reinterpret_cast<uintptr_t>(recorded_ref.data()));
        collected_ref->set_size(recorded_ref.size());
        break;
      }
      case ActivityUserData::STRING_VALUE: {
        base::StringPiece value = recorded_value.GetString();
        collected_value.set_string_value(value.data(), value.size());

        // Promote version information to the global key value store.
        if (report) {
          bool should_promote =
              key == kActivityProduct || key == kActivityChannel ||
              key == kActivityPlatform || key == kActivityVersion;
          if (should_promote) {
            // TODO(siggi): Copy the promoted data or perhaps remove this
            //     promotion once the backend is able to display the per-process
            //     data.
            (*report->mutable_global_data())[key].Swap(&collected_value);
            continue;
          }
        }

        break;
      }
      case ActivityUserData::STRING_VALUE_REFERENCE: {
        base::StringPiece recorded_ref = recorded_value.GetStringReference();
        TypedValue::Reference* collected_ref =
            collected_value.mutable_string_reference();
        collected_ref->set_address(
            reinterpret_cast<uintptr_t>(recorded_ref.data()));
        collected_ref->set_size(recorded_ref.size());
        break;
      }
      case ActivityUserData::CHAR_VALUE: {
        char char_value = recorded_value.GetChar();
        collected_value.set_char_value(&char_value, 1);
        break;
      }
      case ActivityUserData::BOOL_VALUE:
        collected_value.set_bool_value(recorded_value.GetBool());
        break;
      case ActivityUserData::SIGNED_VALUE:
        collected_value.set_signed_value(recorded_value.GetInt());

        // Promote the execution timestamp to the global key value store.
        if (report && key == kActivityStartTimestamp) {
          (*report->mutable_global_data())[key].Swap(&collected_value);
          continue;
        }

        break;
      case ActivityUserData::UNSIGNED_VALUE:
        collected_value.set_unsigned_value(recorded_value.GetUint());
        break;
    }

    (*collected_map)[key].Swap(&collected_value);
  }
}

void CollectModuleInformation(
    const std::vector<GlobalActivityTracker::ModuleInfo>& modules,
    ProcessState* process_state) {
  DCHECK(process_state);

  for (const GlobalActivityTracker::ModuleInfo& recorded : modules) {
    CodeModule* collected = process_state->add_modules();
    collected->set_base_address(recorded.address);
    collected->set_size(recorded.size);
    collected->set_code_file(recorded.file);

    // Compute the code identifier using the required format.
    std::string code_identifier =
        base::StringPrintf("%08X%zx", recorded.timestamp, recorded.size);
    collected->set_code_identifier(code_identifier);
    collected->set_debug_file(recorded.debug_file);

    // Compute the debug identifier using the required format.
    const crashpad::UUID* uuid =
        reinterpret_cast<const crashpad::UUID*>(recorded.identifier);
    std::string debug_identifier = base::StringPrintf(
        "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%x", uuid->data_1,
        uuid->data_2, uuid->data_3, uuid->data_4[0], uuid->data_4[1],
        uuid->data_5[0], uuid->data_5[1], uuid->data_5[2], uuid->data_5[3],
        uuid->data_5[4], uuid->data_5[5], recorded.age);
    collected->set_debug_identifier(debug_identifier);
    collected->set_is_unloaded(!recorded.is_loaded);
  }
}

void CollectActivity(const base::debug::Activity& recorded,
                     Activity* collected) {
  DCHECK(collected);

  collected->set_time(recorded.time_internal);
  collected->set_address(recorded.calling_address);
  collected->set_origin_address(recorded.origin_address);

  // TODO(manzagop): the current collection deals with specific activity types;
  // consider modifying it to handle the notion of activity categories.
  switch (recorded.activity_type) {
    case base::debug::Activity::ACT_TASK_RUN:
      collected->set_type(Activity::ACT_TASK_RUN);
      collected->set_task_sequence_id(recorded.data.task.sequence_id);
      break;
    case base::debug::Activity::ACT_LOCK_ACQUIRE:
      collected->set_type(Activity::ACT_LOCK_ACQUIRE);
      collected->set_lock_address(recorded.data.lock.lock_address);
      break;
    case base::debug::Activity::ACT_EVENT_WAIT:
      collected->set_type(Activity::ACT_EVENT_WAIT);
      collected->set_event_address(recorded.data.event.event_address);
      break;
    case base::debug::Activity::ACT_THREAD_JOIN:
      collected->set_type(Activity::ACT_THREAD_JOIN);
      collected->set_thread_id(recorded.data.thread.thread_id);
      break;
    case base::debug::Activity::ACT_PROCESS_WAIT:
      collected->set_type(Activity::ACT_PROCESS_WAIT);
      collected->set_process_id(recorded.data.process.process_id);
      break;
    case base::debug::Activity::ACT_GENERIC:
      collected->set_type(Activity::ACT_GENERIC);
      collected->set_generic_id(recorded.data.generic.id);
      collected->set_generic_data(recorded.data.generic.info);
      break;
    default:
      break;
  }
}

void CollectException(const base::debug::Activity& recorded,
                      Exception* collected) {
  DCHECK(collected);
  collected->set_code(recorded.data.exception.code);
  collected->set_program_counter(recorded.calling_address);
  collected->set_exception_address(recorded.origin_address);
  collected->set_time(recorded.time_internal);
}

void CollectThread(
    const base::debug::ThreadActivityAnalyzer::Snapshot& snapshot,
    ThreadState* thread_state) {
  DCHECK(thread_state);

  thread_state->set_thread_name(snapshot.thread_name);
  thread_state->set_thread_id(snapshot.thread_id);
  thread_state->set_activity_count(snapshot.activity_stack_depth);

  if (snapshot.last_exception.activity_type ==
      base::debug::Activity::ACT_EXCEPTION) {
    CollectException(snapshot.last_exception,
                     thread_state->mutable_exception());
  }

  for (size_t i = 0; i < snapshot.activity_stack.size(); ++i) {
    Activity* collected = thread_state->add_activities();

    CollectActivity(snapshot.activity_stack[i], collected);
    if (i < snapshot.user_data_stack.size()) {
      CollectUserData(snapshot.user_data_stack[i],
                      collected->mutable_user_data(), nullptr);
    }
  }
}

bool SetProcessType(ProcessState* process_state) {
  DCHECK(process_state);
  google::protobuf::Map<std::string, TypedValue>* process_data =
      process_state->mutable_data();

  const auto it = process_data->find(kActivityProcessType);
  if (it == process_data->end())
    return false;

  const TypedValue& value = it->second;
  if (value.value_case() != TypedValue::kSignedValue)
    return false;

  process_state->set_process_type(
      static_cast<browser_watcher::ProcessState_Type>(value.signed_value()));
  process_data->erase(it);
  return true;
}

void CollectProcess(int64_t pid,
                    GlobalActivityAnalyzer* analyzer,
                    StabilityReport* report) {
  DCHECK(analyzer);
  DCHECK(report);

  ProcessState* process_state = report->add_process_states();

  // Collect thread activity data.
  ThreadActivityAnalyzer* thread_analyzer = analyzer->GetFirstAnalyzer(pid);
  for (; thread_analyzer != nullptr;
       thread_analyzer = analyzer->GetNextAnalyzer()) {
    // Only valid analyzers are expected per contract of GetFirstAnalyzer /
    // GetNextAnalyzer.
    DCHECK(thread_analyzer->IsValid());

    if (!process_state->has_process_id()) {
      process_state->set_process_id(
          thread_analyzer->activity_snapshot().process_id);
    }
    DCHECK_EQ(thread_analyzer->activity_snapshot().process_id,
              process_state->process_id());

    ThreadState* thread_state = process_state->add_threads();
    CollectThread(thread_analyzer->activity_snapshot(), thread_state);
  }

  // Collect global user data.
  ActivityUserData::Snapshot process_data_snapshot =
      analyzer->GetProcessDataSnapshot(pid);
  CollectUserData(process_data_snapshot, process_state->mutable_data(), report);
  SetProcessType(process_state);

  // Collect module information.
  CollectModuleInformation(analyzer->GetModules(pid), process_state);
}

}  // namespace

CollectionStatus Extract(
    std::unique_ptr<GlobalActivityAnalyzer> global_analyzer,
    StabilityReport* report) {
  DCHECK(global_analyzer);
  DCHECK(report);

  report->set_is_complete(global_analyzer->IsDataComplete());

  // Collect process data.
  for (int64_t pid = global_analyzer->GetFirstProcess(); pid != 0;
       pid = global_analyzer->GetNextProcess()) {
    CollectProcess(pid, global_analyzer.get(), report);
  }

  // Collect log messages.
  std::vector<std::string> log_messages = global_analyzer->GetLogMessages();
  for (const std::string& message : log_messages) {
    report->add_log_messages(message);
  }

  return SUCCESS;
}

}  // namespace browser_watcher
