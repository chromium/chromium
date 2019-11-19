// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/json_exporter.h"

#include <inttypes.h>
#include <map>
#include <unordered_map>

#include "base/containers/adapters.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"

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
    case AllocatorType::kOilpan:
      return "blink_gc";
    default:
      NOTREACHED();
      return "unknown";
  }
}

// Writes the top-level allocators section. This section is used by the tracing
// UI to show a small summary for each allocator. It's necessary as a
// placeholder to allow the stack-viewing UI to be shown.
base::Value BuildAllocatorsSummary(const AllocationMap& allocations) {
  // Aggregate stats for each allocator type.
  size_t total_size[kAllocatorCount] = {0};
  size_t total_count[kAllocatorCount] = {0};
  for (const auto& alloc_pair : allocations) {
    int index = static_cast<int>(alloc_pair.first.allocator);
    total_size[index] += alloc_pair.second.size;
    total_count[index] += alloc_pair.second.count;
  }

  base::Value result(base::Value::Type::DICTIONARY);
  for (int i = 0; i < kAllocatorCount; i++) {
    const char* alloc_type = StringForAllocatorType(i);

    // Overall sizes.
    base::Value sizes(base::Value::Type::DICTIONARY);
    sizes.SetStringKey("type", "scalar");
    sizes.SetStringKey("units", "bytes");
    sizes.SetStringKey("value", base::StringPrintf("%zx", total_size[i]));

    base::Value attrs(base::Value::Type::DICTIONARY);
    attrs.SetKey("virtual_size", sizes.Clone());
    attrs.SetKey("size", std::move(sizes));

    base::Value allocator(base::Value::Type::DICTIONARY);
    allocator.SetKey("attrs", std::move(attrs));
    result.SetKey(alloc_type, std::move(allocator));

    // Allocated objects.
    base::Value shim_allocated_objects_count(base::Value::Type::DICTIONARY);
    shim_allocated_objects_count.SetStringKey("type", "scalar");
    shim_allocated_objects_count.SetStringKey("units", "objects");
    shim_allocated_objects_count.SetStringKey(
        "value", base::StringPrintf("%zx", total_count[i]));

    base::Value shim_allocated_objects_size(base::Value::Type::DICTIONARY);
    shim_allocated_objects_size.SetStringKey("type", "scalar");
    shim_allocated_objects_size.SetStringKey("units", "bytes");
    shim_allocated_objects_size.SetStringKey(
        "value", base::StringPrintf("%zx", total_size[i]));

    base::Value allocated_objects_attrs(base::Value::Type::DICTIONARY);
    allocated_objects_attrs.SetKey("shim_allocated_objects_count",
                                   std::move(shim_allocated_objects_count));
    allocated_objects_attrs.SetKey("shim_allocated_objects_size",
                                   std::move(shim_allocated_objects_size));

    base::Value allocated_objects(base::Value::Type::DICTIONARY);
    allocated_objects.SetKey("attrs", std::move(allocated_objects_attrs));
    result.SetKey(alloc_type + std::string("/allocated_objects"),
                  std::move(allocated_objects));
  }
  return result;
}

