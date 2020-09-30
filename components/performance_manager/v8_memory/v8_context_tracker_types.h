// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines various types that are used by the V8ContextTracker. Note that all of
// these will be migrated to mojo types once the browser-side implementation is
// complete and tested.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_TYPES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_TYPES_H_

#include <string>

#include "base/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {
namespace v8_memory {

// Stores information about an iframe element from the point of view of the
// document that hosts the iframe. Explicitly allow copy and assign. This is
// used in the performance.measureMemory API.
struct IframeAttributionData {
  IframeAttributionData();
  IframeAttributionData(const IframeAttributionData&);
  IframeAttributionData& operator=(const IframeAttributionData&);
  ~IframeAttributionData();

  static IframeAttributionData Create(const base::Optional<std::string>& id,
                                      const base::Optional<std::string>& src);

  base::Optional<std::string> id;
  // We don't use a GURL because we don't need to parse this, or otherwise use
  // it as an URL, and GURL has a very large memory footprint.
  base::Optional<std::string> src;
};

// Identifies a V8Context type. Note that this roughly corresponds to the
// world types defined in blink, but with some simplifications.
enum class V8ContextWorldType {
  // The main world, corresponding to a frame / document.
  kMain,
  // Corresponds to the main world of a worker or worklet.
  kWorkerOrWorklet,
  // Corresponds to an extension.
  kExtension,
  // Corresponds to a non-extension isolated world.
  kIsolated,
  // Corresponds to the devtools inspector. Will not have a human readable
  // name or a stable id.
  kInspector,
  // Corresponds to the regexp world. This world is unique in that it is per
  // v8::Isolate, and not associated with any individual execution context.
  // Will not have a human-readable name or stable id.
  kRegExp,
};

// Information describing a V8 Context. Explicitly allow copy and assign. This
// is used in IPC related to the performance.measureMemory API.
struct V8ContextDescription {
  V8ContextDescription();
  V8ContextDescription(const V8ContextDescription&);
  V8ContextDescription& operator=(const V8ContextDescription&);
  ~V8ContextDescription();

  static V8ContextDescription Create(
      blink::V8ContextToken token,
      V8ContextWorldType world_type,
      const base::Optional<std::string>& world_name,
      const base::Optional<blink::ExecutionContextToken>&
          execution_context_token);

  // The unique token that names this world.
  blink::V8ContextToken token;
  // The type of this world.
  V8ContextWorldType world_type;
  // Identifies this world. Only set for extension and isolated worlds. For
  // extension worlds this corresponds to the stable extension ID. For other
  // isolated worlds this is a human-readable description.
  base::Optional<std::string> world_name;
  // The identity of the execution context that this V8Context is associated
  // with. This is specified for all world types, except kRegExp worlds.
  base::Optional<blink::ExecutionContextToken> execution_context_token;
};

}  // namespace v8_memory
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_TYPES_H_
