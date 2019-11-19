// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_manager.h"

#include "base/macros.h"
#include "components/autofill/core/browser/logging/log_router.h"

namespace autofill {

namespace {

class LogManagerImpl : public LogManager {
 public:
  LogManagerImpl(LogRouter* log_router, base::Closure notification_callback);

  ~LogManagerImpl() override;

  // LogManager
  void OnLogRouterAvailabilityChanged(bool router_can_be_used) override;
  void SetSuspended(bool suspended) override;
  void LogTextMessage(const std::string& text) const override;
  void LogEntry(base::Value&& entry) const override;
  bool IsLoggingActive() const override;
  LogBufferSubmitter Log() override;

 private:
  // A LogRouter instance obtained on construction. May be null.
  LogRouter* const log_router_;

  // True if |this| is registered with some LogRouter which can accept logs.
  bool can_use_log_router_;

  bool is_suspended_ = false;

  // Called every time the logging activity status changes.
  base::Closure notification_callback_;

  DISALLOW_COPY_AND_ASSIGN(LogManagerImpl);
};

LogManagerImpl::LogManagerImpl(LogRouter* log_router,
                               base::Closure notification_callback)
    : log_router_(log_router),
      can_use_log_router_(log_router_ && log_router_->RegisterManager(this)),
      notification_callback_(notification_callback) {}

LogManagerImpl::~LogManagerImpl() {
  if (log_router_)
    log_router_->UnregisterManager(this);
}

void LogManagerImpl::OnLogRouterAvailabilityChanged(bool router_can_be_used) {
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

void LogManagerImpl::SetSuspended(bool suspended) {
  if (suspended == is_suspended_)
    return;
  is_suspended_ = suspended;
  if (can_use_log_router_) {
    // The availability of the logging changed as a result.
    if (!notification_callback_.is_null())
      notification_callback_.Run();
  }
}

void LogManagerImpl::LogTextMessage(const std::string& text) const {
  if (!IsLoggingActive())
    return;
  log_router_->ProcessLog(text);
}

void LogManagerImpl::LogEntry(base::Value&& entry) const {
  if (!IsLoggingActive())
    return;
  log_router_->ProcessLog(std::move(entry));
}

bool LogManagerImpl::IsLoggingActive() const {
  return can_use_log_router_ && !is_suspended_;
}

LogBufferSubmitter LogManagerImpl::Log() {
  return LogBufferSubmitter(log_router_, IsLoggingActive());
}

}  // namespace

// static
std::unique_ptr<LogManager> LogManager::Create(
    LogRouter* log_router,
    base::Closure notification_callback) {
  return std::make_unique<LogManagerImpl>(log_router, notification_callback);
}

}  // namespace autofill
