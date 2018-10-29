// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/test_driver.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/heap_profiler_event_filter.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/heap_profiling/supervisor.h"
#include "components/services/heap_profiling/public/cpp/allocator_shim.h"
#include "components/services/heap_profiling/public/cpp/controller.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/service_manager_connection.h"

namespace heap_profiling {

namespace {

const char kTestCategory[] = "kTestCategory";
const char kMallocEvent[] = "kMallocEvent";
const char kMallocTypeTag[] = "kMallocTypeTag";
const char kPAEvent[] = "kPAEvent";
const char kVariadicEvent[] = "kVariadicEvent";
const char kThreadName[] = "kThreadName";

// Note: When we test sampling with |sample_everything| = true, we set the
// sampling interval to 2. It's important that all allocations made in this file
// have size >> 2, so that the probability that they are sampled is
// exponentially close to 1.
//
// Make some specific allocations in Browser to do a deeper test of the
// allocation tracking.
constexpr int kMallocAllocSize = 7907;
constexpr int kMallocAllocCount = 157;

constexpr int kVariadicAllocCount = 157;

// The sample rate should not affect the sampled allocations. Intentionally
// choose an odd number.
constexpr int kSampleRate = 7777;
constexpr int kSamplingAllocSize = 100;
constexpr int kSamplingAllocCount = 10000;
const char kSamplingAllocTypeName[] = "kSamplingAllocTypeName";

// Test fixed-size partition alloc. The size must be aligned to system pointer
// size.
constexpr int kPartitionAllocSize = 8 * 23;
constexpr int kPartitionAllocCount = 107;
static const char* kPartitionAllocTypeName = "kPartitionAllocTypeName";

// Ideally, we'd check to see that at least one renderer exists, and all
// renderers are being profiled, but something odd seems to be happening with
// warm-up/spare renderers.
//
// Whether at least 1 renderer exists, and at least 1 renderer is being
// profiled.
bool RenderersAreBeingProfiled(
    const std::vector<base::ProcessId>& profiled_pids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->GetProcess().Handle() ==
        base::kNullProcessHandle)
      continue;
    base::ProcessId pid = iter.GetCurrentValue()->GetProcess().Pid();
    if (base::ContainsValue(profiled_pids, pid)) {
      return true;
    }
  }

  return false;
}

// On success, populates |pid|.
int NumProcessesWithName(base::Value* dump_json,
                         std::string name,
                         std::vector<int>* pids) {
  int num_processes = 0;
  base::Value* events = dump_json->FindKey("traceEvents");
  for (const base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() != "process_name")
      continue;
    const base::Value* found_args =
        event.FindKeyOfType("args", base::Value::Type::DICTIONARY);
    if (!found_args)
      continue;
    const base::Value* found_process_name =
        found_args->FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_process_name)
      continue;
    if (found_process_name->GetString() != name)
      continue;

    if (pids) {
      const base::Value* found_pid =
          event.FindKeyOfType("pid", base::Value::Type::INTEGER);
      if (!found_pid) {
        LOG(ERROR) << "Process missing pid.";
        return 0;
      }
      pids->push_back(found_pid->GetInt());
    }

    ++num_processes;
  }
  return num_processes;
}

base::Value* FindArgDump(base::ProcessId pid,
                         base::Value* dump_json,
                         const char* arg) {
  base::Value* events = dump_json->FindKey("traceEvents");
  base::Value* dumps = nullptr;
  base::Value* heaps_v2 = nullptr;
  for (base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() != "periodic_interval")
      continue;
    const base::Value* found_pid =
        event.FindKeyOfType("pid", base::Value::Type::INTEGER);
    if (!found_pid)
      continue;
    if (static_cast<base::ProcessId>(found_pid->GetInt()) != pid)
      continue;
    dumps = &event;
    heaps_v2 = dumps->FindPath({"args", "dumps", arg});
    if (heaps_v2)
      return heaps_v2;
  }
  return nullptr;
}

