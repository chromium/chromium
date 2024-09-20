// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_metrics_service_client.h"

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/base/version.h"
#include "components/metrics/client_info.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/persistent_synthetic_trial_observer.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "chromecast/base/android/dumpstate_writer.h"
#endif

namespace chromecast {
namespace metrics {

namespace {

const int kStandardUploadIntervalMinutes = 5;

const char kMetricsOldClientID[] = "user_experience_metrics.client_id";

#if BUILDFLAG(IS_ANDROID)
const char kClientIdName[] = "Client ID";
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

const struct ChannelMap {
  const char* chromecast_channel;
  const ::metrics::SystemProfileProto::Channel chrome_channel;
} kMetricsChannelMap[] = {
    {"canary-channel", ::metrics::SystemProfileProto::CHANNEL_CANARY},
    {"dev-channel", ::metrics::SystemProfileProto::CHANNEL_DEV},
    {"developer-channel", ::metrics::SystemProfileProto::CHANNEL_DEV},
    {"beta-channel", ::metrics::SystemProfileProto::CHANNEL_BETA},
    {"dogfood-channel", ::metrics::SystemProfileProto::CHANNEL_BETA},
    {"stable-channel", ::metrics::SystemProfileProto::CHANNEL_STABLE},
};

::metrics::SystemProfileProto::Channel GetReleaseChannelFromUpdateChannelName(
    const std::string& channel_name) {
  if (channel_name.empty())
    return ::metrics::SystemProfileProto::CHANNEL_UNKNOWN;

  for (const auto& channel_map : kMetricsChannelMap) {
    if (channel_name.compare(channel_map.chromecast_channel) == 0)
      return channel_map.chrome_channel;
  }

  // Any non-empty channel name is considered beta channel
  return ::metrics::SystemProfileProto::CHANNEL_BETA;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

}  // namespace

void CastMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kMetricsOldClientID, std::string());
}

variations::SyntheticTrialRegistry*
CastMetricsServiceClient::GetSyntheticTrialRegistry() {
  return synthetic_trial_registry_.get();
}

::metrics::MetricsService* CastMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

void CastMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  client_id_ = client_id;
  LOG(INFO) << "Metrics client ID set: " << client_id;
  if (delegate_)
    delegate_->SetMetricsClientId(client_id);
#if BUILDFLAG(IS_ANDROID)
  DumpstateWriter::AddDumpValue(kClientIdName, client_id);
#endif
}

void CastMetricsServiceClient::StoreClientInfo(
    const ::metrics::ClientInfo& client_info) {
  // TODO(gfhuang): |force_client_id_| logic is super ugly, we should refactor
  // to align load/save logic of |force_client_id_| with Load/StoreClientInfo.
  // Currently it's lumped inside SetMetricsClientId(client_id).
}

std::unique_ptr<::metrics::ClientInfo>
CastMetricsServiceClient::LoadClientInfo() {
  std::unique_ptr<::metrics::ClientInfo> client_info(new ::metrics::ClientInfo);
  client_info_loaded_ = true;

  // kMetricsIsNewClientID would be missing if either the device was just
  // FDR'ed, or it is on pre-v1.2 build.
  if (!pref_service_->GetBoolean(prefs::kMetricsIsNewClientID)) {
    // If the old client id exists, the device must be on pre-v1.2 build,
    // instead of just being FDR'ed.
    if (!pref_service_->GetString(kMetricsOldClientID).empty()) {
      // Force old client id to be regenerated. See b/9487011.
      client_info->client_id =
          base::Uuid::GenerateRandomV4().AsLowercaseString();
      pref_service_->SetBoolean(prefs::kMetricsIsNewClientID, true);
      return client_info;
    }
    // else the device was just FDR'ed, pass through.
  }

  // Use "forced" client ID if available.
  if (!force_client_id_.empty() &&
      base::Uuid::ParseCaseInsensitive(force_client_id_).is_valid()) {
    client_info->client_id = force_client_id_;
    return client_info;
  }

  if (force_client_id_.empty()) {
    LOG(WARNING) << "Empty client id from platform,"
                 << " assuming this is the first boot up of a new device.";
  } else {
    LOG(ERROR) << "Invalid client id from platform: " << force_client_id_
               << " from platform.";
  }
  return nullptr;
}

int32_t CastMetricsServiceClient::GetProduct() {
  // Chromecast currently uses the same product identifier as Chrome.
  return ::metrics::ChromeUserMetricsExtension::CAST;
}

std::string CastMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

const network_time::NetworkTimeTracker*
CastMetricsServiceClient::GetNetworkTimeTracker() {
  return nullptr;
}

