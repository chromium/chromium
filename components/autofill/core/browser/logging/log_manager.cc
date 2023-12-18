// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_manager.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/logging/log_router.h"

namespace autofill {

namespace {

class RoutingLogManagerImpl : public RoutingLogManager {
 public:
  RoutingLogManagerImpl(LogRouter* log_router,
                        base::RepeatingClosure notification_callback);

  RoutingLogManagerImpl(const RoutingLogManagerImpl&) = delete;
  RoutingLogManagerImpl& operator=(const RoutingLogManagerImpl&) = delete;

  ~RoutingLogManagerImpl() override;

  // RoutingLogManager
  void OnLogRouterAvailabilityChanged(bool router_can_be_used) override;
  void SetSuspended(bool suspended) override;
  // LogManager
  bool IsLoggingActive() const override;
  LogBufferSubmitter Log() override;
  void ProcessLog(base::Value::Dict node,
                  base::PassKey<LogBufferSubmitter>) override;

 private:
  // A LogRouter instance obtained on construction. May be null.
  const raw_ptr<LogRouter> log_router_;

  // True if |this| is registered with some LogRouter which can accept logs.
  bool can_use_log_router_;

  bool is_suspended_ = false;

  // Called every time the logging activity status changes.
  base::RepeatingClosure notification_callback_;
};

RoutingLogManagerImpl::RoutingLogManagerImpl(
    LogRouter* log_router,
    base::RepeatingClosure notification_callback)
    : log_router_(log_router),
      can_use_log_router_(log_router_ && log_router_->RegisterManager(this)),
      notification_callback_(std::move(notification_callback)) {}

RoutingLogManagerImpl::~RoutingLogManagerImpl() {
  if (log_router_)
    log_router_->UnregisterManager(this);
}

void RoutingLogManagerImpl::OnLogRouterAvailabilityChanged(
    bool router_can_be_used) {
  DCHECK(log_router_);  // |log_router_| should be calling this method.
  if (can_use_log_router_ == router_can_be_used)
    return;
  can_use_log_router_ = router_can_be_used;

  if (!is_suspended_) {
    // The availability of the logging changed as a result.
    if (!notification_callback_.is_null())
      notification_callback_.Run();
  }
}

void RoutingLogManagerImpl::SetSuspended(bool suspended) {
  if (suspended == is_suspended_)
    return;
  is_suspended_ = suspended;
  if (can_use_log_router_) {
    // The availability of the logging changed as a result.
    if (!notification_callback_.is_null())
      notification_callback_.Run();
  }
}

bool RoutingLogManagerImpl::IsLoggingActive() const {
  return can_use_log_router_ && !is_suspended_;
}

LogBufferSubmitter RoutingLogManagerImpl::Log() {
  return LogBufferSubmitter(this);
}

void RoutingLogManagerImpl::ProcessLog(base::Value::Dict node,
                                       base::PassKey<LogBufferSubmitter>) {
  log_router_->ProcessLog(std::move(node));
}

class BufferingLogManagerImpl : public BufferingLogManager {
 public:
  BufferingLogManagerImpl() = default;

  BufferingLogManagerImpl(const BufferingLogManagerImpl&) = delete;
  BufferingLogManagerImpl& operator=(const BufferingLogManagerImpl&) = delete;

  ~BufferingLogManagerImpl() override = default;

  // BufferingLogManager
  void Flush(LogManager& destination) override;
  // LogManager
  bool IsLoggingActive() const override;
  LogBufferSubmitter Log() override;
  void ProcessLog(base::Value::Dict node,
                  base::PassKey<LogBufferSubmitter>) override;

 private:
  std::vector<base::Value::Dict> nodes_;
  std::optional<base::PassKey<LogBufferSubmitter>> pass_key_;
};

void BufferingLogManagerImpl::Flush(LogManager& destination) {
  auto nodes = std::exchange(nodes_, {});
  for (auto& node : nodes)
    destination.ProcessLog(std::move(node), *pass_key_);
}

bool BufferingLogManagerImpl::IsLoggingActive() const {
  return true;
}

LogBufferSubmitter BufferingLogManagerImpl::Log() {
  return LogBufferSubmitter(this);
}

void BufferingLogManagerImpl::ProcessLog(
    base::Value::Dict node,
    base::PassKey<LogBufferSubmitter> pass_key) {
  nodes_.push_back(std::move(node));
  pass_key_ = pass_key;
}

}  // namespace

// static
std::unique_ptr<RoutingLogManager> LogManager::Create(
    LogRouter* log_router,
    base::RepeatingClosure notification_callback) {
  return std::make_unique<RoutingLogManagerImpl>(
      log_router, std::move(notification_callback));
}

// static
std::unique_ptr<BufferingLogManager> LogManager::CreateBuffering() {
  return std::make_unique<BufferingLogManagerImpl>();
}

}  // namespace autofill
