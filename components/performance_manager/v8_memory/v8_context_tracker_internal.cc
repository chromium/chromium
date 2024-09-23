// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker_internal.h"

#include <utility>

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "components/performance_manager/v8_memory/v8_context_tracker_helpers.h"

namespace performance_manager {
namespace v8_memory {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ExecutionContextData implementation:

ExecutionContextData::ExecutionContextData(
    ProcessData* process_data,
    const blink::ExecutionContextToken& token,
    mojom::IframeAttributionDataPtr iframe_attribution_data)
    : ExecutionContextState(token, std::move(iframe_attribution_data)),
      process_data_(process_data) {}

ExecutionContextData::~ExecutionContextData() {
  DCHECK(!IsTracked());
  DCHECK(ShouldDestroy());
  DCHECK_EQ(0u, main_nondetached_v8_context_count_);
}

bool ExecutionContextData::IsTracked() const {
  return previous() || next();
}

bool ExecutionContextData::ShouldDestroy() const {
  return v8_context_count_ == 0 && !remote_frame_data_;
}

void ExecutionContextData::SetRemoteFrameData(
    base::PassKey<RemoteFrameData>,
    RemoteFrameData* remote_frame_data) {
  DCHECK(!remote_frame_data_);
  DCHECK(remote_frame_data);
  remote_frame_data_ = remote_frame_data;
}

bool ExecutionContextData::ClearRemoteFrameData(
    base::PassKey<RemoteFrameData>) {
  DCHECK(remote_frame_data_);
  remote_frame_data_ = nullptr;
  return ShouldDestroy();
}

void ExecutionContextData::IncrementV8ContextCount(
    base::PassKey<V8ContextData>) {
  ++v8_context_count_;
}

bool ExecutionContextData::DecrementV8ContextCount(
    base::PassKey<V8ContextData>) {
  DCHECK_LT(0u, v8_context_count_);
  DCHECK_LT(main_nondetached_v8_context_count_, v8_context_count_);
  --v8_context_count_;
  return ShouldDestroy();
}

bool ExecutionContextData::MarkDestroyed(base::PassKey<ProcessData>) {
  if (destroyed)
    return false;
  destroyed = true;
  return true;
}

bool ExecutionContextData::MarkMainV8ContextCreated(
    base::PassKey<V8ContextTrackerDataStore>) {
  if (main_nondetached_v8_context_count_ >= v8_context_count_)
    return false;
  if (main_nondetached_v8_context_count_ >= 1)
    return false;
  ++main_nondetached_v8_context_count_;
  return true;
}

void ExecutionContextData::MarkMainV8ContextDetached(
    base::PassKey<V8ContextData>) {
  DCHECK_LT(0u, main_nondetached_v8_context_count_);
  --main_nondetached_v8_context_count_;
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
  execution_context_data->SetRemoteFrameData(PassKey(), this);
}

RemoteFrameData::~RemoteFrameData() {
  DCHECK(!IsTracked());

  // If this is the last reference keeping alive a tracked ExecutionContextData,
  // then clean it up as well. Untracked ExecutionContextDatas will go out of
  // scope on their own.
  if (execution_context_data_->ClearRemoteFrameData(PassKey()) &&
      execution_context_data_->IsTracked()) {
    // Reset `execution_context_data_` to nullptr because it will be destroyed
    // using the token below.
    blink::ExecutionContextToken token = execution_context_data_->GetToken();
    execution_context_data_ = nullptr;
    process_data_->data_store()->Destroy(token);
  }
}

bool RemoteFrameData::IsTracked() const {
  return previous() || next();
}

////////////////////////////////////////////////////////////////////////////////
// V8ContextData implementation:

V8ContextData::V8ContextData(ProcessData* process_data,
                             const mojom::V8ContextDescription& description,
                             ExecutionContextData* execution_context_data)
    : V8ContextState(description, execution_context_data),
      process_data_(process_data) {
  DCHECK(process_data);
  DCHECK_EQ(static_cast<bool>(execution_context_data),
            static_cast<bool>(description.execution_context_token));
  if (execution_context_data) {
    DCHECK_EQ(execution_context_data->GetToken(),
              *description.execution_context_token);

    // These must be same process.
    DCHECK_EQ(process_data, execution_context_data->process_data());
    execution_context_data->IncrementV8ContextCount(PassKey());
  }
}

V8ContextData::~V8ContextData() {
  DCHECK(!IsTracked());

  // Mark as detached if necessary so that main world counts are appropriately
  // updated even during tear down code paths.
  MarkDetachedImpl();

  // If this is the last reference keeping alive a tracked ExecutionContextData,
  // then clean it up as well. Untracked ExecutionContextDatas will go out of
  // scope on their own.
  auto* execution_context_data = GetExecutionContextData();
  if (execution_context_data &&
      execution_context_data->DecrementV8ContextCount(PassKey()) &&
      execution_context_data->IsTracked()) {
    // Reset the execution_context_state to nullptr because it will be
    // destroyed using the token below.
    blink::ExecutionContextToken token = execution_context_data->GetToken();
    execution_context_state = nullptr;
    process_data_->data_store()->Destroy(token);
  }
}

bool V8ContextData::IsTracked() const {
  return previous() || next();
}

ExecutionContextData* V8ContextData::GetExecutionContextData() const {
  return static_cast<ExecutionContextData*>(execution_context_state);
}

void V8ContextData::SetWasTracked(base::PassKey<V8ContextTrackerDataStore>) {
  DCHECK(!was_tracked_);
  was_tracked_ = true;
}

bool V8ContextData::MarkDetached(base::PassKey<ProcessData>) {
  return MarkDetachedImpl();
}

bool V8ContextData::IsMainV8Context() const {
  auto* ec_data = GetExecutionContextData();
  if (!ec_data)
    return false;
  // ExecutionContexts hosting worklets have no main world (there can be many
  // worklets sharing an ExecutionContext).
  if (IsWorkletToken(ec_data->GetToken()))
    return false;

  // We've already checked sane combinations of ExecutionContextToken types and
  // world types in ValidateV8ContextDescription, so don't need to be overly
  // thorough here.

  // Only main frames and workers can be "main" contexts.
  auto world_type = description.world_type;
  return world_type == mojom::V8ContextWorldType::kMain ||
         world_type == mojom::V8ContextWorldType::kWorkerOrWorklet;
}

bool V8ContextData::MarkDetachedImpl() {
  if (detached)
    return false;
  detached = true;
  // The EC is notified of the main V8 context only when it is passed to the
  // data store (at which point |was_tracked_| is set to true). Only do the
  // symmetric operation if the first occurred.
  if (IsMainV8Context() && was_tracked_) {
    if (auto* ec_data = GetExecutionContextData())
      ec_data->MarkMainV8ContextDetached(PassKey());
  }
  return true;
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

void ProcessData::Add(base::PassKey<V8ContextTrackerDataStore>,
                      ExecutionContextData* ec_data) {
  DCHECK(ec_data);
  DCHECK_EQ(this, ec_data->process_data());
  DCHECK(!ec_data->ShouldDestroy());
  DCHECK(!ec_data->IsTracked());
  execution_context_datas_.Append(ec_data);
  counts_.IncrementExecutionContextDataCount();
}

void ProcessData::Add(base::PassKey<V8ContextTrackerDataStore>,
                      RemoteFrameData* rf_data) {
  DCHECK(rf_data);
  DCHECK_EQ(this, rf_data->process_data());
  DCHECK(!rf_data->IsTracked());
  remote_frame_datas_.Append(rf_data);
}

void ProcessData::Add(base::PassKey<V8ContextTrackerDataStore>,
                      V8ContextData* v8_data) {
  DCHECK(v8_data);
  DCHECK_EQ(this, v8_data->process_data());
  DCHECK(!v8_data->IsTracked());
  v8_context_datas_.Append(v8_data);
  counts_.IncrementV8ContextDataCount();
}

void ProcessData::Remove(base::PassKey<V8ContextTrackerDataStore>,
                         ExecutionContextData* ec_data) {
  DCHECK(ec_data);
  DCHECK_EQ(this, ec_data->process_data());
  DCHECK(ec_data->IsTracked());
  DCHECK(ec_data->ShouldDestroy());
  counts_.DecrementExecutionContextDataCount(ec_data->destroyed);
  ec_data->RemoveFromList();
}

void ProcessData::Remove(base::PassKey<V8ContextTrackerDataStore>,
                         RemoteFrameData* rf_data) {
  DCHECK(rf_data);
  DCHECK_EQ(this, rf_data->process_data());
  DCHECK(rf_data->IsTracked());
  rf_data->RemoveFromList();
}

void ProcessData::Remove(base::PassKey<V8ContextTrackerDataStore>,
                         V8ContextData* v8_data) {
  DCHECK(v8_data);
  DCHECK_EQ(this, v8_data->process_data());
  DCHECK(v8_data->IsTracked());
  counts_.DecrementV8ContextDataCount(v8_data->detached);
  v8_data->RemoveFromList();
}

bool ProcessData::MarkDestroyed(base::PassKey<V8ContextTrackerDataStore>,
                                ExecutionContextData* ec_data) {
  bool result = ec_data->MarkDestroyed(PassKey());
  if (result)
    counts_.MarkExecutionContextDataDestroyed();
  return result;
}

bool ProcessData::MarkDetached(base::PassKey<V8ContextTrackerDataStore>,
                               V8ContextData* v8_data) {
  bool result = v8_data->MarkDetached(PassKey());
  if (result)
    counts_.MarkV8ContextDataDetached();
  return result;
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

bool V8ContextTrackerDataStore::Pass(std::unique_ptr<V8ContextData> v8_data) {
  DCHECK(v8_data.get());
  auto* ec_data = v8_data->GetExecutionContextData();
  if (ec_data && v8_data->IsMainV8Context()) {
    if (!ec_data->MarkMainV8ContextCreated(PassKey()))
      return false;
  }
  v8_data->process_data()->Add(PassKey(), v8_data.get());
  v8_data->SetWasTracked(PassKey());
  auto result = global_v8_context_datas_.insert(std::move(v8_data));
  DCHECK(result.second);
  return true;
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

void V8ContextTrackerDataStore::MarkDestroyed(ExecutionContextData* ec_data) {
  DCHECK(ec_data);
  if (ec_data->process_data()->MarkDestroyed(PassKey(), ec_data)) {
    DCHECK_LT(destroyed_execution_context_count_,
              global_execution_context_datas_.size());
    ++destroyed_execution_context_count_;
  }
}

bool V8ContextTrackerDataStore::MarkDetached(V8ContextData* v8_data) {
  DCHECK(v8_data);
  if (v8_data->process_data()->MarkDetached(PassKey(), v8_data)) {
    DCHECK_LT(detached_v8_context_count_, global_v8_context_datas_.size());
    ++detached_v8_context_count_;
    return true;
  }
  return false;
}

void V8ContextTrackerDataStore::Destroy(
    const blink::ExecutionContextToken& token) {
  auto it = global_execution_context_datas_.find(token);
  CHECK(it != global_execution_context_datas_.end(), base::NotFatalUntil::M130);
  auto* ec_data = it->get();
  if (ec_data->destroyed) {
    DCHECK_LT(0u, destroyed_execution_context_count_);
    --destroyed_execution_context_count_;
  } else {
    DCHECK_LT(destroyed_execution_context_count_,
              global_execution_context_datas_.size());
  }
  ec_data->process_data()->Remove(PassKey(), ec_data);
  global_execution_context_datas_.erase(it);
}

void V8ContextTrackerDataStore::Destroy(const blink::RemoteFrameToken& token) {
  auto it = global_remote_frame_datas_.find(token);
  CHECK(it != global_remote_frame_datas_.end(), base::NotFatalUntil::M130);
  auto* rf_data = it->get();
  rf_data->process_data()->Remove(PassKey(), rf_data);
  global_remote_frame_datas_.erase(it);
}

void V8ContextTrackerDataStore::Destroy(const blink::V8ContextToken& token) {
  auto it = global_v8_context_datas_.find(token);
  CHECK(it != global_v8_context_datas_.end(), base::NotFatalUntil::M130);
  auto* v8_data = it->get();
  if (v8_data->detached) {
    DCHECK_LT(0u, detached_v8_context_count_);
    --detached_v8_context_count_;
  } else {
    DCHECK_LT(detached_v8_context_count_, global_v8_context_datas_.size());
  }
  v8_data->process_data()->Remove(PassKey(), v8_data);
  global_v8_context_datas_.erase(it);
}

}  // namespace internal
}  // namespace v8_memory
}  // namespace performance_manager
