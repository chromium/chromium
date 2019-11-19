// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/json_exporter.h"

#include <sstream>

#include "base/gtest_prod_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
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
const base::Value* FindFirstRegionWithAnyName(const base::Value* root) {
  const base::Value* found_mmaps =
      root->FindKeyOfType("process_mmaps", base::Value::Type::DICTIONARY);
  if (!found_mmaps)
    return nullptr;
  const base::Value* found_regions =
      found_mmaps->FindKeyOfType("vm_regions", base::Value::Type::LIST);
  if (!found_regions)
    return nullptr;

  for (const base::Value& cur : found_regions->GetList()) {
    const base::Value* found_name =
        cur.FindKeyOfType("mf", base::Value::Type::STRING);
    if (!found_name)
      return nullptr;
    if (found_name->GetString() != "")
      return &cur;
  }
  return nullptr;
}
#endif  // !defined(ADDRESS_SANITIZER)

// Looks up a given string id from the string table. Returns -1 if not found.
int GetIdFromStringTable(const base::Value* strings, const char* text) {
  for (const auto& string : strings->GetList()) {
    const base::Value* string_id =
        string.FindKeyOfType("id", base::Value::Type::INTEGER);
    const base::Value* string_text =
        string.FindKeyOfType("string", base::Value::Type::STRING);
    if (string_id != nullptr && string_text != nullptr &&
        string_text->GetString() == text)
      return string_id->GetInt();
  }
  return -1;
}

// Looks up a given string from the string table. Returns empty string if not
// found.
std::string GetStringFromStringTable(const base::Value* strings, int sid) {
  for (const auto& string : strings->GetList()) {
    const base::Value* string_id =
        string.FindKeyOfType("id", base::Value::Type::INTEGER);
    if (string_id->GetInt() == sid) {
      const base::Value* string_text =
          string.FindKeyOfType("string", base::Value::Type::STRING);
      if (!string_text)
        return std::string();
      return string_text->GetString();
    }
  }
  return std::string();
}

int GetNodeWithNameID(const base::Value* nodes, int sid) {
  for (const auto& node : nodes->GetList()) {
    const base::Value* node_id =
        node.FindKeyOfType("id", base::Value::Type::INTEGER);
    const base::Value* node_name_sid =
        node.FindKeyOfType("name_sid", base::Value::Type::INTEGER);
    if (node_id != nullptr && node_name_sid != nullptr &&
        node_name_sid->GetInt() == sid)
      return node_id->GetInt();
  }
  return -1;
}

int GetOffsetForBacktraceID(const base::Value* nodes, int id) {
  int offset = 0;
  for (const auto& node : nodes->GetList()) {
    if (node.GetInt() == id)
      return offset;
    offset++;
  }
  return -1;
}

