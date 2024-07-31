// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/heap_profiling/json_exporter.h"

#include <inttypes.h>

#include <map>
#include <unordered_map>

#include "base/containers/adapters.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_traced_value.h"

namespace heap_profiling {
namespace {

// Maps strings to integers for the JSON string table.
using StringTable = std::unordered_map<std::string, int>;

// Maps allocation site to node_id of the top frame.
using AllocationToNodeId = std::unordered_map<const AllocationSite*, int>;

constexpr int kAllocatorCount = static_cast<int>(AllocatorType::kMaxValue) + 1;

struct BacktraceNode {
  BacktraceNode(int string_id, int parent)
      : string_id_(string_id), parent_(parent) {}

  static constexpr int kNoParent = -1;

  int string_id() const { return string_id_; }
  int parent() const { return parent_; }

  bool operator<(const BacktraceNode& other) const {
    return std::tie(string_id_, parent_) <
           std::tie(other.string_id_, other.parent_);
  }

 private:
  const int string_id_;
  const int parent_;  // kNoParent indicates no parent.
};

using BacktraceTable = std::map<BacktraceNode, int>;

// The hardcoded ID for having no context for an allocation.
constexpr int kUnknownTypeId = 0;

const char* StringForAllocatorType(uint32_t type) {
  switch (static_cast<AllocatorType>(type)) {
    case AllocatorType::kMalloc:
      return "malloc";
    case AllocatorType::kPartitionAlloc:
      return "partition_alloc";
    default:
      NOTREACHED_IN_MIGRATION();
      return "unknown";
  }
}

// Writes the top-level allocators section. This section is used by the tracing
// UI to show a small summary for each allocator. It's necessary as a
// placeholder to allow the stack-viewing UI to be shown.
base::Value::Dict BuildAllocatorsSummary(const AllocationMap& allocations) {
  // Aggregate stats for each allocator type.
  size_t total_size[kAllocatorCount] = {0};
  size_t total_count[kAllocatorCount] = {0};
  for (const auto& alloc_pair : allocations) {
    int index = static_cast<int>(alloc_pair.first.allocator);
    total_size[index] += alloc_pair.second.size;
    total_count[index] += alloc_pair.second.count;
  }

  base::Value::Dict result;
  for (int i = 0; i < kAllocatorCount; i++) {
    const char* alloc_type = StringForAllocatorType(i);

    // Overall sizes.
    base::Value::Dict sizes;
    sizes.Set("type", "scalar");
    sizes.Set("units", "bytes");
    sizes.Set("value", base::StringPrintf("%zx", total_size[i]));

    base::Value::Dict attrs;
    attrs.Set("virtual_size", sizes.Clone());
    attrs.Set("size", std::move(sizes));

    base::Value::Dict allocator;
    allocator.Set("attrs", std::move(attrs));
    result.Set(alloc_type, std::move(allocator));

    // Allocated objects.
    base::Value::Dict shim_allocated_objects_count;
    shim_allocated_objects_count.Set("type", "scalar");
    shim_allocated_objects_count.Set("units", "objects");
    shim_allocated_objects_count.Set("value",
                                     base::StringPrintf("%zx", total_count[i]));

    base::Value::Dict shim_allocated_objects_size;
    shim_allocated_objects_size.Set("type", "scalar");
    shim_allocated_objects_size.Set("units", "bytes");
    shim_allocated_objects_size.Set("value",
                                    base::StringPrintf("%zx", total_size[i]));

    base::Value::Dict allocated_objects_attrs;
    allocated_objects_attrs.Set("shim_allocated_objects_count",
                                std::move(shim_allocated_objects_count));
    allocated_objects_attrs.Set("shim_allocated_objects_size",
                                std::move(shim_allocated_objects_size));

    base::Value::Dict allocated_objects;
    allocated_objects.Set("attrs", std::move(allocated_objects_attrs));
    result.Set(alloc_type + std::string("/allocated_objects"),
               std::move(allocated_objects));
  }
  return result;
}

base::Value BuildMemoryMaps(const ExportParams& params) {
  base::trace_event::TracedValueJSON traced_value;
  memory_instrumentation::TracingObserverTracedValue::MemoryMapsAsValueInto(
      params.maps, &traced_value, params.strip_path_from_mapped_files);
  return std::move(*traced_value.ToBaseValue());
}

// Inserts or retrieves the ID for a string in the string table.
int AddOrGetString(const std::string& str,
                   StringTable* string_table,
                   ExportParams* params) {
  return string_table->emplace(str, params->next_id++).first->second;
}

// Processes the context information.
// Strings are added for each referenced context and a mapping between
// context IDs and string IDs is filled in for each.
void FillContextStrings(ExportParams* params,
                        StringTable* string_table,
                        std::map<int, int>* context_to_string_id_map) {
  // The context map is backwards from what we need, so iterate through the
  // whole thing and see which ones are used.
  for (const auto& context : params->context_map) {
    int string_id = AddOrGetString(context.first, string_table, params);
    context_to_string_id_map->emplace(context.second, string_id);
  }

  // Hard code a string for the unknown context type.
  context_to_string_id_map->emplace(
      kUnknownTypeId, AddOrGetString("[unknown]", string_table, params));
}

int AddOrGetBacktraceNode(BacktraceNode node,
                          BacktraceTable* backtrace_table,
                          ExportParams* params) {
  return backtrace_table->emplace(std::move(node), params->next_id++)
      .first->second;
}

// Returns the index into nodes of the node to reference for this stack. That
// node will reference its parent node, etc. to allow the full stack to
// be represented.
int AppendBacktraceStrings(const AllocationSite& alloc,
                           BacktraceTable* backtrace_table,
                           StringTable* string_table,
                           ExportParams* params) {
  int parent = BacktraceNode::kNoParent;
  // Addresses must be outputted in reverse order.
  for (const Address addr : base::Reversed(alloc.stack)) {
    int sid;
    auto it = params->mapped_strings.find(addr);
    if (it != params->mapped_strings.end()) {
      sid = AddOrGetString(it->second, string_table, params);
    } else {
      char buf[20];
      snprintf(buf, sizeof(buf), "pc:%" PRIx64, addr);
      sid = AddOrGetString(buf, string_table, params);
    }
    parent = AddOrGetBacktraceNode(BacktraceNode(sid, parent), backtrace_table,
                                   params);
  }
  return parent;  // Last item is the top of this stack.
}

base::Value::List BuildStrings(const StringTable& string_table) {
  base::Value::List strings;
  strings.reserve(string_table.size());
  for (const auto& string_pair : string_table) {
    base::Value::Dict item;
    item.Set("id", string_pair.second);
    item.Set("string", string_pair.first);
    strings.Append(std::move(item));
  }
  return strings;
}

base::Value::List BuildMapNodes(const BacktraceTable& nodes) {
  base::Value::List items;
  items.reserve(nodes.size());
  for (const auto& node_pair : nodes) {
    base::Value::Dict item;
    item.Set("id", node_pair.second);
    item.Set("name_sid", node_pair.first.string_id());
    if (node_pair.first.parent() != BacktraceNode::kNoParent)
      item.Set("parent", node_pair.first.parent());
    items.Append(std::move(item));
  }
  return items;
}

base::Value::List BuildTypeNodes(const std::map<int, int>& type_to_string) {
  base::Value::List items;
  items.reserve(type_to_string.size());
  for (const auto& pair : type_to_string) {
    base::Value::Dict item;
    item.Set("id", pair.first);
    item.Set("name_sid", pair.second);
    items.Append(std::move(item));
  }
  return items;
}

base::Value::Dict BuildAllocations(const AllocationMap& allocations,
                                   const AllocationToNodeId& alloc_to_node_id) {
  base::Value::List counts[kAllocatorCount];
  base::Value::List sizes[kAllocatorCount];
  base::Value::List types[kAllocatorCount];
  base::Value::List nodes[kAllocatorCount];

  for (const auto& alloc : allocations) {
    int allocator = static_cast<int>(alloc.first.allocator);
    // We use double to store size and count, as it can precisely represent
    // values up to 2^52 ~ 4.5 petabytes.
    counts[allocator].Append(static_cast<double>(round(alloc.second.count)));
    sizes[allocator].Append(static_cast<double>(alloc.second.size));
    types[allocator].Append(alloc.first.context_id);
    nodes[allocator].Append(alloc_to_node_id.at(&alloc.first));
  }

  base::Value::Dict allocators;
  for (uint32_t i = 0; i < kAllocatorCount; i++) {
    base::Value::Dict allocator;
    allocator.Set("counts", std::move(counts[i]));
    allocator.Set("sizes", std::move(sizes[i]));
    allocator.Set("types", std::move(types[i]));
    allocator.Set("nodes", std::move(nodes[i]));
    allocators.Set(StringForAllocatorType(i), std::move(allocator));
  }
  return allocators;
}

}  // namespace

ExportParams::ExportParams() = default;
ExportParams::~ExportParams() = default;

std::string ExportMemoryMapsAndV2StackTraceToJSON(ExportParams* params) {
  base::Value::Dict result;

  result.Set("level_of_detail", "detailed");
  result.Set("process_mmaps", BuildMemoryMaps(*params));
  result.Set("allocators", BuildAllocatorsSummary(params->allocs));

  base::Value::Dict heaps_v2;

  // Output Heaps_V2 format version. Currently "1" is the only valid value.
  heaps_v2.Set("version", 1);

  // Put all required context strings in the string table and generate a
  // mapping from allocation context_id to string ID.
  StringTable string_table;
  std::map<int, int> context_to_string_id_map;
  FillContextStrings(params, &string_table, &context_to_string_id_map);

  AllocationToNodeId alloc_to_node_id;
  BacktraceTable nodes;
  // For each backtrace, converting the string for the stack entry to string
  // IDs. The backtrace -> node ID will be filled in at this time.
  for (const auto& alloc : params->allocs) {
    int node_id =
        AppendBacktraceStrings(alloc.first, &nodes, &string_table, params);
    alloc_to_node_id.emplace(&alloc.first, node_id);
  }

  // Maps section.
  base::Value::Dict maps;
  maps.Set("strings", BuildStrings(string_table));
  maps.Set("nodes", BuildMapNodes(nodes));
  maps.Set("types", BuildTypeNodes(context_to_string_id_map));
  heaps_v2.Set("maps", std::move(maps));

  heaps_v2.Set("allocators",
               BuildAllocations(params->allocs, alloc_to_node_id));

  result.Set("heaps_v2", std::move(heaps_v2));

  std::string result_json;
  bool ok = base::JSONWriter::WriteWithOptions(
      result, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
      &result_json);
  DCHECK(ok);
  return result_json;
}

}  // namespace heap_profiling