constexpr uint64_t kNullParent = std::numeric_limits<int>::max();
struct Node {
  int name_id;
  std::string name;
  int parent_id = kNullParent;
};
using NodeMap = std::unordered_map<uint64_t, Node>;

// Parses maps.nodes and maps.strings. Returns |true| on success.
bool ParseNodes(base::Value* heaps_v2, NodeMap* output) {
  base::Value* nodes = heaps_v2->FindPath({"maps", "nodes"});
  for (const base::Value& node_value : nodes->GetList()) {
    const base::Value* id = node_value.FindKey("id");
    const base::Value* name_sid = node_value.FindKey("name_sid");
    if (!id || !name_sid) {
      LOG(ERROR) << "Node missing id or name_sid field";
      return false;
    }

    Node node;
    node.name_id = name_sid->GetInt();

    const base::Value* parent_id = node_value.FindKey("parent");
    if (parent_id) {
      node.parent_id = parent_id->GetInt();
    }

    (*output)[id->GetInt()] = node;
  }

  base::Value* strings = heaps_v2->FindPath({"maps", "strings"});
  for (const base::Value& string_value : strings->GetList()) {
    const base::Value* id = string_value.FindKey("id");
    const base::Value* string = string_value.FindKey("string");
    if (!id || !string) {
      LOG(ERROR) << "String struct missing id or string field";
      return false;
    }
    for (auto& pair : *output) {
      if (pair.second.name_id == id->GetInt()) {
        pair.second.name = string->GetString();
        break;
      }
    }
  }

  return true;
}

// Parses maps.types and maps.strings. Returns |true| on success.
bool ParseTypes(base::Value* heaps_v2, NodeMap* output) {
  base::Value* types = heaps_v2->FindPath({"maps", "types"});
  for (const base::Value& type_value : types->GetList()) {
    const base::Value* id = type_value.FindKey("id");
    const base::Value* name_sid = type_value.FindKey("name_sid");
    if (!id || !name_sid) {
      LOG(ERROR) << "Node missing id or name_sid field";
      return false;
    }

    Node node;
    node.name_id = name_sid->GetInt();
    (*output)[id->GetInt()] = node;
  }

  base::Value* strings = heaps_v2->FindPath({"maps", "strings"});
  for (const base::Value& string_value : strings->GetList()) {
    const base::Value* id = string_value.FindKey("id");
    const base::Value* string = string_value.FindKey("string");
    if (!id || !string) {
      LOG(ERROR) << "String struct missing id or string field";
      return false;
    }
    for (auto& pair : *output) {
      if (pair.second.name_id == id->GetInt()) {
        pair.second.name = string->GetString();
        break;
      }
    }
  }

  return true;
}

