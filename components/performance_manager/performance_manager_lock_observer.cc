// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_lock_observer.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/browser/lock_observer.h"

namespace performance_manager {

namespace {

void SetIsHoldingWebLock(int render_process_id,
                         int render_frame_id,
                         bool is_holding_web_lock,
                         GraphImpl* graph) {
  FrameNodeImpl* frame_node =
      graph->GetFrameNodeById(render_process_id, render_frame_id);
  if (frame_node)
    frame_node->SetIsHoldingWebLock(is_holding_web_lock);
}

void SetIsHoldingIndexedDBConnections(int render_process_id,
                                      int render_frame_id,
                                      bool is_holding_indexed_db_connection,
                                      GraphImpl* graph) {
  FrameNodeImpl* frame_node =
      graph->GetFrameNodeById(render_process_id, render_frame_id);
  // TODO(https://crbug.com/980533): Rename
  // FrameNodeImpl::SetIsHoldingIndexedDBLock() to
  // SetIsHoldingIndexedDBConnections().
  if (frame_node)
    frame_node->SetIsHoldingIndexedDBLock(is_holding_indexed_db_connection);
}

}  // namespace

PerformanceManagerLockObserver::PerformanceManagerLockObserver() = default;

PerformanceManagerLockObserver::~PerformanceManagerLockObserver() {
  // TODO(https://crbug.com/1013760): DCHECK that this happens after ThreadPool
  // shutdown.
}

void PerformanceManagerLockObserver::OnFrameStartsHoldingWebLocks(
    int render_process_id,
    int render_frame_id) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&SetIsHoldingWebLock, render_process_id,
                                render_frame_id, true));
}

void PerformanceManagerLockObserver::OnFrameStopsHoldingWebLocks(
    int render_process_id,
    int render_frame_id) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&SetIsHoldingWebLock, render_process_id,
                                render_frame_id, false));
}

void PerformanceManagerLockObserver::OnFrameStartsHoldingIndexedDBConnections(
    int render_process_id,
    int render_frame_id) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&SetIsHoldingIndexedDBConnections,
                                render_process_id, render_frame_id, true));
}

void PerformanceManagerLockObserver::OnFrameStopsHoldingIndexedDBConnections(
    int render_process_id,
    int render_frame_id) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&SetIsHoldingIndexedDBConnections,
                                render_process_id, render_frame_id, false));
}

}  // namespace performance_manager
