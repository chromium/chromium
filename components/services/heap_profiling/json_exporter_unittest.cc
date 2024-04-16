// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/json_exporter.h"

#include <sstream>

#include "base/gtest_prod_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace heap_profiling {
namespace {

using MemoryMap = std::vector<memory_instrumentation::mojom::VmRegionPtr>;

static constexpr int kNoParent = -1;

#if !defined(ADDRESS_SANITIZER)
// Finds the first vm region in the given periodic interval. Returns null on
// failure.
const base::Value* FindFirstRegionWithAnyName(const base::Value::Dict& root) {
  const base::Value::Dict* found_mmaps = root.FindDict("process_mmaps");
  if (!found_mmaps)
    return nullptr;
  const base::Value::List* found_regions = found_mmaps->FindList("vm_regions");
  if (!found_regions)
    return nullptr;

  for (const base::Value& cur : *found_regions) {
    const std::string* found_name = cur.GetDict().FindString("mf");
    if (!found_name)
      return nullptr;
    if (*found_name != "")
      return &cur;
  }
  return nullptr;
}
#endif  // !defined(ADDRESS_SANITIZER)

// Looks up a given string id from the string table. Returns -1 if not found.
int GetIdFromStringTable(const base::Value::List& strings, const char* text) {
  for (const auto& string : strings) {
    std::optional<int> string_id = string.GetDict().FindInt("id");
    const std::string* string_text = string.GetDict().FindString("string");
    if (string_id.has_value() && string_text != nullptr &&
        *string_text == text) {
      return *string_id;
    }
  }
  return -1;
}

// Looks up a given string from the string table. Returns empty string if not
// found.
std::string GetStringFromStringTable(const base::Value::List& strings,
                                     int sid) {
  for (const auto& string : strings) {
    std::optional<int> string_id = string.GetDict().FindInt("id");
    if (*string_id == sid) {
      const std::string* string_text = string.GetDict().FindString("string");
      if (!string_text)
        return std::string();
      return *string_text;
    }
  }
  return std::string();
}

int GetNodeWithNameID(const base::Value::List& nodes, int sid) {
  for (const auto& node : nodes) {
    std::optional<int> node_id = node.GetDict().FindInt("id");
    std::optional<int> node_name_sid = node.GetDict().FindInt("name_sid");
    if (node_id.has_value() && node_name_sid.has_value() &&
        *node_name_sid == sid) {
      return *node_id;
    }
  }
  return -1;
}

int GetOffsetForBacktraceID(const base::Value::List& nodes, int id) {
  int offset = 0;
  for (const auto& node : nodes) {
    if (node.GetInt() == id)
      return offset;
    offset++;
  }
  return -1;
}

bool IsBacktraceInList(const base::Value::List& backtraces,
                       int id,
                       int parent) {
  for (const auto& backtrace : backtraces) {
    std::optional<int> backtrace_id = backtrace.GetDict().FindInt("id");
    if (!backtrace_id.has_value())
      continue;

    std::optional<int> backtrace_parent = backtrace.GetDict().FindInt("parent");
    int backtrace_parent_int = kNoParent;
    if (backtrace_parent.has_value())
      backtrace_parent_int = *backtrace_parent;

    if (*backtrace_id == id && backtrace_parent_int == parent)
      return true;
  }
  return false;
}

void InsertAllocation(AllocationMap* allocs,
                      AllocatorType type,
                      size_t size,
                      std::vector<Address> stack,
                      int context_id) {
  AllocationMetrics& metrics =
      allocs
          ->emplace(std::piecewise_construct,
                    std::forward_as_tuple(type, std::move(stack), context_id),
                    std::forward_as_tuple())
          .first->second;
  metrics.size += size;
  metrics.count++;
}

}  // namespace

