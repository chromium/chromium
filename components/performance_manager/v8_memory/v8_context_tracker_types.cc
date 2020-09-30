// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker_types.h"

namespace performance_manager {
namespace v8_memory {

////////////////////////////////////////////////////////////////////////////////
// IframeAttributionData implementation:

IframeAttributionData::IframeAttributionData() = default;

IframeAttributionData::IframeAttributionData(const IframeAttributionData&) =
    default;

IframeAttributionData& IframeAttributionData::operator=(
    const IframeAttributionData&) = default;

IframeAttributionData::~IframeAttributionData() = default;

// static
IframeAttributionData IframeAttributionData::Create(
    const base::Optional<std::string>& id,
    const base::Optional<std::string>& src) {
  IframeAttributionData data;
  data.id = id;
  data.src = src;
  return data;
}

////////////////////////////////////////////////////////////////////////////////
// V8ContextDescription implementation:

V8ContextDescription::V8ContextDescription() = default;

V8ContextDescription::V8ContextDescription(const V8ContextDescription&) =
    default;

V8ContextDescription& V8ContextDescription::operator=(
    const V8ContextDescription&) = default;

V8ContextDescription::~V8ContextDescription() = default;

// static
V8ContextDescription V8ContextDescription::Create(
    blink::V8ContextToken token,
    V8ContextWorldType world_type,
    const base::Optional<std::string>& world_name,
    const base::Optional<blink::ExecutionContextToken>&
        execution_context_token) {
  V8ContextDescription desc;
  desc.token = token;
  desc.world_type = world_type;
  desc.world_name = world_name;
  desc.execution_context_token = execution_context_token;
  return desc;
}

}  // namespace v8_memory
}  // namespace performance_manager
