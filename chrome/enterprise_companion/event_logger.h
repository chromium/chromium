// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_EVENT_LOGGER_H_
#define CHROME_ENTERPRISE_COMPANION_EVENT_LOGGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/proto/log_request.pb.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class Clock;
}

namespace network::mojom {
class URLResponseHead;
}

namespace enterprise_companion {

// The shortest duration to wait between making remote log requests.
inline constexpr base::TimeDelta kMinLogTransmissionCooldown =
    base::Minutes(15);

// Records events from the client and transmits service health metrics.
// Construction, destruction, and all method calls must occur on the same
// sequence. However, the callbacks returned by the event registration methods
// may be invoked from any sequence. `InitializeEventLogger` must be called
// before any `EventLogger` is used. Each EventLogger instance should be
// responsible for a single batch of operations that are logged together.
class EventLogger : public base::RefCountedThreadSafe<EventLogger> {
 public:
  using OnEnrollmentFinishCallback = StatusCallback;
  using OnPolicyFetchFinishCallback = StatusCallback;

  // Flush logged events. This will either transmit the events to the remote
  // logging endpoint, or, if the client is rate-limited, cache the logs
  // in-memory. Cached logs will be transmitted once the client is no longer
  // rate-limited. Logs are flushed without blocking when the logger is
  // destroyed.
  virtual void Flush() = 0;

  // Functions to register the start of a loggable event. These functions return
  // callbacks that should be invoked when the action has completed.
  [[nodiscard]] virtual OnEnrollmentFinishCallback OnEnrollmentStart() = 0;
  [[nodiscard]] virtual OnPolicyFetchFinishCallback OnPolicyFetchStart() = 0;

 protected:
  friend class base::RefCountedThreadSafe<EventLogger>;
  virtual ~EventLogger() = default;
};

// Manages event loggers by batching logged events, transmitting logs to the
// remote endpoint, creating logger instances, and respecting rate-limiting.
class EventLoggerManager {
 public:
  virtual ~EventLoggerManager() = default;

  virtual scoped_refptr<EventLogger> CreateEventLogger() = 0;
};

// Functional interface for performing the network request.
class EventLogUploader {
 public:
  virtual ~EventLogUploader() = default;

  // Callback type for log requests. Response head may be nullptr.
  using LogRequestCallback = base::OnceCallback<void(
      mojo::StructPtr<network::mojom::URLResponseHead> response_head,
      std::optional<std::string> response_info)>;

  virtual void DoLogRequest(proto::LogRequest request,
                            LogRequestCallback callback) = 0;
};

std::unique_ptr<EventLoggerManager> CreateEventLoggerManager(
    std::unique_ptr<EventLogUploader> uploader,
    const base::Clock* clock = base::DefaultClock::GetInstance());

std::unique_ptr<EventLogUploader> CreateEventLogUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_EVENT_LOGGER_H_
