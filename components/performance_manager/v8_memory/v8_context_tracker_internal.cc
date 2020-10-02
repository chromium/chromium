// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker_internal.h"

#include "base/check.h"

namespace performance_manager {
namespace v8_memory {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ExecutionContextData implementation:

ExecutionContextData::ExecutionContextData(
    ProcessData* process_data,
    const blink::ExecutionContextToken& token,
    const base::Optional<IframeAttributionData> iframe_attribution_data)
    : ExecutionContextState(token, iframe_attribution_data),
      process_data_(process_data) {}

ExecutionContextData::~ExecutionContextData() {
  DCHECK(!IsTracked());
  DCHECK(ShouldDestroy());
}

bool ExecutionContextData::IsTracked() const {
  return previous() || next();
}

bool ExecutionContextData::ShouldDestroy() const {
  return v8_context_count_ == 0 && !remote_frame_data_;
}

void ExecutionContextData::SetRemoteFrameData(
    util::PassKey<RemoteFrameData>,
    RemoteFrameData* remote_frame_data) {
  DCHECK(!remote_frame_data_);
  DCHECK(remote_frame_data);
  remote_frame_data_ = remote_frame_data;
}

bool ExecutionContextData::ClearRemoteFrameData(
    util::PassKey<RemoteFrameData>) {
  DCHECK(remote_frame_data_);
  remote_frame_data_ = nullptr;
  return ShouldDestroy();
}

void ExecutionContextData::IncrementV8ContextCount(
    util::PassKey<V8ContextData>) {
  ++v8_context_count_;
}

bool ExecutionContextData::DecrementV8ContextCount(
    util::PassKey<V8ContextData>) {
  DCHECK_LT(0u, v8_context_count_);
  --v8_context_count_;
  return ShouldDestroy();
}

////////////////////////////////////////////////////////////////////////////////
// RemoteFrameData implementation:

RemoteFrameData::RemoteFrameData(ProcessData* process_data,
                                 const blink::RemoteFrameToken& token,
                                 ExecutionContextData* execution_context_data)
    : process_data_(process_data),
      token_(token),
      execution_context_data_(execution_context_data) {
  DCHECK(process_data);
  DCHECK(execution_context_data);
  // This and the ExecutionContext *must* be cross-process.
  DCHECK_NE(process_data, execution_context_data->process_data());
  execution_context_data->SetRemoteFrameData(PassKey(), this);
}

RemoteFrameData::~RemoteFrameData() {
  DCHECK(!IsTracked());

  // If this is the last reference keeping alive a tracked ExecutionContextData,
  // then clean it up as well. Untracked ExecutionContextDatas will go out of
  // scope on their own.
  if (execution_context_data_->ClearRemoteFrameData(PassKey()) &&
      execution_context_data_->IsTracked()) {
    process_data_->data_store()->Destroy(execution_context_data_->GetToken());
  }
}

bool RemoteFrameData::IsTracked() const {
  return previous() || next();
}

////////////////////////////////////////////////////////////////////////////////
// V8ContextData implementation:

V8ContextData::V8ContextData(ProcessData* process_data,
                             const V8ContextDescription& description,
                             ExecutionContextData* execution_context_data)
    : V8ContextState(description, execution_context_data),
      process_data_(process_data) {
  DCHECK(process_data);
  if (execution_context_data) {
    // These must be same process.
    DCHECK_EQ(process_data, execution_context_data->process_data());
    execution_context_data->IncrementV8ContextCount(PassKey());
  }
}

V8ContextData::~V8ContextData() {
  DCHECK(!IsTracked());

  // If this is the last reference keeping alive a tracked ExecutionContextData,
  // then clean it up as well. Untracked ExecutionContextDatas will go out of
  // scope on their own.
  if (auto* ecd = GetExecutionContextData()) {
    if (ecd->DecrementV8ContextCount(PassKey()) && ecd->IsTracked())
      process_data_->data_store()->Destroy(ecd->GetToken());
  }
}

bool V8ContextData::IsTracked() const {
  return previous() || next();
}

ExecutionContextData* V8ContextData::GetExecutionContextData() const {
  return static_cast<ExecutionContextData*>(execution_context_state);
}

////////////////////////////////////////////////////////////////////////////////
// ProcessData implementation:

ProcessData::ProcessData(const ProcessNodeImpl* process_node)
    : data_store_(GetDataStore(process_node)) {}

ProcessData::~ProcessData() {
  DCHECK(execution_context_datas_.empty());
  DCHECK(remote_frame_datas_.empty());
  DCHECK(v8_context_datas_.empty());
}

void ProcessData::TearDown() {
  // First, remove any RemoteFrameData references owned by this ProcessData
  // that are keeping alive ExecutionContextDatas in other ProcessDatas. This
  // can cause ExecutionContextDatas to be torn down.
  while (!remote_frame_datas_.empty()) {
    auto* node = remote_frame_datas_.head();
    data_store_->Destroy(node->value()->GetToken());
  }

  // Drain the list of V8ContextTokens. This will also indirectly clean up and
  // ExecutionContextDatas that are only being kept alive by V8ContextData
  // references.
  while (!v8_context_datas_.empty()) {
    auto* node = v8_context_datas_.head();
    data_store_->Destroy(node->value()->GetToken());
  }

  // Any ExecutionContextDatas still alive are only being kept alive because of
  // RemoteFrameData references from another ProcessData. Clean those up.
  while (!execution_context_datas_.empty()) {
    auto* node = execution_context_datas_.head();
    auto* ec_data = node->value();
    auto* rfd = ec_data->remote_frame_data();
    DCHECK(rfd);
    DCHECK_EQ(0u, ec_data->v8_context_count());
    data_store_->Destroy(rfd->GetToken());
  }

  // We now expect everything to have been cleaned up.
  DCHECK(execution_context_datas_.empty());
  DCHECK(remote_frame_datas_.empty());
  DCHECK(v8_context_datas_.empty());
}

void ProcessData::Add(util::PassKey<V8ContextTrackerDataStore>,
                      ExecutionContextData* ec_data) {
  DCHECK(ec_data);
  DCHECK_EQ(this, ec_data->process_data());
  DCHECK(!ec_data->ShouldDestroy());
  DCHECK(!ec_data->IsTracked());
  execution_context_datas_.Append(ec_data);
}

void ProcessData::Add(util::PassKey<V8ContextTrackerDataStore>,
                      RemoteFrameData* rf_data) {
  DCHECK(rf_data);
  DCHECK_EQ(this, rf_data->process_data());
  DCHECK(!rf_data->IsTracked());
  remote_frame_datas_.Append(rf_data);
}

void ProcessData::Add(util::PassKey<V8ContextTrackerDataStore>,
                      V8ContextData* v8_data) {
  DCHECK(v8_data);
  DCHECK_EQ(this, v8_data->process_data());
  DCHECK(!v8_data->IsTracked());
  v8_context_datas_.Append(v8_data);
}

void ProcessData::Remove(util::PassKey<V8ContextTrackerDataStore>,
                         ExecutionContextData* ec_data) {
  DCHECK(ec_data);
  DCHECK_EQ(this, ec_data->process_data());
  DCHECK(ec_data->IsTracked());
  DCHECK(ec_data->ShouldDestroy());
  ec_data->RemoveFromList();
}

void ProcessData::Remove(util::PassKey<V8ContextTrackerDataStore>,
                         RemoteFrameData* rf_data) {
  DCHECK(rf_data);
  DCHECK_EQ(this, rf_data->process_data());
  DCHECK(rf_data->IsTracked());
  rf_data->RemoveFromList();
}

void ProcessData::Remove(util::PassKey<V8ContextTrackerDataStore>,
                         V8ContextData* v8_data) {
  DCHECK(v8_data);
  DCHECK_EQ(this, v8_data->process_data());
  DCHECK(v8_data->IsTracked());
  v8_data->RemoveFromList();
}

////////////////////////////////////////////////////////////////////////////////
// V8ContextTrackerDataStore implementation:

V8ContextTrackerDataStore::V8ContextTrackerDataStore() = default;

V8ContextTrackerDataStore::~V8ContextTrackerDataStore() {
  DCHECK(global_execution_context_datas_.empty());
  DCHECK(global_remote_frame_datas_.empty());
  DCHECK(global_v8_context_datas_.empty());
}

void V8ContextTrackerDataStore::Pass(
    std::unique_ptr<ExecutionContextData> ec_data) {
  DCHECK(ec_data.get());
  ec_data->process_data()->Add(PassKey(), ec_data.get());
  auto result = global_execution_context_datas_.insert(std::move(ec_data));
  DCHECK(result.second);
}

void V8ContextTrackerDataStore::Pass(std::unique_ptr<RemoteFrameData> rf_data) {
  DCHECK(rf_data.get());
  rf_data->process_data()->Add(PassKey(), rf_data.get());
  auto result = global_remote_frame_datas_.insert(std::move(rf_data));
  DCHECK(result.second);
}

void V8ContextTrackerDataStore::Pass(std::unique_ptr<V8ContextData> v8_data) {
  DCHECK(v8_data.get());
  v8_data->process_data()->Add(PassKey(), v8_data.get());
  auto result = global_v8_context_datas_.insert(std::move(v8_data));
  DCHECK(result.second);
}

ExecutionContextData* V8ContextTrackerDataStore::Get(
    const blink::ExecutionContextToken& token) {
  auto it = global_execution_context_datas_.find(token);
  if (it == global_execution_context_datas_.end())
    return nullptr;
  return it->get();
}

RemoteFrameData* V8ContextTrackerDataStore::Get(
    const blink::RemoteFrameToken& token) {
  auto it = global_remote_frame_datas_.find(token);
  if (it == global_remote_frame_datas_.end())
    return nullptr;
  return it->get();
}

V8ContextData* V8ContextTrackerDataStore::Get(
    const blink::V8ContextToken& token) {
  auto it = global_v8_context_datas_.find(token);
  if (it == global_v8_context_datas_.end())
    return nullptr;
  return it->get();
}

void V8ContextTrackerDataStore::Destroy(
    const blink::ExecutionContextToken& token) {
  auto it = global_execution_context_datas_.find(token);
  DCHECK(it != global_execution_context_datas_.end());
  auto* ec_data = it->get();
  ec_data->process_data()->Remove(PassKey(), ec_data);
  global_execution_context_datas_.erase(it);
}

void V8ContextTrackerDataStore::Destroy(const blink::RemoteFrameToken& token) {
  auto it = global_remote_frame_datas_.find(token);
  DCHECK(it != global_remote_frame_datas_.end());
  auto* rf_data = it->get();
  rf_data->process_data()->Remove(PassKey(), rf_data);
  global_remote_frame_datas_.erase(it);
}

void V8ContextTrackerDataStore::Destroy(const blink::V8ContextToken& token) {
  auto it = global_v8_context_datas_.find(token);
  DCHECK(it != global_v8_context_datas_.end());
  auto* v8_data = it->get();
  v8_data->process_data()->Remove(PassKey(), v8_data);
  global_v8_context_datas_.erase(it);
}

}  // namespace internal
}  // namespace v8_memory
}  // namespace performance_manager