// Verify expectations are present in heap dump.
bool ValidateDump(base::Value* heaps_v2,
                  int expected_alloc_size,
                  int expected_alloc_count,
                  const char* allocator_name,
                  const char* type_name,
                  const std::string& frame_name,
                  const std::string& thread_name) {
  base::Value* sizes =
      heaps_v2->FindPath({"allocators", allocator_name, "sizes"});
  if (!sizes) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name
               << ".sizes' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& sizes_list = sizes->GetList();
  if (sizes_list.empty()) {
    LOG(ERROR) << "'allocators." << allocator_name
               << ".sizes' is an empty list";
    return false;
  }

  base::Value* counts =
      heaps_v2->FindPath({"allocators", allocator_name, "counts"});
  if (!counts) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name
               << ".counts' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& counts_list = counts->GetList();
  if (sizes_list.size() != counts_list.size()) {
    LOG(ERROR)
        << "'allocators." << allocator_name
        << ".sizes' does not have the same number of elements as *.counts";
    return false;
  }

  base::Value* types =
      heaps_v2->FindPath({"allocators", allocator_name, "types"});
  if (!types) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name
               << ".types' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& types_list = types->GetList();
  if (types_list.empty()) {
    LOG(ERROR) << "'allocators." << allocator_name
               << ".types' is an empty list";
    return false;
  }

  if (sizes_list.size() != types_list.size()) {
    LOG(ERROR)
        << "'allocators." << allocator_name
        << ".types' does not have the same number of elements as *.sizes";
    return false;
  }

  base::Value* nodes =
      heaps_v2->FindPath({"allocators", allocator_name, "nodes"});
  if (!nodes) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name
               << ".nodes' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& nodes_list = nodes->GetList();
  if (sizes_list.size() != nodes_list.size()) {
    LOG(ERROR)
        << "'allocators." << allocator_name
        << ".sizes' does not have the same number of elements as *.nodes";
    return false;
  }

  bool found_browser_alloc = false;
  size_t browser_alloc_index = 0;
  for (size_t i = 0; i < sizes_list.size(); i++) {
    if (counts_list[i].GetInt() == expected_alloc_count &&
        sizes_list[i].GetInt() != expected_alloc_size) {
      LOG(WARNING) << "Allocation candidate (size:" << sizes_list[i].GetInt()
                   << " count:" << counts_list[i].GetInt() << ")";
    }
    if (counts_list[i].GetInt() == expected_alloc_count &&
        sizes_list[i].GetInt() == expected_alloc_size) {
      browser_alloc_index = i;
      found_browser_alloc = true;
      break;
    }
  }

  if (!found_browser_alloc) {
    LOG(ERROR) << "Failed to find an allocation of the "
                  "appropriate size. Did the send buffer "
                  "not flush? (size: "
               << expected_alloc_size << " count:" << expected_alloc_count
               << ")";
    return false;
  }

  // Find the type, if an expectation was passed in.
  if (type_name) {
    NodeMap node_map;
    if (!ParseTypes(heaps_v2, &node_map)) {
      LOG(ERROR) << "Failed to parse type and string structs";
      return false;
    }

    int type = types_list[browser_alloc_index].GetInt();
    auto it = node_map.find(type);
    if (it == node_map.end()) {
      LOG(ERROR) << "Failed to look up type.";
      return false;
    }
    if (it->second.name != type_name) {
      LOG(ERROR) << "actual name: " << it->second.name
                 << " expected name: " << type_name;
      return false;
    }
  }

  // Check that the frame has the right name.
  if (!frame_name.empty()) {
    NodeMap node_map;
    if (!ParseNodes(heaps_v2, &node_map)) {
      LOG(ERROR) << "Failed to parse node and string structs";
      return false;
    }

    int node_id = nodes_list[browser_alloc_index].GetInt();
    auto it = node_map.find(node_id);

    if (it == node_map.end()) {
      LOG(ERROR) << "Failed to find frame for node with id: " << node_id;
      return false;
    }

    if (it->second.name != frame_name) {
      LOG(ERROR) << "Wrong name: " << it->second.name
                 << " for frame with expected name: " << frame_name;
      return false;
    }
  }

  // Check that the thread [top frame] has the right name.
  if (!thread_name.empty()) {
    NodeMap node_map;
    if (!ParseNodes(heaps_v2, &node_map)) {
      LOG(ERROR) << "Failed to parse node and string structs";
      return false;
    }

    int node_id = nodes_list[browser_alloc_index].GetInt();
    auto it = node_map.find(node_id);
    while (true) {
      if (it == node_map.end() || it->second.parent_id == kNullParent)
        break;
      it = node_map.find(it->second.parent_id);
    }

    if (it == node_map.end()) {
      LOG(ERROR) << "Failed to find root for node with id: " << node_id;
      return false;
    }

    if (it->second.name != thread_name) {
      LOG(ERROR) << "Wrong name: " << it->second.name
                 << " for thread with expected name: " << thread_name;
      return false;
    }
  }

  return true;
}

// |expected_size| of 0 means no expectation.
bool GetAllocatorSubarray(base::Value* heaps_v2,
                          const char* allocator_name,
                          const char* subarray_name,
                          size_t expected_size,
                          const base::Value::ListStorage** output) {
  base::Value* subarray =
      heaps_v2->FindPath({"allocators", allocator_name, subarray_name});
  if (!subarray) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name << "."
               << subarray_name << "' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& subarray_list = subarray->GetList();
  if (expected_size && subarray_list.size() != expected_size) {
    LOG(ERROR) << subarray_name << " has wrong size";
    return false;
  }

  *output = &subarray_list;
  return true;
}

