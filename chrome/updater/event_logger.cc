// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_logger.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/telemetry_logger/proto/log_request.pb.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/protos/omaha_usage_stats_event.pb.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "components/update_client/network.h"
#include "event_logger.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "url/gurl.h"

namespace updater {
namespace {

// Returns the logging cookie value which should be used in an event
// transmission. If a cookie is expired or not persisted, a special value is
// used to request a new token from the server. Expired cookies are cleared from
// the store.
std::string GetLoggingCookieValue(base::Time now,
                                  scoped_refptr<PersistedData> persisted_data) {
  static constexpr char kDefaultLoggingCookie[] = "\"\"";
  std::optional<PersistedData::Cookie> logging_cookie =
      persisted_data->GetRemoteLoggingCookie();
  if (!logging_cookie) {
    return kDefaultLoggingCookie;
  }
  if (logging_cookie->expiration <= now) {
    VLOG(1) << "Clearing expired logging cookie";
    persisted_data->ClearRemoteLoggingCookie();
    return kDefaultLoggingCookie;
  }
  return logging_cookie->value;
}

proto::Omaha4UsageStatsMetadata GetMetadata(
    UpdaterScope scope,
    scoped_refptr<Configurator> configurator,
    bool is_cloud_managed) {
  proto::Omaha4UsageStatsMetadata metadata;

  metadata.set_platform(configurator->GetOSLongName());
  metadata.set_os_version(base::SysInfo::OperatingSystemVersion());
  metadata.set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
  metadata.set_o4_omaha_cohort_id(
      configurator->GetPersistedData()->GetCohort(kUpdaterAppId));
  metadata.set_app_version(kUpdaterVersion);
  metadata.set_is_machine(IsSystemInstall(scope));
  metadata.set_is_cbcm_managed(is_cloud_managed);
  std::optional<bool> externally_managed =
      configurator->IsMachineExternallyManaged();
  if (externally_managed.has_value()) {
    metadata.set_is_domain_joined(*externally_managed);
  }
  return metadata;
}

}  // namespace

using HttpRequestCallback =
    base::OnceCallback<void(std::optional<int> http_status,
                            std::optional<std::string> response_body)>;
using ::updater::proto::Omaha4Metric;
using ::updater::proto::Omaha4UsageStatsExtension;
using ::updater::proto::Omaha4UsageStatsMetadata;

RemoteLoggingDelegate::RemoteLoggingDelegate(
    UpdaterScope scope,
    const GURL& event_logging_url,
    bool is_cloud_managed,
    scoped_refptr<Configurator> configurator,
    std::unique_ptr<base::Clock> clock)
    : event_logging_url_(event_logging_url),
      metadata_(::updater::GetMetadata(scope, configurator, is_cloud_managed)),
      minimum_cooldown_(configurator->MinimumEventLoggingCooldown()),
      configurator_(configurator),
      clock_(std::move(clock)),
      persisted_data_(configurator_->GetUpdaterPersistedData()),
      main_sequence_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemoteLoggingDelegate::~RemoteLoggingDelegate() = default;

void RemoteLoggingDelegate::StoreNextAllowedAttemptTime(
    base::Time time,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_sequence_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<PersistedData> persisted_data, base::Time time) {
            persisted_data->SetNextAllowedLoggingAttemptTime(time);
          },
          persisted_data_, time),
      std::move(callback));
}

