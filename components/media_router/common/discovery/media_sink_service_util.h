// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_SERVICE_UTIL_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_SERVICE_UTIL_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/media_router/common/discovery/media_sink_internal.h"

namespace base {
class SequencedTaskRunner;
}

namespace media_router {

using OnSinksDiscoveredCallback =
    base::RepeatingCallback<void(std::vector<MediaSinkInternal>)>;

// Called when a new sink becomes available or an available sink becomes
// unavailable for |app_name|.
// |app_name|: app name on receiver device (e.g. YouTube)
// |available_sinks|: list of currently available sinks for |app_name|
using OnAvailableSinksUpdatedCallback = base::RepeatingCallback<void(
    const std::string& app_name,
    std::vector<MediaSinkInternal> available_sinks)>;

// Runs |sinks_discovered_cb| with |sinks| on |task_runner|.
void RunSinksDiscoveredCallbackOnSequence(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const OnSinksDiscoveredCallback& callback,
    std::vector<MediaSinkInternal> sinks);

void RunAvailableSinksUpdatedCallbackOnSequence(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const OnAvailableSinksUpdatedCallback& callback,
    const std::string& app_name,
    std::vector<MediaSinkInternal> available_sinks);
}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_SERVICE_UTIL_H_