bool ValidateSamplingAllocations(base::Value* heaps_v2,
                                 const char* allocator_name,
                                 int approximate_size,
                                 int approximate_count,
                                 const char* type_name) {
  // Maps type ids to strings.
  NodeMap type_map;
  if (!ParseTypes(heaps_v2, &type_map))
    return false;

  bool found = false;
  int id_of_type = 0;
  for (auto& pair : type_map) {
    if (pair.second.name == type_name) {
      id_of_type = pair.first;
      found = true;
    }
  }

  if (!found) {
    LOG(ERROR) << "Failed to find type with name: " << type_name;
    return false;
  }

  // Find the type with the appropriate id.
  const base::Value::ListStorage* types_list;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "types", 0,
                            &types_list)) {
    return false;
  }

  found = false;
  size_t index = 0;
  for (size_t i = 0; i < types_list->size(); ++i) {
    if ((*types_list)[i].GetInt() == id_of_type) {
      index = i;
      found = true;
      break;
    }
  }

  if (!found) {
    LOG(ERROR) << "Failed to find type with correct sid";
    return false;
  }

  // Look up the size.
  const base::Value::ListStorage* sizes;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "sizes",
                            types_list->size(), &sizes)) {
    return false;
  }

  if ((*sizes)[index].GetInt() < approximate_size / 2 ||
      (*sizes)[index].GetInt() > approximate_size * 2) {
    LOG(ERROR) << "sampling size " << (*sizes)[index].GetInt()
               << " was not within a factor of 2 of expected size "
               << approximate_size;
    return false;
  }

  // Look up the count.
  const base::Value::ListStorage* counts;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "counts",
                            types_list->size(), &counts)) {
    return false;
  }

  if ((*counts)[index].GetInt() < approximate_count / 2 ||
      (*counts)[index].GetInt() > approximate_count * 2) {
    LOG(ERROR) << "sampling size " << (*counts)[index].GetInt()
               << " was not within a factor of 2 of expected count "
               << approximate_count;
    return false;
  }
  return true;
}

bool ValidateProcessMmaps(base::Value* process_mmaps,
                          bool should_have_contents) {
  base::Value* vm_regions = process_mmaps->FindKey("vm_regions");
  size_t count = vm_regions->GetList().size();
  if (!should_have_contents) {
    if (count != 0) {
      LOG(ERROR) << "vm_regions should be empty, but has contents";
      return false;
    }
    return true;
  }

  if (count == 0) {
    LOG(ERROR) << "vm_regions should have contents, but doesn't";
    return false;
  }

  // File paths may contain PII. Make sure that "mf" entries only contain the
  // basename, rather than a full path.
  for (const base::Value& vm_region : vm_regions->GetList()) {
    const base::Value* file_path_value = vm_region.FindKey("mf");
    if (file_path_value) {
      std::string file_path = file_path_value->GetString();

      base::FilePath::StringType path(file_path.begin(), file_path.end());
      if (base::FilePath(path).BaseName().AsUTF8Unsafe() != file_path) {
        LOG(ERROR) << "vm_region should not contain file path: " << file_path;
        return false;
      }
    }
  }

  return true;
}

}  // namespace

TestDriver::TestDriver()
    : wait_for_ui_thread_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  partition_allocator_.init();
}
TestDriver::~TestDriver() {}