TEST(ProfilingJsonExporterTest, Simple) {
  std::vector<Address> stack1{Address(0x5678), Address(0x1234)};
  std::vector<Address> stack2{Address(0x9013), Address(0x9012),
                              Address(0x1234)};
  AllocationMap allocs;
  InsertAllocation(&allocs, AllocatorType::kMalloc, 20, stack1, 0);
  InsertAllocation(&allocs, AllocatorType::kMalloc, 32, stack2, 0);
  InsertAllocation(&allocs, AllocatorType::kMalloc, 20, stack1, 0);
  InsertAllocation(&allocs, AllocatorType::kPartitionAlloc, 20, stack1, 0);
  InsertAllocation(&allocs, AllocatorType::kMalloc, 12, stack2, 0);

  ExportParams params;
  params.allocs = std::move(allocs);
  std::string json = ExportMemoryMapsAndV2StackTraceToJSON(&params);

  // JSON should parse.
  std::optional<base::Value> root = base::JSONReader::Read(json);
  ASSERT_TRUE(root);

  const base::Value::Dict* dict = root->GetIfDict();
  ASSERT_TRUE(dict);

  // Validate the allocators summary.
  const base::Value::Dict* malloc_summary =
      dict->FindDictByDottedPath("allocators.malloc");
  ASSERT_TRUE(malloc_summary);
  const std::string* malloc_size =
      malloc_summary->FindStringByDottedPath("attrs.size.value");
  ASSERT_TRUE(malloc_size);
  EXPECT_EQ("54", *malloc_size);
  const std::string* malloc_virtual_size =
      malloc_summary->FindStringByDottedPath("attrs.virtual_size.value");
  ASSERT_TRUE(malloc_virtual_size);
  EXPECT_EQ("54", *malloc_virtual_size);

  const base::Value::Dict* partition_alloc_summary =
      dict->FindDictByDottedPath("allocators.partition_alloc");
  ASSERT_TRUE(partition_alloc_summary);
  const std::string* partition_alloc_size =
      partition_alloc_summary->FindStringByDottedPath("attrs.size.value");
  ASSERT_TRUE(partition_alloc_size);
  EXPECT_EQ("14", *partition_alloc_size);
  const std::string* partition_alloc_virtual_size =
      partition_alloc_summary->FindStringByDottedPath(
          "attrs.virtual_size.value");
  ASSERT_TRUE(partition_alloc_virtual_size);
  EXPECT_EQ("14", *partition_alloc_virtual_size);

  const base::Value::Dict* heaps_v2 = dict->FindDict("heaps_v2");
  ASSERT_TRUE(heaps_v2);

  // Retrieve maps and validate their structure.
  const base::Value::List* nodes = heaps_v2->FindListByDottedPath("maps.nodes");
  const base::Value::List* strings =
      heaps_v2->FindListByDottedPath("maps.strings");
  ASSERT_TRUE(nodes);
  ASSERT_TRUE(strings);

  // Validate the strings table.
  EXPECT_EQ(5u, strings->size());
  int sid_unknown = GetIdFromStringTable(*strings, "[unknown]");
  int sid_1234 = GetIdFromStringTable(*strings, "pc:1234");
  int sid_5678 = GetIdFromStringTable(*strings, "pc:5678");
  int sid_9012 = GetIdFromStringTable(*strings, "pc:9012");
  int sid_9013 = GetIdFromStringTable(*strings, "pc:9013");
  EXPECT_NE(-1, sid_unknown);
  EXPECT_NE(-1, sid_1234);
  EXPECT_NE(-1, sid_5678);
  EXPECT_NE(-1, sid_9012);
  EXPECT_NE(-1, sid_9013);

  // Validate the nodes table.
  // Nodes should be a list with 4 items.
  //   [0] => address: 1234  parent: none
  //   [1] => address: 5678  parent: 0
  //   [2] => address: 9012  parent: 0
  //   [3] => address: 9013  parent: 2
  EXPECT_EQ(4u, nodes->size());
  int id0 = GetNodeWithNameID(*nodes, sid_1234);
  int id1 = GetNodeWithNameID(*nodes, sid_5678);
  int id2 = GetNodeWithNameID(*nodes, sid_9012);
  int id3 = GetNodeWithNameID(*nodes, sid_9013);
  EXPECT_NE(-1, id0);
  EXPECT_NE(-1, id1);
  EXPECT_NE(-1, id2);
  EXPECT_NE(-1, id3);
  EXPECT_TRUE(IsBacktraceInList(*nodes, id0, kNoParent));
  EXPECT_TRUE(IsBacktraceInList(*nodes, id1, id0));
  EXPECT_TRUE(IsBacktraceInList(*nodes, id2, id0));
  EXPECT_TRUE(IsBacktraceInList(*nodes, id3, id2));

  // Retrieve the allocations and validate their structure.
  const base::Value::List* counts =
      heaps_v2->FindListByDottedPath("allocators.malloc.counts");
  const base::Value::List* types =
      heaps_v2->FindListByDottedPath("allocators.malloc.types");
  const base::Value::List* sizes =
      heaps_v2->FindListByDottedPath("allocators.malloc.sizes");
  const base::Value::List* backtraces =
      heaps_v2->FindListByDottedPath("allocators.malloc.nodes");

  ASSERT_TRUE(counts);
  ASSERT_TRUE(types);
  ASSERT_TRUE(sizes);
  ASSERT_TRUE(backtraces);

  // Counts should be a list of two items, a 1 and a 2. The two matching 20-byte
  // allocations should be coalesced to produce the 2.
  EXPECT_EQ(2u, counts->size());
  EXPECT_EQ(2u, types->size());
  EXPECT_EQ(2u, sizes->size());

  int node1 = GetOffsetForBacktraceID(*backtraces, id1);
  int node3 = GetOffsetForBacktraceID(*backtraces, id3);
  EXPECT_NE(-1, node1);
  EXPECT_NE(-1, node3);

  // Validate node allocated with |stack1|.
  EXPECT_EQ(2, (*counts)[node1].GetInt());
  EXPECT_EQ(0, (*types)[node1].GetInt());
  EXPECT_EQ(40, (*sizes)[node1].GetInt());
  EXPECT_EQ(id1, (*backtraces)[node1].GetInt());

  // Validate node allocated with |stack2|.
  EXPECT_EQ(2, (*counts)[node3].GetInt());
  EXPECT_EQ(0, (*types)[node3].GetInt());
  EXPECT_EQ(44, (*sizes)[node3].GetInt());
  EXPECT_EQ(id3, (*backtraces)[node3].GetInt());

  // Validate that the PartitionAlloc one got through.
  counts = heaps_v2->FindListByDottedPath("allocators.partition_alloc.counts");
  types = heaps_v2->FindListByDottedPath("allocators.partition_alloc.types");
  sizes = heaps_v2->FindListByDottedPath("allocators.partition_alloc.sizes");
  backtraces =
      heaps_v2->FindListByDottedPath("allocators.partition_alloc.nodes");

  ASSERT_TRUE(counts);
  ASSERT_TRUE(types);
  ASSERT_TRUE(sizes);
  ASSERT_TRUE(backtraces);

  // There should just be one entry for the partition_alloc allocation.
  EXPECT_EQ(1u, counts->size());
  EXPECT_EQ(1u, types->size());
  EXPECT_EQ(1u, sizes->size());
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/42050458): Re-enable when MemoryMaps works on Fuchsia.
#define MAYBE_MemoryMaps DISABLED_MemoryMaps
#else
#define MAYBE_MemoryMaps MemoryMaps
#endif
// GetProcessMemoryMaps iterates through every memory region, making allocations
// for each one. ASAN will potentially, for each allocation, make memory
// regions. This will cause the test to time out.
#if !defined(ADDRESS_SANITIZER)
TEST(ProfilingJsonExporterTest, MAYBE_MemoryMaps) {
  ExportParams params;
  params.maps = memory_instrumentation::OSMetrics::GetProcessMemoryMaps(
      base::Process::Current().Pid());
  ASSERT_GT(params.maps.size(), 2u);

  std::string json = ExportMemoryMapsAndV2StackTraceToJSON(&params);

  // JSON should parse.
  std::optional<base::Value> root = base::JSONReader::Read(json);
  ASSERT_TRUE(root);

  const base::Value::Dict* dict = root->GetIfDict();
  ASSERT_TRUE(dict);

  const base::Value* region = FindFirstRegionWithAnyName(*dict);
  ASSERT_TRUE(region) << "Array contains no named vm regions";

  const std::string* start_address = region->GetDict().FindString("sa");
  ASSERT_TRUE(start_address);
  EXPECT_NE(*start_address, "");
  EXPECT_NE(*start_address, "0");

  const std::string* size = region->GetDict().FindString("sz");
  ASSERT_TRUE(size);
  EXPECT_NE(*size, "");
  EXPECT_NE(*size, "0");
}
#endif  // !defined(ADDRESS_SANITIZER)

