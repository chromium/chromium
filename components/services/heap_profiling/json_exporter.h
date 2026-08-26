// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_JSON_EXPORTER_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_JSON_EXPORTER_H_

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/services/heap_profiling/allocation.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_service.mojom.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace heap_profiling {

// Configuration passed to the export functions because they take many
// arguments. All parameters must be set.
struct ExportParams {
  ExportParams();
  ~ExportParams();

  // Allocation events to export.
  AllocationMap allocs;

  // VM map of all regions in the process.
  std::vector<memory_instrumentation::mojom::VmRegionPtr> maps;

  // Map from context string to context ID. A reverse-mapping will tell you
  // what the context_id in the allocation mean.
  std::map<std::string, int> context_map;

  // Some addresses represent strings rather than instruction pointers.
  // The strings are assumed to already be escaped for JSON.
  std::unordered_map<uint64_t, std::string> mapped_strings;

  // The type of browser [browser, renderer, gpu] that is being heap-dumped.
  mojom::ProcessType process_type = mojom::ProcessType::OTHER;

  // Whether or not the paths should be stripped from mapped files. Doing so
  // anonymizes the trace, since the paths could potentially contain a username.
  // However, it prevents symbolization of locally built instances of Chrome.
  bool strip_path_from_mapped_files = false;

  // The heaps_v2 trace format requires that ids are unique across heap dumps in
  // a single trace. This class is currently stateless, and does not know
  // whether a heap dump will be in a trace with other heap dumps. To work
  // around this, just make all IDs unique. The parameter is an input parameter
  // that tells the exporter which ID to start from. It is also an output
  // parameter, and tells the caller the next unused ID.
  // See https://crbug.com/808066.
  size_t next_id = 1;
};

// Creates a JSON string representing a JSON dictionary that contains memory
// maps and v2 format stack traces.
std::string ExportMemoryMapsAndV2StackTraceToJSON(ExportParams* params);

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_JSON_EXPORTER_H_
