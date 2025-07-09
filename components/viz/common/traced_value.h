// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_TRACED_VALUE_H_
#define COMPONENTS_VIZ_COMMON_TRACED_VALUE_H_

#include <cstdint>

#include "components/viz/common/viz_common_export.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace viz {

class VIZ_COMMON_EXPORT TracedValue {
 public:
  // Helper class to safely convert void* to uintptr_t for tracing IDs
  class Id {
   public:
    explicit Id(const void* ptr) : value_(reinterpret_cast<uintptr_t>(ptr)) {}

   private:
    friend class TracedValue;
    const uintptr_t value_;
  };

  static void AppendIDRef(Id id, base::trace_event::TracedValue* state);
  static void SetIDRef(Id id,
                       base::trace_event::TracedValue* state,
                       const char* name);
  static void MakeDictIntoImplicitSnapshot(base::trace_event::TracedValue* dict,
                                           const char* object_name,
                                           Id id);
  static void MakeDictIntoImplicitSnapshotWithCategory(
      const char* category,
      base::trace_event::TracedValue* dict,
      const char* object_name,
      Id id);
  static void MakeDictIntoImplicitSnapshotWithCategory(
      const char* category,
      base::trace_event::TracedValue* dict,
      const char* object_base_type_name,
      const char* object_name,
      Id id);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_TRACED_VALUE_H_