bool CastMetricsServiceClient::GetBrand(std::string* brand_code) {
  return false;
}

::metrics::SystemProfileProto::Channel CastMetricsServiceClient::GetChannel() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  switch (cast_sys_info_->GetBuildType()) {
    case CastSysInfo::BUILD_ENG:
      return ::metrics::SystemProfileProto::CHANNEL_UNKNOWN;
    case CastSysInfo::BUILD_BETA:
      return ::metrics::SystemProfileProto::CHANNEL_BETA;
    case CastSysInfo::BUILD_PRODUCTION:
      return ::metrics::SystemProfileProto::CHANNEL_STABLE;
  }
  NOTREACHED();
#else
  // Use the system (or signed) release channel here to avoid the noise in the
  // metrics caused by the virtual channel which could be temporary or
  // arbitrary.
  return GetReleaseChannelFromUpdateChannelName(
      cast_sys_info_->GetSystemReleaseChannel());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
}

bool CastMetricsServiceClient::IsExtendedStableChannel() {
  return false;  // Not supported on Chromecast.
}

std::string CastMetricsServiceClient::GetVersionString() {
  int build_number;
  if (!base::StringToInt(CAST_BUILD_INCREMENTAL, &build_number))
    build_number = 0;

  // Sample result: 31.0.1650.0-K15386-devel
  std::string version_string(PRODUCT_VERSION);

  version_string.append("-K");
  version_string.append(base::NumberToString(build_number));

  const ::metrics::SystemProfileProto::Channel channel = GetChannel();
  CHECK(!CAST_IS_DEBUG_BUILD() ||
        channel != ::metrics::SystemProfileProto::CHANNEL_STABLE);
  const bool is_official_build =
      build_number > 0 && !CAST_IS_DEBUG_BUILD() &&
      channel != ::metrics::SystemProfileProto::CHANNEL_UNKNOWN;
  if (!is_official_build)
    version_string.append("-devel");

  return version_string;
}

void CastMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  if (collect_final_metrics_cb_)
    collect_final_metrics_cb_.Run(std::move(done_callback));
  else
    std::move(done_callback).Run();
}

void CastMetricsServiceClient::SetCallbacks(
    base::RepeatingCallback<void(base::OnceClosure)> collect_final_metrics_cb,
    base::RepeatingCallback<void(base::OnceClosure)> external_events_cb) {
  collect_final_metrics_cb_ = collect_final_metrics_cb;
  external_events_cb_ = external_events_cb;
}

GURL CastMetricsServiceClient::GetMetricsServerUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOverrideMetricsUploadUrl)) {
    return GURL(
        command_line->GetSwitchValueASCII(switches::kOverrideMetricsUploadUrl));
  }
  // Note: This uses the old metrics service URL because some server-side
  // provisioning is needed to support the extra Cast traffic on the new URL.
  return GURL(::metrics::kOldMetricsServerUrl);
}

std::unique_ptr<::metrics::MetricsLogUploader>
CastMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    std::string_view mime_type,
    ::metrics::MetricsLogUploader::MetricServiceType service_type,
    const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  return std::make_unique<::metrics::NetMetricsLogUploader>(
      url_loader_factory_, server_url, insecure_server_url, mime_type,
      service_type, on_upload_complete);
}

base::TimeDelta CastMetricsServiceClient::GetStandardUploadInterval() {
  return base::Minutes(kStandardUploadIntervalMinutes);
}

::metrics::MetricsLogStore::StorageLimits
CastMetricsServiceClient::GetStorageLimits() const {
  auto limits = ::metrics::MetricsServiceClient::GetStorageLimits();
  if (delegate_)
    delegate_->ApplyMetricsStorageLimits(&limits);
  return limits;
}

bool CastMetricsServiceClient::IsConsentGiven() const {
  return pref_service_->GetBoolean(prefs::kOptInStats);
}

bool CastMetricsServiceClient::IsReportingEnabled() const {
  // Recording metrics is controlled by the opt-in stats preference
  // (`IsConsentGiven()`), but reporting them to Google is controlled by
  // ToS being accepted.
  return pref_service_->GetBoolean(prefs::kTosAccepted) &&
         ::metrics::EnabledStateProvider::IsReportingEnabled();
}

