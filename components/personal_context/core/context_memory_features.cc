// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/context_memory_features.h"

#include <string_view>

#include "components/personal_context/proto/context_memory_service.pb.h"

namespace personal_context {

// LINT.IfChange(ContextMemoryFeature)
std::string_view GetStringNameForContextMemoryFeature(
    proto::ContextMemoryFeature feature) {
  switch (feature) {
    case proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL:
      return "AmbientAutofill";
    case proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY:
      return "AtMemory";
    case proto::CONTEXT_MEMORY_FEATURE_UNSPECIFIED:
      return "Unspecified";
    case proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS:
      return "AutoTodos";
    case proto::CONTEXT_MEMORY_FEATURE_SMART_SEARCH:
      return "SmartSearch";
    case proto::ContextMemoryFeature_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::ContextMemoryFeature_INT_MAX_SENTINEL_DO_NOT_USE_:
      return "";
      // Must be in sync with ContextMemoryFeature variant in
      // personal_context/histograms.xml for metric recording.
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/personal_context/histograms.xml:ContextMemoryFeature)

}  // namespace personal_context