void RemoteLoggingDelegate::DoPostRequest(
    const std::string& request_body,
    base::OnceCallback<void(std::optional<int> http_status,
                            std::optional<std::string> response_body)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<Configurator> configurator, const GURL& url,
             const std::string& request_body, base::Time now,
             base::OnceCallback<void(std::optional<int>,
                                     std::optional<std::string> response_body)>
                 callback) {
            std::unique_ptr<update_client::NetworkFetcher> fetcher =
                configurator->GetNetworkFetcherFactory()->Create();
            if (!fetcher) {
              VLOG(1) << "Failed to create network fetcher for logging request";
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), std::nullopt,
                                            std::nullopt));
              return;
            }
            // The fetcher and response code are retained by
            // the completion callback.
            auto response_code =
                std::make_unique<std::optional<int>>(std::nullopt);
            std::optional<int>* response_code_ptr = response_code.get();
            update_client::NetworkFetcher* fetcher_ptr = fetcher.get();

            fetcher_ptr->PostRequest(
                url, request_body, "application/x-protobuffer",
                {{update_client::NetworkFetcher::kHeaderCookie,
                  base::StrCat(
                      {"NID=",
                       GetLoggingCookieValue(
                           now, configurator->GetUpdaterPersistedData())})}},
                base::BindRepeating(
                    [](std::optional<int>* response_code_out, int response_code,
                       int64_t content_length) {
                      *response_code_out = response_code;
                    },
                    response_code_ptr),
                /*progress_callback=*/base::DoNothing(),
                base::BindOnce(
                    [](base::Time now,
                       scoped_refptr<PersistedData> persisted_data,
                       HttpRequestCallback callback,
                       std::unique_ptr<std::optional<int>> response_code,
                       std::unique_ptr<update_client::NetworkFetcher> fetcher,
                       std::optional<std::string> response_body, int net_error,
                       const std::string& header_etag,
                       const std::string& header_x_cup_server_proof,
                       const std::string& header_set_cookie,
                       int64_t xheader_retry_after_sec) {
                      if (net_error) {
                        VLOG(1)
                            << "Upload failed due to net error " << net_error;
                        VLOG_IF(1, response_code->has_value())
                            << "HTTP response code: " << response_code->value();
                        base::SequencedTaskRunner::GetCurrentDefault()
                            ->PostTask(
                                FROM_HERE,
                                base::BindOnce(std::move(callback),
                                               std::nullopt, std::nullopt));
                        return;
                      }
                      std::optional<PersistedData::Cookie> logging_cookie =
                          ExtractEventLoggingCookie(now, header_set_cookie);
                      if (logging_cookie) {
                        persisted_data->SetRemoteLoggingCookie(*logging_cookie);
                      }
                      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                          FROM_HERE,
                          base::BindOnce(std::move(callback), *response_code,
                                         response_body));
                    },
                    now, configurator->GetUpdaterPersistedData(),
                    std::move(callback), std::move(response_code),
                    std::move(fetcher)));
          },
          configurator_, event_logging_url_, request_body, clock_->Now(),
          base::BindPostTaskToCurrentDefault(std::move(callback))));
}

int RemoteLoggingDelegate::GetLogIdentifier() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enterprise_companion::telemetry_logger::proto::CHROME_UPDATER;
}

std::string RemoteLoggingDelegate::AggregateAndSerializeEvents(
    base::span<Omaha4Metric> events) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Omaha4UsageStatsExtension extension;
  *extension.mutable_metadata() = GetMetadata();
  for (const Omaha4Metric& metric : events) {
    *extension.add_metric() = metric;
  }
  return extension.SerializeAsString();
}

base::TimeDelta RemoteLoggingDelegate::MinimumCooldownTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return minimum_cooldown_;
}

Omaha4UsageStatsMetadata RemoteLoggingDelegate::GetMetadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metadata_;
}

// Parses the NID cookie from a returned Cookie: HTTP header.
std::optional<PersistedData::Cookie> ExtractEventLoggingCookie(
    base::Time now,
    const std::string& set_cookie_value) {
  static constexpr base::TimeDelta kDefaultTtl = base::Days(180);
  std::optional<std::string> cookie_value;
  std::optional<base::Time> expiration;
  for (const std::string& s :
       base::SplitString(set_cookie_value, ";", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    std::optional<std::pair<std::string_view, std::string_view>> kv_pair =
        base::SplitStringOnce(s, "=");
    if (!kv_pair.has_value()) {
      continue;
    }
    auto [key, value] = *kv_pair;
    if (key == "NID") {
      if (!value.empty()) {
        cookie_value = value;
      }
    } else if (base::EqualsCaseInsensitiveASCII(key, "expires")) {
      base::Time parsed_time;
      if (base::Time::FromString(std::string(value).c_str(), &parsed_time)) {
        expiration = parsed_time;
      }
    }
  }

  if (!cookie_value) {
    return std::nullopt;
  }

  return PersistedData::Cookie{
      .value = *std::move(cookie_value),
      .expiration = expiration ? *expiration : now + kDefaultTtl,
  };
}

}  // namespace updater