bool TestDriver::RunTest(const Options& options) {
  options_ = options;

  if (options_.should_sample)
    base::PoissonAllocationSampler::Get()->SuppressRandomnessForTest(true);

  running_on_ui_thread_ =
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);

  // The only thing to test for Mode::kNone is that profiling hasn't started.
  if (options_.mode == Mode::kNone) {
    if (running_on_ui_thread_) {
      has_started_ = Supervisor::GetInstance()->HasStarted();
    } else {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&TestDriver::GetHasStartedOnUIThread,
                         base::Unretained(this)));
      wait_for_ui_thread_.Wait();
    }
    if (has_started_) {
      LOG(ERROR) << "Profiling should not have started";
      return false;
    }
    return true;
  }

  if (running_on_ui_thread_) {
    if (!CheckOrStartProfilingOnUIThreadWithNestedRunLoops())
      return false;
    Supervisor::GetInstance()->SetKeepSmallAllocations(true);
    if (ShouldProfileRenderer())
      WaitForProfilingToStartForAllRenderersUIThread();
    if (ShouldProfileBrowser())
      MakeTestAllocations();
    CollectResults(true);
  } else {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&TestDriver::CheckOrStartProfilingOnUIThreadAndSignal,
                       base::Unretained(this)));
    wait_for_ui_thread_.Wait();
    if (!initialization_success_)
      return false;
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&TestDriver::SetKeepSmallAllocationsOnUIThreadAndSignal,
                       base::Unretained(this)));
    wait_for_ui_thread_.Wait();
    if (ShouldProfileRenderer()) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(
              &TestDriver::
                  WaitForProfilingToStartForAllRenderersUIThreadAndSignal,
              base::Unretained(this)));
      wait_for_ui_thread_.Wait();
    }
    if (ShouldProfileBrowser()) {
      base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                               base::BindOnce(&TestDriver::MakeTestAllocations,
                                              base::Unretained(this)));
    }
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindOnce(&TestDriver::CollectResults,
                                            base::Unretained(this), false));
    wait_for_ui_thread_.Wait();
  }

  std::unique_ptr<base::Value> dump_json =
      base::JSONReader::Read(serialized_trace_);
  if (!dump_json) {
    LOG(ERROR) << "Failed to deserialize trace.";
    return false;
  }

  if (!ValidateBrowserAllocations(dump_json.get())) {
    LOG(ERROR) << "Failed to validate browser allocations";
    return false;
  }

  if (!ValidateRendererAllocations(dump_json.get())) {
    LOG(ERROR) << "Failed to validate renderer allocations";
    return false;
  }

  return true;
}

void TestDriver::GetHasStartedOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  has_started_ = Supervisor::GetInstance()->HasStarted();
  wait_for_ui_thread_.Signal();
}

void TestDriver::CheckOrStartProfilingOnUIThreadAndSignal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  initialization_success_ =
      CheckOrStartProfilingOnUIThreadWithAsyncSignalling();

  // If the flag is true, then the WaitableEvent will be signaled after
  // profiling has started.
  if (!wait_for_profiling_to_start_)
    wait_for_ui_thread_.Signal();
}

void TestDriver::SetKeepSmallAllocationsOnUIThreadAndSignal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  Supervisor::GetInstance()->SetKeepSmallAllocations(true);
  wait_for_ui_thread_.Signal();
}

bool TestDriver::CheckOrStartProfilingOnUIThreadWithAsyncSignalling() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (options_.profiling_already_started) {
    if (!Supervisor::GetInstance()->HasStarted()) {
      LOG(ERROR) << "Profiling should have been started, but wasn't";
      return false;
    }

    // Even if profiling has started, it's possible that the allocator shim
    // has not yet been initialized. Wait for it.
    if (ShouldProfileBrowser()) {
      bool already_initialized = SetOnInitAllocatorShimCallbackForTesting(
          base::BindOnce(&base::WaitableEvent::Signal,
                         base::Unretained(&wait_for_ui_thread_)),
          base::ThreadTaskRunnerHandle::Get());
      if (!already_initialized) {
        wait_for_profiling_to_start_ = true;
      }
    }
    return true;
  }

  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  if (!connection) {
    LOG(ERROR) << "A ServiceManagerConnection was not available for the "
                  "current process.";
    return false;
  }

  wait_for_profiling_to_start_ = true;
  base::OnceClosure start_callback;

  // If we're going to profile the browser, then wait for the allocator shim to
  // start. Otherwise, wait for the Supervisor to start.
  if (ShouldProfileBrowser()) {
    SetOnInitAllocatorShimCallbackForTesting(
        base::BindOnce(&base::WaitableEvent::Signal,
                       base::Unretained(&wait_for_ui_thread_)),
        base::ThreadTaskRunnerHandle::Get());
  } else {
    start_callback = base::BindOnce(&base::WaitableEvent::Signal,
                                    base::Unretained(&wait_for_ui_thread_));
  }

  uint32_t sampling_rate = options_.should_sample
                               ? (options_.sample_everything ? 2 : kSampleRate)
                               : 1;
  Supervisor::GetInstance()->Start(connection, options_.mode,
                                   options_.stack_mode, sampling_rate,
                                   std::move(start_callback));

  return true;
}