TEST(ProfilingJsonExporterTest, Context) {
  ExportParams params;

  std::vector<Address> stack{Address(0x1234)};

  std::string context_str1("Context 1");
  int context_id1 = 1;
  params.context_map[context_str1] = context_id1;
  std::string context_str2("Context 2");
  int context_id2 = 2;
  params.context_map[context_str2] = context_id2;

  // Make 4 events, all with identical metadata except context. Two share the
  // same context so should get folded, one has unique context, and one has no
  // context.
  AllocationMap allocs;
  InsertAllocation(&allocs, AllocatorType::kPartitionAlloc, 16, stack,
                   context_id1);
  InsertAllocation(&allocs, AllocatorType::kPartitionAlloc, 16, stack,
                   context_id2);
  InsertAllocation(&allocs, AllocatorType::kPartitionAlloc, 16, stack, 0);
  InsertAllocation(&allocs, AllocatorType::kPartitionAlloc, 16, stack,
                   context_id1);
  params.allocs = std::move(allocs);

  std::string json = ExportMemoryMapsAndV2StackTraceToJSON(&params);

  // JSON should parse.
  std::optional<base::Value> root = base::JSONReader::Read(json);
  ASSERT_TRUE(root);

  // Retrieve the allocations.
  const base::Value::Dict* heaps_v2 = root->GetDict().FindDict("heaps_v2");
  ASSERT_TRUE(heaps_v2);

  const base::Value::List* counts =
      heaps_v2->FindListByDottedPath("allocators.partition_alloc.counts");
  ASSERT_TRUE(counts);
  const base::Value::List* types =
      heaps_v2->FindListByDottedPath("allocators.partition_alloc.types");
  ASSERT_TRUE(types);

  // There should be three allocations, two coalesced ones, one with unique
  // context, and one with no context.
  EXPECT_EQ(3u, counts->size());
  EXPECT_EQ(3u, types->size());

  const base::Value::List* types_map =
      heaps_v2->FindListByDottedPath("maps.types");
  ASSERT_TRUE(types_map);
  const base::Value::List* strings =
      heaps_v2->FindListByDottedPath("maps.strings");
  ASSERT_TRUE(strings);

  // Reconstruct the map from type id to string.
  std::map<int, std::string> type_to_string;
  for (const auto& type : *types_map) {
    const std::optional<int> id = type.GetDict().FindInt("id");
    ASSERT_TRUE(id.has_value());
    const std::optional<int> name_sid = type.GetDict().FindInt("name_sid");
    ASSERT_TRUE(name_sid.has_value());

    type_to_string[*id] = GetStringFromStringTable(*strings, *name_sid);
  }

  // Track the three entries we have down to what we expect. The order is not
  // defined so this is relatively complex to do.
  bool found_double_context = false;  // Allocations sharing the same context.
  bool found_single_context = false;  // Allocation with unique context.
  bool found_no_context = false;      // Allocation with no context.
  for (size_t i = 0; i < types->size(); i++) {
    const auto& found = type_to_string.find((*types)[i].GetInt());
    ASSERT_NE(type_to_string.end(), found);
    if (found->second == context_str1) {
      // Context string matches the one with two allocations.
      ASSERT_FALSE(found_double_context);
      found_double_context = true;
      ASSERT_EQ(2, (*counts)[i].GetInt());
    } else if (found->second == context_str2) {
      // Context string matches the one with one allocation.
      ASSERT_FALSE(found_single_context);
      found_single_context = true;
      ASSERT_EQ(1, (*counts)[i].GetInt());
    } else if (found->second == "[unknown]") {
      // Context string for the one with no context.
      ASSERT_FALSE(found_no_context);
      found_no_context = true;
      ASSERT_EQ(1, (*counts)[i].GetInt());
    }
  }

  // All three types of things should have been found in the loop.
  ASSERT_TRUE(found_double_context);
  ASSERT_TRUE(found_single_context);
  ASSERT_TRUE(found_no_context);
}

