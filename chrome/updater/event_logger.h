// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EVENT_LOGGER_H_
#define CHROME_UPDATER_EVENT_LOGGER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/telemetry_logger/telemetry_logger.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/protos/omaha_usage_stats_event.pb.h"
#include "chrome/updater/updater_scope.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace updater {
namespace proto {
class Omaha4Metric;
class Omaha4UsageStatsMetadata;
}  // namespace proto

class Configurator;

using UpdaterEventLogger =
    ::enterprise_companion::telemetry_logger::TelemetryLogger<
        proto::Omaha4Metric>;

// The TelemetryLogger delegate for the updater. The delegate is created on the
// main sequence but is accessed / destroyed on a sequence managed by
// TelemetryLogger.
class RemoteLoggingDelegate final : public UpdaterEventLogger::Delegate {
 public:
  RemoteLoggingDelegate(UpdaterScope scope,
                        const GURL& event_logging_url,
                        bool is_cloud_managed,
                        scoped_refptr<Configurator> configurator,
                        std::unique_ptr<base::Clock> clock);

  ~RemoteLoggingDelegate() override;

  // Overrides for TelemetryLogger::Delegate.
  void StoreNextAllowedAttemptTime(base::Time time,
                                   base::OnceClosure callback) override;
  void DoPostRequest(
      const std::string& request_body,
      base::OnceCallback<void(std::optional<int> http_status,
                              std::optional<std::string> response_body)>
          callback) override;
  int GetLogIdentifier() const override;
  base::TimeDelta MinimumCooldownTime() const override;
  std::string AggregateAndSerializeEvents(
      base::span<proto::Omaha4Metric> events) const override;

 private:
  proto::Omaha4UsageStatsMetadata GetMetadata() const;

  SEQUENCE_CHECKER(sequence_checker_);
  const GURL event_logging_url_;
  const proto::Omaha4UsageStatsMetadata metadata_;
  const base::TimeDelta minimum_cooldown_;
  scoped_refptr<Configurator> configurator_;
  std::unique_ptr<base::Clock> clock_;
  const scoped_refptr<PersistedData> persisted_data_;
  scoped_refptr<base::SequencedTaskRunner> main_sequence_;
};

// Extracts the value and expiration of the event logging cooke from the value
// of a Set-Cookie header. Returns nullopt if the logging cookie is not present
// or the value could not be parsed. If the cookie has no expiration, a default
// is chosen a fixed offset from `now`.
std::optional<PersistedData::Cookie> ExtractEventLoggingCookie(
    base::Time now,
    const std::string& set_cookie_value);

}  // namespace updater

#endif  // CHROME_UPDATER_EVENT_LOGGER_H_
