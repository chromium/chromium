// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_logger.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
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
#include "base/time/default_clock.h"
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

proto::Omaha4UsageStatsMetadata GetMetadata(UpdaterScope scope,
                                            const Configurator& configurator,
                                            bool is_cloud_managed) {
  proto::Omaha4UsageStatsMetadata metadata;

  metadata.set_platform(configurator.GetOSLongName());
  metadata.set_os_version(base::SysInfo::OperatingSystemVersion());
  metadata.set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
  metadata.set_o4_omaha_cohort_id(
      configurator.GetPersistedData()->GetCohort(kUpdaterAppId));
  metadata.set_app_version(kUpdaterVersion);
  metadata.set_is_machine(IsSystemInstall(scope));
  metadata.set_is_cbcm_managed(is_cloud_managed);
  std::optional<bool> externally_managed =
      configurator.IsMachineExternallyManaged();
  if (externally_managed.has_value()) {
    metadata.set_is_domain_joined(*externally_managed);
  }
  return metadata;
}

// Owns a network fetcher and keeps the factory that created it alive, because
// the factory must outlive its fetchers.
struct OwnedNetworkFetcher {
  scoped_refptr<update_client::NetworkFetcherFactory> factory;
  std::unique_ptr<update_client::NetworkFetcher> fetcher;
};

}  // namespace

using HttpRequestCallback =
    base::OnceCallback<void(std::optional<int> http_status,
                            std::optional<std::string> response_body)>;
using ::updater::proto::Omaha4Metric;
using ::updater::proto::Omaha4UsageStatsExtension;
using ::updater::proto::Omaha4UsageStatsMetadata;

RemoteLoggingUploader::RemoteLoggingUploader(
    const GURL& event_logging_url,
    scoped_refptr<PersistedData> persisted_data,
    NetworkFetcherFactoryGetter network_fetcher_factory_getter,
    std::unique_ptr<base::Clock> clock)
    : event_logging_url_(event_logging_url),
      persisted_data_(std::move(persisted_data)),
      network_fetcher_factory_getter_(
          std::move(network_fetcher_factory_getter)),
      clock_(std::move(clock)) {}

RemoteLoggingUploader::~RemoteLoggingUploader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RemoteLoggingUploader::PostRequest(const std::string& request_body,
                                        PostRequestCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OwnedNetworkFetcher owned_fetcher;
  owned_fetcher.factory = network_fetcher_factory_getter_.Run();
  owned_fetcher.fetcher = owned_fetcher.factory->Create();
  if (!owned_fetcher.fetcher) {
    VLOG(1) << "Failed to create network fetcher for logging request";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::nullopt, std::nullopt));
    return;
  }
  const base::Time now = clock_->Now();
  // The fetcher and response code are retained by the completion callback.
  auto response_code =
      base::MakeRefCounted<base::RefCountedData<std::optional<int>>>(
          std::nullopt);
  update_client::NetworkFetcher* fetcher = owned_fetcher.fetcher.get();

  fetcher->PostRequest(
      event_logging_url_, request_body, "application/x-protobuffer",
      {{update_client::NetworkFetcher::kHeaderCookie,
        base::StrCat({"NID=", GetLoggingCookieValue(now, persisted_data_)})}},
      base::BindRepeating(
          [](base::RefCountedData<std::optional<int>>* response_code_out,
             int response_code, int64_t content_length) {
            response_code_out->data = response_code;
          },
          base::RetainedRef(response_code)),
      /*progress_callback=*/base::DoNothing(),
      base::BindOnce(
          [](base::OnceCallback<void(const std::string&)>
                 persist_logging_cookie,
             PostRequestCallback callback,
             base::RefCountedData<std::optional<int>>* response_code,
             OwnedNetworkFetcher owned_fetcher,
             std::optional<std::string> response_body, int net_error,
             const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             const std::string& header_set_cookie,
             int64_t xheader_retry_after_sec) {
            if (net_error) {
              VLOG(1) << "Upload failed due to net error " << net_error;
              VLOG_IF(1, response_code->data.has_value())
                  << "HTTP response code: " << response_code->data.value();
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), std::nullopt,
                                            std::nullopt));
              return;
            }
            // Does nothing if the uploader has been destroyed.
            std::move(persist_logging_cookie).Run(header_set_cookie);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback),
                                          response_code->data, response_body));
          },
          base::BindOnce(&RemoteLoggingUploader::PersistLoggingCookie,
                         weak_factory_.GetWeakPtr(), now),
          std::move(callback), base::RetainedRef(response_code),
          std::move(owned_fetcher)));
}

void RemoteLoggingUploader::StoreNextAllowedAttemptTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  persisted_data_->SetNextAllowedLoggingAttemptTime(time);
}

base::WeakPtr<RemoteLoggingUploader> RemoteLoggingUploader::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void RemoteLoggingUploader::PersistLoggingCookie(
    base::Time now,
    const std::string& set_cookie_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<PersistedData::Cookie> logging_cookie =
      ExtractEventLoggingCookie(now, set_cookie_value);
  if (logging_cookie) {
    persisted_data_->SetRemoteLoggingCookie(*logging_cookie);
  }
}

RemoteLoggingDelegate::RemoteLoggingDelegate(
    UpdaterScope scope,
    bool is_cloud_managed,
    const Configurator& configurator,
    base::WeakPtr<RemoteLoggingUploader> uploader)
    : metadata_(::updater::GetMetadata(scope, configurator, is_cloud_managed)),
      minimum_cooldown_(configurator.MinimumEventLoggingCooldown()),
      uploader_(std::move(uploader)),
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
      base::BindOnce(&RemoteLoggingUploader::StoreNextAllowedAttemptTime,
                     uploader_, time),
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
          [](base::WeakPtr<RemoteLoggingUploader> uploader,
             const std::string& request_body, HttpRequestCallback callback) {
            if (!uploader) {
              VLOG(1) << "The uploader is gone; dropping the logging request";
              std::move(callback).Run(std::nullopt, std::nullopt);
              return;
            }
            uploader->PostRequest(request_body, std::move(callback));
          },
          uploader_, request_body,
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

RemoteEventLogging::RemoteEventLogging(
    UpdaterScope scope,
    bool is_cloud_managed,
    const Configurator& configurator,
    const GURL& event_logging_url,
    RemoteLoggingUploader::NetworkFetcherFactoryGetter
        network_fetcher_factory_getter)
    : uploader_(std::make_unique<RemoteLoggingUploader>(
          event_logging_url,
          configurator.GetUpdaterPersistedData(),
          std::move(network_fetcher_factory_getter),
          std::make_unique<base::DefaultClock>())),
      event_logger_(UpdaterEventLogger::Create(
          std::make_unique<RemoteLoggingDelegate>(scope,
                                                  is_cloud_managed,
                                                  configurator,
                                                  uploader_->GetWeakPtr()),
          configurator.GetUpdaterPersistedData()
              ->GetNextAllowedLoggingAttemptTime(),
          /*auto_flush=*/false)) {}

RemoteEventLogging::~RemoteEventLogging() = default;

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