void CastMetricsServiceClient::UpdateMetricsServiceState() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastMetricsServiceClient::UpdateMetricsServiceState,
                       base::Unretained(this)));
    return;
  }

  if (IsConsentGiven()) {
    metrics_service_->Start();
    if (!IsReportingEnabled()) {
      // Metrics are only reported after ToS have been accepted. If usage
      // reporting is enabled, but ToS is not accepted, we can record metrics
      // but must not report/upload them.
      //
      // `MetricsServiceImpl::Start()` will start recording and reporting.
      // We must call `DisableReporting()` which will update the internal
      // state machine of the reporting service and stop the upload scheduler
      // from running.
      metrics_service_->DisableReporting();
    }
  } else {
    metrics_service_->Stop();
  }
}

void CastMetricsServiceClient::DisableMetricsService() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastMetricsServiceClient::DisableMetricsService,
                       base::Unretained(this)));
    return;
  }
  metrics_service_->Stop();
}

CastMetricsServiceClient::CastMetricsServiceClient(
    CastMetricsServiceDelegate* delegate,
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(delegate),
      pref_service_(pref_service),
      client_info_loaded_(false),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      url_loader_factory_(url_loader_factory),
      cast_sys_info_(CreateSysInfo()) {}

CastMetricsServiceClient::~CastMetricsServiceClient() = default;

void CastMetricsServiceClient::OnApplicationNotIdle() {
  metrics_service_->OnApplicationNotIdle();
}

void CastMetricsServiceClient::SetForceClientId(const std::string& client_id) {
  DCHECK(force_client_id_.empty());
  DCHECK(!client_info_loaded_)
      << "Force client ID must be set before client info is loaded.";
  force_client_id_ = client_id;
  SetMetricsClientId(force_client_id_);
}

void CastMetricsServiceClient::InitializeMetricsService() {
  DCHECK(!metrics_state_manager_);
  metrics_state_manager_ = ::metrics::MetricsStateManager::Create(
      pref_service_, this, std::wstring(),
      // Pass an empty file path since Chromecast does not use the Variations
      // framework.
      /*user_data_dir=*/base::FilePath(),
      ::metrics::StartupVisibility::kUnknown, ::metrics::EntropyParams{},
      base::BindRepeating(&CastMetricsServiceClient::StoreClientInfo,
                          base::Unretained(this)),
      base::BindRepeating(&CastMetricsServiceClient::LoadClientInfo,
                          base::Unretained(this)));

  // Check that the FieldTrialList already exists. This happens in
  // CastMainDelegate::PostEarlyInitialization().
  DCHECK(base::FieldTrialList::GetInstance());
  // Perform additional setup that should be done after the FieldTrialList, the
  // MetricsStateManager, and its CleanExitBeacon exist. Since the list already
  // exists, the entropy provider type is unused.
  // TODO(crbug.com/40791269): Make Chromecast consistent with other platforms.
  // I.e. create the FieldTrialList and the MetricsStateManager around the same
  // time.
  metrics_state_manager_->InstantiateFieldTrialList();

  synthetic_trial_registry_ =
      std::make_unique<variations::SyntheticTrialRegistry>();
  synthetic_trial_observation_.Observe(synthetic_trial_registry_.get());

  metrics_service_.reset(new ::metrics::MetricsService(
      metrics_state_manager_.get(), this, pref_service_));

  // Always create a client id as it may also be used by crash reporting,
  // (indirectly) included in feedback, and can be queried during setup. For UMA
  // and crash reporting, associated opt-in settings control sending reports as
  // directed by the user. For setup (which also communicates the user's opt-in
  // preferences), report the client id and expect setup to handle the current
  // opt-in value.
  metrics_state_manager_->ForceClientIdCreation();
  // Populate |client_id| in other component parts.
  SetMetricsClientId(metrics_state_manager_->client_id());
}

void CastMetricsServiceClient::StartMetricsService() {
  if (delegate_)
    delegate_->RegisterMetricsProviders(metrics_service_.get());

  metrics_service_->InitializeMetricsRecordingState();
#if !BUILDFLAG(IS_ANDROID)
  // Signal that the session has not yet exited cleanly. We later signal that
  // the session exited cleanly via MetricsService::LogCleanShutdown().
  // TODO(crbug.com/40766116): See whether this can be called even earlier.
  metrics_state_manager_->LogHasSessionShutdownCleanly(false);
#endif  // !BUILDFLAG(IS_ANDROID)

  UpdateMetricsServiceState();
}

void CastMetricsServiceClient::Finalize() {
  metrics_service_->Stop();
}

void CastMetricsServiceClient::ProcessExternalEvents(base::OnceClosure cb) {
  if (external_events_cb_)
    external_events_cb_.Run(std::move(cb));
  else
    std::move(cb).Run();
}

}  // namespace metrics
}  // namespace chromecast