bool TestDriver::CheckOrStartProfilingOnUIThreadWithNestedRunLoops() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (options_.profiling_already_started) {
    if (!Supervisor::GetInstance()->HasStarted()) {
      LOG(ERROR) << "Profiling should have been started, but wasn't";
      return false;
    }

    // Even if profiling has started, it's possible that the allocator shim
    // has not yet been initialized. Wait for it.
    if (ShouldProfileBrowser()) {
      std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);
      bool already_initialized = SetOnInitAllocatorShimCallbackForTesting(
          run_loop->QuitClosure(), base::ThreadTaskRunnerHandle::Get());
      if (!already_initialized)
        run_loop->Run();
    }
    return true;
  }

  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  if (!connection) {
    LOG(ERROR) << "A ServiceManagerConnection was not available for the "
                  "current process.";
    return false;
  }

  // When this is not-null, initialization should wait for the QuitClosure to be
  // called.
  std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);
  base::OnceClosure start_callback;

  // If we're going to profile the browser, then wait for the allocator shim to
  // start. Otherwise, wait for the Supervisor to start.
  if (ShouldProfileBrowser()) {
    SetOnInitAllocatorShimCallbackForTesting(
        run_loop->QuitClosure(), base::ThreadTaskRunnerHandle::Get());
  } else {
    start_callback = run_loop->QuitClosure();
  }

  uint32_t sampling_rate = options_.should_sample
                               ? (options_.sample_everything ? 2 : kSampleRate)
                               : 1;
  Supervisor::GetInstance()->Start(connection, options_.mode,
                                   options_.stack_mode, sampling_rate,
                                   std::move(start_callback));

  run_loop->Run();

  return true;
}

void TestDriver::MakeTestAllocations() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::PlatformThread::SetName(kThreadName);

  // In sampling mode, only sampling allocations are relevant.
  if (!IsRecordingAllAllocations()) {
    leaks_.reserve(kSamplingAllocCount);
    for (int i = 0; i < kSamplingAllocCount; ++i) {
      leaks_.push_back(static_cast<char*>(partition_allocator_.root()->Alloc(
          kSamplingAllocSize, kSamplingAllocTypeName)));
    }
    return;
  }

  leaks_.reserve(2 * kMallocAllocCount + 1 + kPartitionAllocSize);

  {
    TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION event(kMallocTypeTag);
    TRACE_EVENT0(kTestCategory, kMallocEvent);

    for (int i = 0; i < kMallocAllocCount; ++i) {
      leaks_.push_back(new char[kMallocAllocSize]);
    }
  }

  {
    TRACE_EVENT0(kTestCategory, kPAEvent);

    for (int i = 0; i < kPartitionAllocCount; ++i) {
      leaks_.push_back(static_cast<char*>(partition_allocator_.root()->Alloc(
          kPartitionAllocSize, kPartitionAllocTypeName)));
    }
  }

  {
    TRACE_EVENT0(kTestCategory, kVariadicEvent);

    for (int i = 0; i < kVariadicAllocCount; ++i) {
      leaks_.push_back(new char[i + 8000]);  // Variadic allocation.
      total_variadic_allocations_ += i + 8000;
    }
  }

  // // Navigate around to force allocations in the renderer.
  // ASSERT_TRUE(embedded_test_server()->Start());
  // ui_test_utils::NavigateToURL(
  //     browser(), embedded_test_server()->GetURL("/english_page.html"));
  // // Vive la France!
  // ui_test_utils::NavigateToURL(
  //     browser(), embedded_test_server()->GetURL("/french_page.html"));
}

