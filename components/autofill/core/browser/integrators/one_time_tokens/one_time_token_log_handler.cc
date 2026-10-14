// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/one_time_token_log_handler.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill {

one_time_tokens::OneTimeTokenLogSink::LogHandler CreateOneTimeTokenLogHandler(
    LogRouter* log_router) {
  if (!log_router) {
    return base::NullCallback();
  }
  std::unique_ptr<LogManager> log_manager = LogManager::Create(
      log_router, /*notification_callback=*/base::NullCallback());
  return base::BindRepeating(
      [](LogManager* manager, std::string_view message) {
        LOG_AF(manager) << LoggingScope::kOneTimeTokens << message;
      },
      base::Owned(std::move(log_manager)));
}

}  // namespace autofill