#if defined(ARCH_CPU_64_BITS)
TEST(ProfilingJsonExporterTest, LargeAllocation) {
  std::vector<Address> stack1{Address(0x5678), Address(0x1234)};
  AllocationMap allocs;
  InsertAllocation(&allocs, AllocatorType::kMalloc,
                   static_cast<size_t>(0x9876543210ul), stack1, 0);

  ExportParams params;
  params.allocs = std::move(allocs);
  std::string json = ExportMemoryMapsAndV2StackTraceToJSON(&params);

  // JSON should parse.
  ASSERT_OK_AND_ASSIGN(auto parsed_json,
                       base::JSONReader::ReadAndReturnValueWithError(json));

  // Validate the allocators summary.
  const base::Value::Dict* malloc_summary =
      parsed_json.GetDict().FindDictByDottedPath("allocators.malloc");
  ASSERT_TRUE(malloc_summary);
  const std::string* malloc_size =
      malloc_summary->FindStringByDottedPath("attrs.size.value");
  ASSERT_TRUE(malloc_size);
  EXPECT_EQ("9876543210", *malloc_size);
  const std::string* malloc_virtual_size =
      malloc_summary->FindStringByDottedPath("attrs.virtual_size.value");
  ASSERT_TRUE(malloc_virtual_size);
  EXPECT_EQ("9876543210", *malloc_virtual_size);

  // Validate allocators details.
  // heaps_v2.allocators.malloc.sizes.reduce((a,s)=>a+s,0).
  const base::Value::Dict* malloc =
      parsed_json.GetDict().FindDictByDottedPath("heaps_v2.allocators.malloc");
  const base::Value::List* malloc_sizes = malloc->FindList("sizes");
  EXPECT_EQ(1u, malloc_sizes->size());
  EXPECT_EQ(0x9876543210ul, (*malloc_sizes)[0].GetDouble());
}
#endif

}  // namespace heap_profiling