void TestDriver::CollectResults(bool synchronous) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::Closure finish_tracing_closure;
  std::unique_ptr<base::RunLoop> run_loop;

  if (synchronous) {
    run_loop.reset(new base::RunLoop);
    finish_tracing_closure = run_loop->QuitClosure();
  } else {
    finish_tracing_closure = base::Bind(&base::WaitableEvent::Signal,
                                        base::Unretained(&wait_for_ui_thread_));
  }

  Supervisor::GetInstance()->RequestTraceWithHeapDump(
      base::BindOnce(&TestDriver::TraceFinished, base::Unretained(this),
                     std::move(finish_tracing_closure)),
      /* anonymize= */ true);

  if (synchronous)
    run_loop->Run();
}

void TestDriver::TraceFinished(base::Closure closure,
                               bool success,
                               std::string trace_json) {
  serialized_trace_.swap(trace_json);
  std::move(closure).Run();
}

bool TestDriver::ValidateBrowserAllocations(base::Value* dump_json) {
  base::Value* heaps_v2 =
      FindArgDump(base::Process::Current().Pid(), dump_json, "heaps_v2");

  if (!ShouldProfileBrowser()) {
    if (heaps_v2) {
      LOG(ERROR) << "There should be no heap dump for the browser.";
      return false;
    }
    return true;
  }

  if (!heaps_v2) {
    LOG(ERROR) << "Browser heap dump missing.";
    return false;
  }

  bool result = false;

  bool should_validate_dumps = true;
#if defined(OS_ANDROID) && !defined(OFFICIAL_BUILD)
  // TODO(ajwong): This step fails on Nexus 5X devices running kit-kat. It works
  // on Nexus 5X devices running oreo. The problem is that all allocations have
  // the same [an effectively empty] backtrace and get glommed together. More
  // investigation is necessary. For now, I'm turning this off for Android.
  // https://crbug.com/786450.
  if (!HasPseudoFrames())
    should_validate_dumps = false;
#endif

  std::string thread_name = ShouldIncludeNativeThreadNames() ? kThreadName : "";

  if (IsRecordingAllAllocations()) {
    if (should_validate_dumps) {
      result = ValidateDump(heaps_v2, kMallocAllocSize * kMallocAllocCount,
                            kMallocAllocCount, "malloc", kMallocTypeTag,
                            HasPseudoFrames() ? kMallocEvent : "", thread_name);
      if (!result) {
        LOG(ERROR) << "Failed to validate malloc fixed allocations";
        return false;
      }

      result = ValidateDump(
          heaps_v2, total_variadic_allocations_, kVariadicAllocCount, "malloc",
          nullptr, HasPseudoFrames() ? kVariadicEvent : "", thread_name);
      if (!result) {
        LOG(ERROR) << "Failed to validate malloc variadic allocations";
        return false;
      }
    }

    // TODO(ajwong): Like malloc, all Partition-Alloc allocations get glommed
    // together for some Android device/OS configurations. However, since there
    // is only one place that uses partition alloc in the browser process [this
    // test], the count is still valid. This should still be made more robust by
    // fixing backtrace. https://crbug.com/786450.
    result = ValidateDump(heaps_v2, kPartitionAllocSize * kPartitionAllocCount,
                          kPartitionAllocCount, "partition_alloc",
                          kPartitionAllocTypeName,
                          HasPseudoFrames() ? kPAEvent : "", thread_name);
    if (!result) {
      LOG(ERROR) << "Failed to validate PA allocations";
      return false;
    }
  } else {
    bool result = ValidateSamplingAllocations(
        heaps_v2, "partition_alloc", kSamplingAllocSize * kSamplingAllocCount,
        kSamplingAllocCount, kSamplingAllocTypeName);
    if (!result) {
      LOG(ERROR) << "Failed to validate sampling allocations";
      return false;
    }
  }

  int process_count = NumProcessesWithName(dump_json, "Browser", nullptr);
  if (process_count != 1) {
    LOG(ERROR) << "Found " << process_count
               << " processes with name: Browser. Expected 1.";
    return false;
  }

  base::Value* process_mmaps =
      FindArgDump(base::Process::Current().Pid(), dump_json, "process_mmaps");
  if (!ValidateProcessMmaps(process_mmaps, HasNativeFrames())) {
    LOG(ERROR) << "Failed to validate browser process mmaps.";
    return false;
  }

  return true;
}

