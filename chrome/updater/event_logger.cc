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
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
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

}  // namespace

using PostRequestCompleteCallback =
    ::update_client::NetworkFetcher::PostRequestCompleteCallback;
using ::updater::proto::Omaha4Metric;
using ::updater::proto::Omaha4UsageStatsExtension;
using ::updater::proto::Omaha4UsageStatsMetadata;

RemoteLoggingDelegate::RemoteLoggingDelegate(
    UpdaterScope scope,
    const GURL& event_logging_url,
    bool is_cloud_managed,
    scoped_refptr<Configurator> configurator,
    std::unique_ptr<base::Clock> clock)
    : scope_(scope),
      event_logging_url_(event_logging_url),
      is_cloud_managed_(is_cloud_managed),
      configurator_(configurator),
      clock_(std::move(clock)),
      persisted_data_(configurator_->GetUpdaterPersistedData()) {}

RemoteLoggingDelegate::~RemoteLoggingDelegate() = default;

bool RemoteLoggingDelegate::StoreNextAllowedAttemptTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  persisted_data_->SetNextAllowedLoggingAttemptTime(time);
  return true;
}

std::optional<base::Time> RemoteLoggingDelegate::GetNextAllowedAttemptTime()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time time = persisted_data_->GetNextAllowedLoggingAttemptTime();
  if (time.is_null()) {
    return std::nullopt;
  }
  return time;
}

void RemoteLoggingDelegate::DoPostRequest(
    const std::string& request_body,
    base::OnceCallback<void(std::optional<int> http_status,
                            std::optional<std::string> response_body)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (network_fetcher_) {
    VLOG(1) << "Refusing to perform a concurrent logging request";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::nullopt, std::nullopt));
    return;
  }

  network_fetcher_ = configurator_->GetNetworkFetcherFactory()->Create();
  if (!network_fetcher_) {
    VLOG(1) << "Failed to create network fetcher for logging request";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::nullopt, std::nullopt));
    return;
  }

  network_fetcher_->PostRequest(
      this->event_logging_url_, request_body, "text/plain",
      {{update_client::NetworkFetcher::kHeaderCookie,
        base::StrCat(
            {"NID=", GetLoggingCookieValue(clock_->Now(), persisted_data_)})}},
      base::BindRepeating(&RemoteLoggingDelegate::ResponseStart,
                          weak_factory_.GetWeakPtr()),
      /*progress_callback=*/base::DoNothing(),
      base::BindOnce(
          [](base::WeakPtr<RemoteLoggingDelegate> weak_this,
             base::OnceCallback<void(std::optional<int> http_status,
                                     std::optional<std::string> response_body)>
                 callback,
             std::optional<std::string> response_body, int net_error,
             const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             const std::string& header_set_cookie,
             int64_t xheader_retry_after_sec) {
            if (!weak_this) {
              VLOG(1)
                  << "The logging delegate destroyed before an HTTP request "
                     "completed";
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), std::nullopt,
                                            std::nullopt));
              return;
            }
            weak_this->ResponseComplete(
                std::move(callback), response_body, net_error, header_etag,
                header_x_cup_server_proof, header_set_cookie,
                xheader_retry_after_sec);
          },
          weak_factory_.GetWeakPtr(), std::move(callback)));
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
  return configurator_->MinimumEventLoggingCooldown();
}

void RemoteLoggingDelegate::ResponseStart(int status_code,
                                          int64_t content_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  http_status_code_ = status_code;
}

void RemoteLoggingDelegate::ResponseComplete(
    base::OnceCallback<void(std::optional<int> http_status,
                            std::optional<std::string> response_body)> callback,
    std::optional<std::string> response_body,
    int net_error,
    const std::string& header_etag,
    const std::string& header_x_cup_server_proof,
    const std::string& header_set_cookie,
    int64_t xheader_retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reset the network fetcher to allow subsequent requests to create new ones.
  network_fetcher_ = nullptr;

  if (net_error || http_status_code_ != 200) {
    VLOG(1) << __func__ << "Remote logging post request failed. "
            << "net_error: " << net_error
            << ", http_status_code: " << http_status_code_;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::nullopt, std::nullopt));
    return;
  }

  std::optional<PersistedData::Cookie> logging_cookie =
      ExtractEventLoggingCookie(clock_->Now(), header_set_cookie);
  if (logging_cookie) {
    persisted_data_->SetRemoteLoggingCookie(*logging_cookie);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), http_status_code_, response_body));
}

Omaha4UsageStatsMetadata RemoteLoggingDelegate::GetMetadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proto::Omaha4UsageStatsMetadata metadata;

  metadata.set_platform(configurator_->GetOSLongName());
  metadata.set_os_version(base::SysInfo::OperatingSystemVersion());
  metadata.set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
  metadata.set_o4_omaha_cohort_id(persisted_data_->GetCohort(kUpdaterAppId));
  metadata.set_app_version(kUpdaterVersion);
  metadata.set_is_machine(IsSystemInstall(scope_));
  metadata.set_is_cbcm_managed(is_cloud_managed_);
  std::optional<bool> externally_managed =
      configurator_->IsMachineExternallyManaged();
  if (externally_managed.has_value()) {
    metadata.set_is_domain_joined(*externally_managed);
  }
  return metadata;
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