bool IsBacktraceInList(const base::Value* backtraces, int id, int parent) {
  for (const auto& backtrace : backtraces->GetList()) {
    const base::Value* backtrace_id =
        backtrace.FindKeyOfType("id", base::Value::Type::INTEGER);
    if (backtrace_id == nullptr)
      continue;

    const base::Value* backtrace_parent =
        backtrace.FindKeyOfType("parent", base::Value::Type::INTEGER);
    int backtrace_parent_int = kNoParent;
    if (backtrace_parent)
      backtrace_parent_int = backtrace_parent->GetInt();

    if (backtrace_id->GetInt() == id && backtrace_parent_int == parent)
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
  base::JSONReader reader(base::JSON_PARSE_RFC);
  std::unique_ptr<base::Value> root = reader.ReadToValueDeprecated(json);
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, reader.error_code())
      << reader.GetErrorMessage();
  ASSERT_TRUE(root);

  // Validate the allocators summary.
  const base::Value* malloc_summary = root->FindPath({"allocators", "malloc"});
  ASSERT_TRUE(malloc_summary);
  const base::Value* malloc_size =
      malloc_summary->FindPath({"attrs", "size", "value"});
  ASSERT_TRUE(malloc_size);
  EXPECT_EQ("54", malloc_size->GetString());
  const base::Value* malloc_virtual_size =
      malloc_summary->FindPath({"attrs", "virtual_size", "value"});
  ASSERT_TRUE(malloc_virtual_size);
  EXPECT_EQ("54", malloc_virtual_size->GetString());

  const base::Value* partition_alloc_summary =
      root->FindPath({"allocators", "partition_alloc"});
  ASSERT_TRUE(partition_alloc_summary);
  const base::Value* partition_alloc_size =
      partition_alloc_summary->FindPath({"attrs", "size", "value"});
  ASSERT_TRUE(partition_alloc_size);
  EXPECT_EQ("14", partition_alloc_size->GetString());
  const base::Value* partition_alloc_virtual_size =
      partition_alloc_summary->FindPath({"attrs", "virtual_size", "value"});
  ASSERT_TRUE(partition_alloc_virtual_size);
  EXPECT_EQ("14", partition_alloc_virtual_size->GetString());

  const base::Value* heaps_v2 = root->FindKey("heaps_v2");
  ASSERT_TRUE(heaps_v2);

  // Retrieve maps and validate their structure.
  const base::Value* nodes = heaps_v2->FindPath({"maps", "nodes"});
  const base::Value* strings = heaps_v2->FindPath({"maps", "strings"});
  ASSERT_TRUE(nodes);
  ASSERT_TRUE(strings);

  // Validate the strings table.
  EXPECT_EQ(5u, strings->GetList().size());
  int sid_unknown = GetIdFromStringTable(strings, "[unknown]");
  int sid_1234 = GetIdFromStringTable(strings, "pc:1234");
  int sid_5678 = GetIdFromStringTable(strings, "pc:5678");
  int sid_9012 = GetIdFromStringTable(strings, "pc:9012");
  int sid_9013 = GetIdFromStringTable(strings, "pc:9013");
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
  EXPECT_EQ(4u, nodes->GetList().size());
  int id0 = GetNodeWithNameID(nodes, sid_1234);
  int id1 = GetNodeWithNameID(nodes, sid_5678);
  int id2 = GetNodeWithNameID(nodes, sid_9012);
  int id3 = GetNodeWithNameID(nodes, sid_9013);
  EXPECT_NE(-1, id0);
  EXPECT_NE(-1, id1);
  EXPECT_NE(-1, id2);
  EXPECT_NE(-1, id3);
  EXPECT_TRUE(IsBacktraceInList(nodes, id0, kNoParent));
  EXPECT_TRUE(IsBacktraceInList(nodes, id1, id0));
  EXPECT_TRUE(IsBacktraceInList(nodes, id2, id0));
  EXPECT_TRUE(IsBacktraceInList(nodes, id3, id2));

  // Retrieve the allocations and validate their structure.
  const base::Value* counts =
      heaps_v2->FindPath({"allocators", "malloc", "counts"});
  const base::Value* types =
      heaps_v2->FindPath({"allocators", "malloc", "types"});
  const base::Value* sizes =
      heaps_v2->FindPath({"allocators", "malloc", "sizes"});
  const base::Value* backtraces =
      heaps_v2->FindPath({"allocators", "malloc", "nodes"});

  ASSERT_TRUE(counts);
  ASSERT_TRUE(types);
  ASSERT_TRUE(sizes);
  ASSERT_TRUE(backtraces);

  // Counts should be a list of two items, a 1 and a 2. The two matching 20-byte
  // allocations should be coalesced to produce the 2.
  EXPECT_EQ(2u, counts->GetList().size());
  EXPECT_EQ(2u, types->GetList().size());
  EXPECT_EQ(2u, sizes->GetList().size());

  int node1 = GetOffsetForBacktraceID(backtraces, id1);
  int node3 = GetOffsetForBacktraceID(backtraces, id3);
  EXPECT_NE(-1, node1);
  EXPECT_NE(-1, node3);

  // Validate node allocated with |stack1|.
  EXPECT_EQ(2, counts->GetList()[node1].GetInt());
  EXPECT_EQ(0, types->GetList()[node1].GetInt());
  EXPECT_EQ(40, sizes->GetList()[node1].GetInt());
  EXPECT_EQ(id1, backtraces->GetList()[node1].GetInt());

  // Validate node allocated with |stack2|.
  EXPECT_EQ(2, counts->GetList()[node3].GetInt());
  EXPECT_EQ(0, types->GetList()[node3].GetInt());
  EXPECT_EQ(44, sizes->GetList()[node3].GetInt());
  EXPECT_EQ(id3, backtraces->GetList()[node3].GetInt());

  // Validate that the partition alloc one got through.
  counts = heaps_v2->FindPath({"allocators", "partition_alloc", "counts"});
  types = heaps_v2->FindPath({"allocators", "partition_alloc", "types"});
  sizes = heaps_v2->FindPath({"allocators", "partition_alloc", "sizes"});
  backtraces = heaps_v2->FindPath({"allocators", "partition_alloc", "nodes"});

  ASSERT_TRUE(counts);
  ASSERT_TRUE(types);
  ASSERT_TRUE(sizes);
  ASSERT_TRUE(backtraces);

  // There should just be one entry for the partition_alloc allocation.
  EXPECT_EQ(1u, counts->GetList().size());
  EXPECT_EQ(1u, types->GetList().size());
  EXPECT_EQ(1u, sizes->GetList().size());
}

