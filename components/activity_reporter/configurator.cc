// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/activity_reporter/configurator.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/enterprise_util.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "components/activity_reporter/constants.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/network.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_handler.h"
#include "url/gurl.h"

namespace activity_reporter {

namespace {

constexpr int kUnknown = -2;

class ActivityService final : public update_client::ActivityDataService {
 public:
  ActivityService() = default;
  ActivityService(const ActivityService&) = delete;
  ActivityService& operator=(const ActivityService&) = delete;

  // update_client::ActivityDataService:
  void GetActiveBits(const std::vector<std::string>& ids,
                     base::OnceCallback<void(const std::set<std::string>&)>
                         callback) const override {
    // This is only called when we have activity to report, assume active.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::set<std::string>{std::string{kChromeActivityId}}));
  }

  void GetAndClearActiveBits(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::set<std::string>&)> callback)
      override {
    // Other implementations of `GetActiveBits` rely on some local state to
    // track whether activity has happened. Since
    // `ActivityService::GetActiveBits` is only called when this app is
    // active, it doesn't have any local state to clear, and this function
    // just reduces to `GetActiveBits`.
    GetActiveBits(ids, std::move(callback));
  }

  int GetDaysSinceLastActive(const std::string& id) const override {
    // Always use DateLast counting, never DaysSince.
    return kUnknown;
  }

  int GetDaysSinceLastRollCall(const std::string& id) const override {
    // Always use DateLast counting, never DaysSince.
    return kUnknown;
  }
};

}  // namespace

ActivityReporterConfigurator::ActivityReporterConfigurator(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    bool per_user_install)
    : pref_service_provider_(pref_service_provider),
      persisted_data_(update_client::CreatePersistedData(
          pref_service_provider,
          std::make_unique<ActivityService>())),
      network_fetcher_factory_(network_fetcher_factory),
      channel_provider_(channel_provider),
      per_user_install_(per_user_install) {}

ActivityReporterConfigurator::~ActivityReporterConfigurator() = default;

base::TimeDelta ActivityReporterConfigurator::InitialDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(0);
}

base::TimeDelta ActivityReporterConfigurator::NextCheckDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(0);
}

base::TimeDelta ActivityReporterConfigurator::OnDemandDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(0);
}

base::TimeDelta ActivityReporterConfigurator::UpdateDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(0);
}

std::vector<GURL> ActivityReporterConfigurator::UpdateUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {GURL(kUrl)};
}

std::vector<GURL> ActivityReporterConfigurator::PingUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {GURL(kUrl)};
}

std::string ActivityReporterConfigurator::GetProdId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "activity_reporter";
}

base::Version ActivityReporterConfigurator::GetBrowserVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return version_info::GetVersion();
}

std::string ActivityReporterConfigurator::GetChannel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::string{version_info::GetChannelString(channel_provider_.Run())};
}

std::string ActivityReporterConfigurator::GetLang() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

std::string ActivityReporterConfigurator::GetOSLongName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::string{version_info::GetOSType()};
}

base::flat_map<std::string, std::string>
ActivityReporterConfigurator::ExtraRequestParams() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

std::string ActivityReporterConfigurator::GetDownloadPreference() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

scoped_refptr<update_client::NetworkFetcherFactory>
ActivityReporterConfigurator::GetNetworkFetcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
ActivityReporterConfigurator::GetCrxDownloaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Since this module calls with CheckForUpdate only, even if the server
  // returns an update, update_client should not try to apply it.
  NOTREACHED();
}

scoped_refptr<update_client::UnzipperFactory>
ActivityReporterConfigurator::GetUnzipperFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Since this module calls with CheckForUpdate only, even if the server
  // returns an update, update_client should not try to apply it.
  NOTREACHED();
}

scoped_refptr<update_client::PatcherFactory>
ActivityReporterConfigurator::GetPatcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Since this module calls with CheckForUpdate only, even if the server
  // returns an update, update_client should not try to apply it.
  NOTREACHED();
}

bool ActivityReporterConfigurator::EnabledBackgroundDownloader() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

bool ActivityReporterConfigurator::EnabledCupSigning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

PrefService* ActivityReporterConfigurator::GetPrefService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_provider_.Run();
}

update_client::PersistedData* ActivityReporterConfigurator::GetPersistedData()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return persisted_data_.get();
}

bool ActivityReporterConfigurator::IsPerUserInstall() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return per_user_install_;
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
ActivityReporterConfigurator::GetProtocolHandlerFactory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

std::optional<bool> ActivityReporterConfigurator::IsMachineExternallyManaged()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy::PlatformManagementService* service =
      policy::PlatformManagementService::GetInstance();
  return service && service->IsManaged();
}

update_client::UpdaterStateProvider
ActivityReporterConfigurator::GetUpdaterStateProvider() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindRepeating([](bool /*is_machine*/) {
    return update_client::UpdaterStateAttributes();
  });
}

scoped_refptr<update_client::CrxCache>
ActivityReporterConfigurator::GetCrxCache() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<update_client::CrxCache>(std::nullopt);
}

bool ActivityReporterConfigurator::IsConnectionMetered() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // activity_reporter won't apply updates, so it does not matter what this
  // returns.
  return false;
}

}  // namespace activity_reporter
