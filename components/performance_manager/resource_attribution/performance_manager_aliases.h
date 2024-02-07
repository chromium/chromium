// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_PERFORMANCE_MANAGER_ALIASES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_PERFORMANCE_MANAGER_ALIASES_H_

#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/render_process_host_id.h"

// This header imports performance_manager symbols that are commonly used in the
// resource attribution implementation into the resource_attribution namespace,
// for convenience. It should never be included in header files that can be
// included outside components/performance_manager/resource_attribution.

namespace performance_manager {

class FrameNode;
class FrameNodeImpl;
class Graph;
class GraphImpl;
class PageNode;
class PageNodeImpl;
class PerformanceManager;
class ProcessNode;
class ProcessNodeImpl;
class WorkerNode;
class WorkerNodeImpl;

}  // namespace performance_manager

namespace resource_attribution {

using BrowserChildProcessHostId =
    performance_manager::BrowserChildProcessHostId;
using FrameNode = performance_manager::FrameNode;
using FrameNodeImpl = performance_manager::FrameNodeImpl;
using Graph = performance_manager::Graph;
using GraphImpl = performance_manager::GraphImpl;
using PageNode = performance_manager::PageNode;
using PageNodeImpl = performance_manager::PageNodeImpl;
using PerformanceManager = performance_manager::PerformanceManager;
using ProcessNode = performance_manager::ProcessNode;
using ProcessNodeImpl = performance_manager::ProcessNodeImpl;
using RenderProcessHostId = performance_manager::RenderProcessHostId;
using WorkerNode = performance_manager::WorkerNode;
using WorkerNodeImpl = performance_manager::WorkerNodeImpl;

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_PERFORMANCE_MANAGER_ALIASES_H_