base::Value BuildMemoryMaps(const ExportParams& params) {
  base::trace_event::TracedValue traced_value(0, /* force_json */ true);
  memory_instrumentation::TracingObserver::MemoryMapsAsValueInto(
      params.maps, &traced_value, params.strip_path_from_mapped_files);
  return traced_value.ToBaseValue()->Clone();
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

base::Value BuildStrings(const StringTable& string_table) {
  base::Value::ListStorage strings;
  strings.reserve(string_table.size());
  for (const auto& string_pair : string_table) {
    base::Value item(base::Value::Type::DICTIONARY);
    item.SetIntKey("id", string_pair.second);
    item.SetStringKey("string", string_pair.first);
    strings.push_back(std::move(item));
  }
  return base::Value(std::move(strings));
}

base::Value BuildMapNodes(const BacktraceTable& nodes) {
  base::Value::ListStorage items;
  items.reserve(nodes.size());
  for (const auto& node_pair : nodes) {
    base::Value item(base::Value::Type::DICTIONARY);
    item.SetIntKey("id", node_pair.second);
    item.SetIntKey("name_sid", node_pair.first.string_id());
    if (node_pair.first.parent() != BacktraceNode::kNoParent)
      item.SetIntKey("parent", node_pair.first.parent());
    items.push_back(std::move(item));
  }
  return base::Value(std::move(items));
}

base::Value BuildTypeNodes(const std::map<int, int>& type_to_string) {
  base::Value::ListStorage items;
  items.reserve(type_to_string.size());
  for (const auto& pair : type_to_string) {
    base::Value item(base::Value::Type::DICTIONARY);
    item.SetIntKey("id", pair.first);
    item.SetIntKey("name_sid", pair.second);
    items.push_back(std::move(item));
  }
  return base::Value(std::move(items));
}

base::Value BuildAllocations(const AllocationMap& allocations,
                             const AllocationToNodeId& alloc_to_node_id) {
  base::Value::ListStorage counts[kAllocatorCount];
  base::Value::ListStorage sizes[kAllocatorCount];
  base::Value::ListStorage types[kAllocatorCount];
  base::Value::ListStorage nodes[kAllocatorCount];

  for (const auto& alloc : allocations) {
    int allocator = static_cast<int>(alloc.first.allocator);
    // We use double to store size and count, as it can precisely represent
    // values up to 2^52 ~ 4.5 petabytes.
    counts[allocator].push_back(
        base::Value(static_cast<double>(alloc.second.count)));
    sizes[allocator].push_back(
        base::Value(static_cast<double>(alloc.second.size)));
    types[allocator].push_back(base::Value(alloc.first.context_id));
    nodes[allocator].push_back(base::Value(alloc_to_node_id.at(&alloc.first)));
  }

  base::Value allocators(base::Value::Type::DICTIONARY);
  for (uint32_t i = 0; i < kAllocatorCount; i++) {
    base::Value allocator(base::Value::Type::DICTIONARY);
    allocator.SetKey("counts", base::Value(std::move(counts[i])));
    allocator.SetKey("sizes", base::Value(std::move(sizes[i])));
    allocator.SetKey("types", base::Value(std::move(types[i])));
    allocator.SetKey("nodes", base::Value(std::move(nodes[i])));
    allocators.SetKey(StringForAllocatorType(i), std::move(allocator));
  }
  return allocators;
}

}  // namespace

ExportParams::ExportParams() = default;
ExportParams::~ExportParams() = default;

std::string ExportMemoryMapsAndV2StackTraceToJSON(ExportParams* params) {
  base::Value result(base::Value::Type::DICTIONARY);

  result.SetStringKey("level_of_detail", "detailed");
  result.SetKey("process_mmaps", BuildMemoryMaps(*params));
  result.SetKey("allocators", BuildAllocatorsSummary(params->allocs));

  base::Value heaps_v2(base::Value::Type::DICTIONARY);

  // Output Heaps_V2 format version. Currently "1" is the only valid value.
  heaps_v2.SetIntKey("version", 1);

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
  base::Value maps(base::Value::Type::DICTIONARY);
  maps.SetKey("strings", BuildStrings(string_table));
  maps.SetKey("nodes", BuildMapNodes(nodes));
  maps.SetKey("types", BuildTypeNodes(context_to_string_id_map));
  heaps_v2.SetKey("maps", std::move(maps));

  heaps_v2.SetKey("allocators",
                  BuildAllocations(params->allocs, alloc_to_node_id));

  result.SetKey("heaps_v2", std::move(heaps_v2));

  std::string result_json;
  bool ok = base::JSONWriter::WriteWithOptions(
      result, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
      &result_json);
  DCHECK(ok);
  return result_json;
}

}  // namespace heap_profiling
