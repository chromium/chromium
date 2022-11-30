// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_TRACED_VALUE_H_
#define COMPONENTS_VIZ_COMMON_TRACED_VALUE_H_

#include "components/viz/common/viz_common_export.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace viz {

class VIZ_COMMON_EXPORT TracedValue {
 public:
  static void AppendIDRef(const void* id,
                          base::trace_event::TracedValue* array);
  static void SetIDRef(const void* id,
                       base::trace_event::TracedValue* dict,
                       const char* name);
  static void MakeDictIntoImplicitSnapshot(base::trace_event::TracedValue* dict,
                                           const char* object_name,
                                           const void* id);
  static void MakeDictIntoImplicitSnapshotWithCategory(
      const char* category,
      base::trace_event::TracedValue* dict,
      const char* object_name,
      const void* id);
  static void MakeDictIntoImplicitSnapshotWithCategory(
      const char* category,
      base::trace_event::TracedValue* dict,
      const char* object_base_type_name,
      const char* object_name,
      const void* id);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_TRACED_VALUE_H_