// GetProcessMemoryMaps iterates through every memory region, making allocations
// for each one. ASAN will potentially, for each allocation, make memory
// regions. This will cause the test to time out.
#if !defined(ADDRESS_SANITIZER)
TEST(ProfilingJsonExporterTest, MemoryMaps) {
  ExportParams params;
  params.maps = memory_instrumentation::OSMetrics::GetProcessMemoryMaps(
      base::Process::Current().Pid());
  ASSERT_GT(params.maps.size(), 2u);

  std::string json = ExportMemoryMapsAndV2StackTraceToJSON(&params);

  // JSON should parse.
  base::JSONReader reader(base::JSON_PARSE_RFC);
  std::unique_ptr<base::Value> root = reader.ReadToValueDeprecated(json);
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, reader.error_code())
      << reader.GetErrorMessage();
  ASSERT_TRUE(root);

  const base::Value* region = FindFirstRegionWithAnyName(root.get());
  ASSERT_TRUE(region) << "Array contains no named vm regions";

  const base::Value* start_address =
      region->FindKeyOfType("sa", base::Value::Type::STRING);
  ASSERT_TRUE(start_address);
  EXPECT_NE(start_address->GetString(), "");
  EXPECT_NE(start_address->GetString(), "0");

  const base::Value* size =
      region->FindKeyOfType("sz", base::Value::Type::STRING);
  ASSERT_TRUE(size);
  EXPECT_NE(size->GetString(), "");
  EXPECT_NE(size->GetString(), "0");
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
  base::JSONReader reader(base::JSON_PARSE_RFC);
  std::unique_ptr<base::Value> root = reader.ReadToValueDeprecated(json);
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, reader.error_code())
      << reader.GetErrorMessage();
  ASSERT_TRUE(root);

  // Retrieve the allocations.
  const base::Value* heaps_v2 = root->FindKey("heaps_v2");
  ASSERT_TRUE(heaps_v2);

  const base::Value* counts =
      heaps_v2->FindPath({"allocators", "partition_alloc", "counts"});
  ASSERT_TRUE(counts);
  const base::Value* types =
      heaps_v2->FindPath({"allocators", "partition_alloc", "types"});
  ASSERT_TRUE(types);

  const auto& counts_list = counts->GetList();
  const auto& types_list = types->GetList();

  // There should be three allocations, two coalesced ones, one with unique
  // context, and one with no context.
  EXPECT_EQ(3u, counts_list.size());
  EXPECT_EQ(3u, types_list.size());

  const base::Value* types_map = heaps_v2->FindPath({"maps", "types"});
  ASSERT_TRUE(types_map);
  const base::Value* strings = heaps_v2->FindPath({"maps", "strings"});
  ASSERT_TRUE(strings);

  // Reconstruct the map from type id to string.
  std::map<int, std::string> type_to_string;
  for (const auto& type : types_map->GetList()) {
    const base::Value* id =
        type.FindKeyOfType("id", base::Value::Type::INTEGER);
    ASSERT_TRUE(id);
    const base::Value* name_sid =
        type.FindKeyOfType("name_sid", base::Value::Type::INTEGER);
    ASSERT_TRUE(name_sid);

    type_to_string[id->GetInt()] =
        GetStringFromStringTable(strings, name_sid->GetInt());
  }

  // Track the three entries we have down to what we expect. The order is not
  // defined so this is relatively complex to do.
  bool found_double_context = false;  // Allocations sharing the same context.
  bool found_single_context = false;  // Allocation with unique context.
  bool found_no_context = false;      // Allocation with no context.
  for (size_t i = 0; i < types_list.size(); i++) {
    const auto& found = type_to_string.find(types_list[i].GetInt());
    ASSERT_NE(type_to_string.end(), found);
    if (found->second == context_str1) {
      // Context string matches the one with two allocations.
      ASSERT_FALSE(found_double_context);
      found_double_context = true;
      ASSERT_EQ(2, counts_list[i].GetInt());
    } else if (found->second == context_str2) {
      // Context string matches the one with one allocation.
      ASSERT_FALSE(found_single_context);
      found_single_context = true;
      ASSERT_EQ(1, counts_list[i].GetInt());
    } else if (found->second == "[unknown]") {
      // Context string for the one with no context.
      ASSERT_FALSE(found_no_context);
      found_no_context = true;
      ASSERT_EQ(1, counts_list[i].GetInt());
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
  base::JSONReader json_reader(base::JSON_PARSE_RFC);
  base::Optional<base::Value> result = json_reader.ReadToValue(json);
  ASSERT_TRUE(result.has_value()) << json_reader.GetErrorMessage();

  // Validate the allocators summary.
  const base::Value* malloc_summary =
      result.value().FindPath({"allocators", "malloc"});
  ASSERT_TRUE(malloc_summary);
  const base::Value* malloc_size =
      malloc_summary->FindPath({"attrs", "size", "value"});
  ASSERT_TRUE(malloc_size);
  EXPECT_EQ("9876543210", malloc_size->GetString());
  const base::Value* malloc_virtual_size =
      malloc_summary->FindPath({"attrs", "virtual_size", "value"});
  ASSERT_TRUE(malloc_virtual_size);
  EXPECT_EQ("9876543210", malloc_virtual_size->GetString());

  // Validate allocators details.
  // heaps_v2.allocators.malloc.sizes.reduce((a,s)=>a+s,0).
  const base::Value* malloc =
      result.value().FindPath({"heaps_v2", "allocators", "malloc"});
  const base::Value* malloc_sizes = malloc->FindKey("sizes");
  EXPECT_EQ(1u, malloc_sizes->GetList().size());
  EXPECT_EQ(0x9876543210ul, malloc_sizes->GetList()[0].GetDouble());
}
#endif

}  // namespace heap_profiling