bool TestDriver::ValidateRendererAllocations(base::Value* dump_json) {
  // On Android Webview, there is may not be a separate Renderer process. If we
  // are not asked to profile the Renderer, do not perform any Renderer checks.
  if (!ShouldProfileRenderer())
    return true;

  std::vector<int> pids;
  bool result = NumProcessesWithName(dump_json, "Renderer", &pids) >= 1;
  if (!result) {
    LOG(ERROR) << "Failed to find process with name Renderer";
    return false;
  }

  for (int pid : pids) {
    base::ProcessId renderer_pid = static_cast<base::ProcessId>(pid);
    base::Value* heaps_v2 = FindArgDump(renderer_pid, dump_json, "heaps_v2");
    if (!heaps_v2) {
      LOG(ERROR) << "Failed to find heaps v2 for renderer";
      return false;
    }

    base::Value* process_mmaps =
        FindArgDump(renderer_pid, dump_json, "process_mmaps");
    if (!ValidateProcessMmaps(process_mmaps, HasNativeFrames())) {
      LOG(ERROR) << "Failed to validate renderer process mmaps.";
      return false;
    }
  }

  return true;
}

bool TestDriver::ShouldProfileBrowser() {
  return options_.mode == Mode::kAll || options_.mode == Mode::kBrowser ||
         options_.mode == Mode::kMinimal ||
         options_.mode == Mode::kUtilityAndBrowser;
}

bool TestDriver::ShouldProfileRenderer() {
  return options_.mode == Mode::kAll || options_.mode == Mode::kAllRenderers;
}

bool TestDriver::ShouldIncludeNativeThreadNames() {
  return options_.stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES;
}

bool TestDriver::HasPseudoFrames() {
  return options_.stack_mode == mojom::StackMode::PSEUDO ||
         options_.stack_mode == mojom::StackMode::MIXED;
}

bool TestDriver::HasNativeFrames() {
  return options_.stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES ||
         options_.stack_mode == mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES ||
         options_.stack_mode == mojom::StackMode::MIXED;
}

bool TestDriver::IsRecordingAllAllocations() {
  return !options_.should_sample || options_.sample_everything;
}

void TestDriver::WaitForProfilingToStartForAllRenderersUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  while (true) {
    std::vector<base::ProcessId> profiled_pids;
    base::RunLoop run_loop;
    auto callback = base::BindOnce(
        [](std::vector<base::ProcessId>* results, base::OnceClosure finished,
           std::vector<base::ProcessId> pids) {
          results->swap(pids);
          std::move(finished).Run();
        },
        &profiled_pids, run_loop.QuitClosure());
    Supervisor::GetInstance()->GetProfiledPids(std::move(callback));
    run_loop.Run();

    if (RenderersAreBeingProfiled(profiled_pids))
      break;
  }
}

void TestDriver::WaitForProfilingToStartForAllRenderersUIThreadAndSignal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  Supervisor::GetInstance()->GetProfiledPids(base::BindOnce(
      &TestDriver::WaitForProfilingToStartForAllRenderersUIThreadCallback,
      base::Unretained(this)));
}

void TestDriver::WaitForProfilingToStartForAllRenderersUIThreadCallback(
    std::vector<base::ProcessId> results) {
  if (RenderersAreBeingProfiled(results)) {
    wait_for_ui_thread_.Signal();
    return;
  }

  // Brief sleep to prevent spamming the task queue, since this code is called
  // in a tight loop.
  base::PlatformThread::Sleep(base::TimeDelta::FromMicroseconds(100));

  WaitForProfilingToStartForAllRenderersUIThreadAndSignal();
}

}  // namespace heap_profiling
