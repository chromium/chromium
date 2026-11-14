// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EVENT_LOGGER_H_
#define CHROME_UPDATER_EVENT_LOGGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
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

// Sends the event logging requests and persists the event logging state for
// `RemoteLoggingDelegate`. Lives on the main sequence. Owned by
// `RemoteEventLogging`.
class RemoteLoggingUploader {
 public:
  using NetworkFetcherFactoryGetter = base::RepeatingCallback<
      scoped_refptr<update_client::NetworkFetcherFactory>()>;
  using PostRequestCallback =
      base::OnceCallback<void(std::optional<int> http_status,
                              std::optional<std::string> response_body)>;

  RemoteLoggingUploader(
      const GURL& event_logging_url,
      scoped_refptr<PersistedData> persisted_data,
      NetworkFetcherFactoryGetter network_fetcher_factory_getter,
      std::unique_ptr<base::Clock> clock);
  RemoteLoggingUploader(const RemoteLoggingUploader&) = delete;
  RemoteLoggingUploader& operator=(const RemoteLoggingUploader&) = delete;
  ~RemoteLoggingUploader();

  // Posts `request_body` to the event logging URL along with the logging
  // cookie. `http_status` and `response_body` are nullopt if the request
  // fails. A request that is in flight when this object is destroyed still
  // completes and runs `callback`, but the cookie set by the server is not
  // persisted.
  void PostRequest(const std::string& request_body,
                   PostRequestCallback callback);

  // Persists the time of the next allowed upload attempt.
  void StoreNextAllowedAttemptTime(base::Time time);

  base::WeakPtr<RemoteLoggingUploader> GetWeakPtr();

 private:
  // Persists the logging cookie found in the value of a Set-Cookie header.
  void PersistLoggingCookie(base::Time now,
                            const std::string& set_cookie_value);

  SEQUENCE_CHECKER(sequence_checker_);
  const GURL event_logging_url_;
  const scoped_refptr<PersistedData> persisted_data_;
  const NetworkFetcherFactoryGetter network_fetcher_factory_getter_;
  const std::unique_ptr<base::Clock> clock_;
  base::WeakPtrFactory<RemoteLoggingUploader> weak_factory_{this};
};

// The TelemetryLogger delegate for the updater. The delegate is created on the
// main sequence but is accessed / destroyed on a sequence managed by
// TelemetryLogger. For that reason, it owns nothing that lives on the main
// sequence, and posts its work to a `RemoteLoggingUploader` instead.
class RemoteLoggingDelegate final : public UpdaterEventLogger::Delegate {
 public:
  // `configurator` is only used during construction. `uploader` is only
  // dereferenced on the sequence that constructs this object.
  RemoteLoggingDelegate(UpdaterScope scope,
                        bool is_cloud_managed,
                        const Configurator& configurator,
                        base::WeakPtr<RemoteLoggingUploader> uploader);

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
  const proto::Omaha4UsageStatsMetadata metadata_;
  const base::TimeDelta minimum_cooldown_;
  const base::WeakPtr<RemoteLoggingUploader> uploader_;
  const scoped_refptr<base::SequencedTaskRunner> main_sequence_;
};

// Owns the remote event logging objects of a `Configurator`: the event logger
// and the `RemoteLoggingUploader` that the delegate of the event logger posts
// its main sequence work to. Lives on the main sequence.
//
// The delegate only holds a weak pointer to the uploader, so the event logger
// doesn't keep the `Configurator`, its persisted data or its prefs alive. The
// event logger is reference counted and may outlive this object. After this
// object is destroyed, the event logger still answers its callbacks, but it no
// longer sends requests or persists anything.
class RemoteEventLogging {
 public:
  // `configurator` is only used during construction.
  // `network_fetcher_factory_getter` is only run while this object is alive.
  RemoteEventLogging(UpdaterScope scope,
                     bool is_cloud_managed,
                     const Configurator& configurator,
                     const GURL& event_logging_url,
                     RemoteLoggingUploader::NetworkFetcherFactoryGetter
                         network_fetcher_factory_getter);
  RemoteEventLogging(const RemoteEventLogging&) = delete;
  RemoteEventLogging& operator=(const RemoteEventLogging&) = delete;
  ~RemoteEventLogging();

  scoped_refptr<UpdaterEventLogger> event_logger() const {
    return event_logger_;
  }

 private:
  // Declared before `event_logger_`, because the delegate of `event_logger_`
  // is given a weak pointer to the uploader during construction.
  const std::unique_ptr<RemoteLoggingUploader> uploader_;
  const scoped_refptr<UpdaterEventLogger> event_logger_;
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
