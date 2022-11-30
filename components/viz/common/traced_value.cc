// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/traced_value.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"

namespace viz {

void TracedValue::AppendIDRef(const void* id,
                              base::trace_event::TracedValue* state) {
  state->BeginDictionary();
  state->SetString("id_ref", base::StringPrintf("%p", id));
  state->EndDictionary();
}

void TracedValue::SetIDRef(const void* id,
                           base::trace_event::TracedValue* state,
                           const char* name) {
  state->BeginDictionary(name);
  state->SetString("id_ref", base::StringPrintf("%p", id));
  state->EndDictionary();
}

void TracedValue::MakeDictIntoImplicitSnapshot(
    base::trace_event::TracedValue* dict,
    const char* object_name,
    const void* id) {
  dict->SetString("id", base::StringPrintf("%s/%p", object_name, id));
}

void TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
    const char* category,
    base::trace_event::TracedValue* dict,
    const char* object_name,
    const void* id) {
  dict->SetString("cat", category);
  MakeDictIntoImplicitSnapshot(dict, object_name, id);
}

void TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
    const char* category,
    base::trace_event::TracedValue* dict,
    const char* object_base_type_name,
    const char* object_name,
    const void* id) {
  dict->SetString("cat", category);
  dict->SetString("base_type", object_base_type_name);
  MakeDictIntoImplicitSnapshot(dict, object_name, id);
}

}  // namespace viz
