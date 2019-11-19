// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/update_client/action_runner.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/configurator.h"
#include "components/update_client/network.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"

// The state machine representing how a CRX component changes during an update.
//
//     +------------------------- kNew
//     |                            |
//     |                            V
//     |                        kChecking
//     |                            |
//     V                error       V     no           no
//  kUpdateError <------------- [update?] -> [action?] -> kUpToDate  kUpdated
//     ^                            |           |            ^        ^
//     |                        yes |           | yes        |        |
//     |                            V           |            |        |
//     |                        kCanUpdate      +--------> kRun       |
//     |                            |                                 |
//     |                no          V                                 |
//     |               +-<- [differential update?]                    |
//     |               |               |                              |
//     |               |           yes |                              |
//     |               | error         V                              |
//     |               +-<----- kDownloadingDiff            kRun---->-+
//     |               |               |                     ^        |
//     |               |               |                 yes |        |
//     |               | error         V                     |        |
//     |               +-<----- kUpdatingDiff ---------> [action?] ->-+
//     |               |                                     ^     no
//     |    error      V                                     |
//     +-<-------- kDownloading                              |
//     |               |                                     |
//     |               |                                     |
//     |    error      V                                     |
//     +-<-------- kUpdating --------------------------------+

namespace update_client {

namespace {

using InstallOnBlockingTaskRunnerCompleteCallback = base::OnceCallback<
    void(ErrorCategory error_category, int error_code, int extra_code1)>;

void InstallComplete(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    InstallOnBlockingTaskRunnerCompleteCallback callback,
    const base::FilePath& unpack_path,
    const CrxInstaller::Result& result) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
             InstallOnBlockingTaskRunnerCompleteCallback callback,
             const base::FilePath& unpack_path,
             const CrxInstaller::Result& result) {
            base::DeleteFileRecursively(unpack_path);
            const ErrorCategory error_category =
                result.error ? ErrorCategory::kInstall : ErrorCategory::kNone;
            main_task_runner->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), error_category,
                                          static_cast<int>(result.error),
                                          result.extended_error));
          },
          main_task_runner, std::move(callback), unpack_path, result));
}

void InstallOnBlockingTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    const base::FilePath& unpack_path,
    const std::string& public_key,
    const std::string& fingerprint,
    scoped_refptr<CrxInstaller> installer,
    InstallOnBlockingTaskRunnerCompleteCallback callback) {
  DCHECK(base::DirectoryExists(unpack_path));

  // Acquire the ownership of the |unpack_path|.
  base::ScopedTempDir unpack_path_owner;
  ignore_result(unpack_path_owner.Set(unpack_path));

  if (static_cast<int>(fingerprint.size()) !=
      base::WriteFile(
          unpack_path.Append(FILE_PATH_LITERAL("manifest.fingerprint")),
          fingerprint.c_str(), base::checked_cast<int>(fingerprint.size()))) {
    const CrxInstaller::Result result(InstallError::FINGERPRINT_WRITE_FAILED);
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ErrorCategory::kInstall,
                       static_cast<int>(result.error), result.extended_error));
    return;
  }

  installer->Install(
      unpack_path, public_key,
      base::BindOnce(&InstallComplete, main_task_runner, std::move(callback),
                     unpack_path_owner.Take()));
}

void UnpackCompleteOnBlockingTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    const base::FilePath& crx_path,
    const std::string& fingerprint,
    scoped_refptr<CrxInstaller> installer,
    InstallOnBlockingTaskRunnerCompleteCallback callback,
    const ComponentUnpacker::Result& result) {
  update_client::DeleteFileAndEmptyParentDirectory(crx_path);

  if (result.error != UnpackerError::kNone) {
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ErrorCategory::kUnpack,
                       static_cast<int>(result.error), result.extended_error));
    return;
  }

  base::PostTask(FROM_HERE, kTaskTraits,
                 base::BindOnce(&InstallOnBlockingTaskRunner, main_task_runner,
                                result.unpack_path, result.public_key,
                                fingerprint, installer, std::move(callback)));
}

void StartInstallOnBlockingTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& crx_path,
    const std::string& fingerprint,
    scoped_refptr<CrxInstaller> installer,
    std::unique_ptr<Unzipper> unzipper_,
    scoped_refptr<Patcher> patcher_,
    crx_file::VerifierFormat crx_format,
    InstallOnBlockingTaskRunnerCompleteCallback callback) {
  auto unpacker = base::MakeRefCounted<ComponentUnpacker>(
      pk_hash, crx_path, installer, std::move(unzipper_), std::move(patcher_),
      crx_format);

  unpacker->Unpack(base::BindOnce(&UnpackCompleteOnBlockingTaskRunner,
                                  main_task_runner, crx_path, fingerprint,
                                  installer, std::move(callback)));
}

// Returns a string literal corresponding to the value of the downloader |d|.
const char* DownloaderToString(CrxDownloader::DownloadMetrics::Downloader d) {
  switch (d) {
    case CrxDownloader::DownloadMetrics::kUrlFetcher:
      return "direct";
    case CrxDownloader::DownloadMetrics::kBits:
      return "bits";
    default:
      return "unknown";
  }
}

}  // namespace

Component::Component(const UpdateContext& update_context, const std::string& id)
    : id_(id),
      state_(std::make_unique<StateNew>(this)),
      update_context_(update_context) {}

Component::~Component() {}

scoped_refptr<Configurator> Component::config() const {
  return update_context_.config;
}

std::string Component::session_id() const {
  return update_context_.session_id;
}

bool Component::is_foreground() const {
  return update_context_.is_foreground;
}

void Component::Handle(CallbackHandleComplete callback_handle_complete) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_);

  callback_handle_complete_ = std::move(callback_handle_complete);

  state_->Handle(
      base::BindOnce(&Component::ChangeState, base::Unretained(this)));
}

void Component::ChangeState(std::unique_ptr<State> next_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  previous_state_ = state();
  if (next_state)
    state_ = std::move(next_state);
  else
    is_handled_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, std::move(callback_handle_complete_));
}

CrxUpdateItem Component::GetCrxUpdateItem() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  CrxUpdateItem crx_update_item;
  crx_update_item.state = state_->state();
  crx_update_item.id = id_;
  if (crx_component_)
    crx_update_item.component = *crx_component_;
  crx_update_item.last_check = last_check_;
  crx_update_item.next_version = next_version_;
  crx_update_item.next_fp = next_fp_;
  crx_update_item.error_category = error_category_;
  crx_update_item.error_code = error_code_;
  crx_update_item.extra_code1 = extra_code1_;

  return crx_update_item;
}

void Component::SetParseResult(const ProtocolParser::Result& result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK_EQ(0, update_check_error_);

  status_ = result.status;
  action_run_ = result.action_run;

  if (result.manifest.packages.empty())
    return;

  next_version_ = base::Version(result.manifest.version);
  const auto& package = result.manifest.packages.front();
  next_fp_ = package.fingerprint;

  // Resolve the urls by combining the base urls with the package names.
  for (const auto& crx_url : result.crx_urls) {
    const GURL url = crx_url.Resolve(package.name);
    if (url.is_valid())
      crx_urls_.push_back(url);
  }
  for (const auto& crx_diffurl : result.crx_diffurls) {
    const GURL url = crx_diffurl.Resolve(package.namediff);
    if (url.is_valid())
      crx_diffurls_.push_back(url);
  }

  hash_sha256_ = package.hash_sha256;
  hashdiff_sha256_ = package.hashdiff_sha256;
}

void Component::Uninstall(const base::Version& version, int reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK_EQ(ComponentState::kNew, state());

  crx_component_ = CrxComponent();
  crx_component_->version = version;

  previous_version_ = version;
  next_version_ = base::Version("0");
  extra_code1_ = reason;

  state_ = std::make_unique<StateUninstalled>(this);
}

void Component::SetUpdateCheckResult(
    const base::Optional<ProtocolParser::Result>& result,
    ErrorCategory error_category,
    int error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(ComponentState::kChecking, state());

  error_category_ = error_category;
  error_code_ = error;
  if (result)
    SetParseResult(result.value());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, std::move(update_check_complete_));
}

bool Component::CanDoBackgroundDownload() const {
  // Foreground component updates are always downloaded in foreground.
  return !is_foreground() &&
         (crx_component() && crx_component()->allows_background_download) &&
         update_context_.config->EnabledBackgroundDownloader();
}

void Component::AppendEvent(base::Value event) {
  events_.push_back(std::move(event));
}

void Component::NotifyObservers(UpdateClient::Observer::Events event) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  update_context_.notify_observers_callback.Run(event, id_);
}

base::TimeDelta Component::GetUpdateDuration() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (update_begin_.is_null())
    return base::TimeDelta();

  const base::TimeDelta update_cost(base::TimeTicks::Now() - update_begin_);
  DCHECK_GE(update_cost, base::TimeDelta());
  const base::TimeDelta max_update_delay =
      base::TimeDelta::FromSeconds(update_context_.config->UpdateDelay());
  return std::min(update_cost, max_update_delay);
}

base::Value Component::MakeEventUpdateComplete() const {
  base::Value event(base::Value::Type::DICTIONARY);
  event.SetKey("eventtype", base::Value(3));
  event.SetKey(
      "eventresult",
      base::Value(static_cast<int>(state() == ComponentState::kUpdated)));
  if (error_category() != ErrorCategory::kNone)
    event.SetKey("errorcat", base::Value(static_cast<int>(error_category())));
  if (error_code())
    event.SetKey("errorcode", base::Value(error_code()));
  if (extra_code1())
    event.SetKey("extracode1", base::Value(extra_code1()));
  if (HasDiffUpdate(*this)) {
    const int diffresult = static_cast<int>(!diff_update_failed());
    event.SetKey("diffresult", base::Value(diffresult));
  }
  if (diff_error_category() != ErrorCategory::kNone) {
    const int differrorcat = static_cast<int>(diff_error_category());
    event.SetKey("differrorcat", base::Value(differrorcat));
  }
  if (diff_error_code())
    event.SetKey("differrorcode", base::Value(diff_error_code()));
  if (diff_extra_code1())
    event.SetKey("diffextracode1", base::Value(diff_extra_code1()));
  if (!previous_fp().empty())
    event.SetKey("previousfp", base::Value(previous_fp()));
  if (!next_fp().empty())
    event.SetKey("nextfp", base::Value(next_fp()));
  DCHECK(previous_version().IsValid());
  event.SetKey("previousversion", base::Value(previous_version().GetString()));
  if (next_version().IsValid())
    event.SetKey("nextversion", base::Value(next_version().GetString()));
  return event;
}

base::Value Component::MakeEventDownloadMetrics(
    const CrxDownloader::DownloadMetrics& dm) const {
  base::Value event(base::Value::Type::DICTIONARY);
  event.SetKey("eventtype", base::Value(14));
  event.SetKey("eventresult", base::Value(static_cast<int>(dm.error == 0)));
  event.SetKey("downloader", base::Value(DownloaderToString(dm.downloader)));
  if (dm.error)
    event.SetKey("errorcode", base::Value(dm.error));
  event.SetKey("url", base::Value(dm.url.spec()));

  // -1 means that the  byte counts are not known.
  if (dm.total_bytes != -1 && dm.total_bytes < kProtocolMaxInt)
    event.SetKey("total", base::Value(static_cast<double>(dm.total_bytes)));
  if (dm.downloaded_bytes != -1 && dm.total_bytes < kProtocolMaxInt) {
    event.SetKey("downloaded",
                 base::Value(static_cast<double>(dm.downloaded_bytes)));
  }
  if (dm.download_time_ms && dm.total_bytes < kProtocolMaxInt) {
    event.SetKey("download_time_ms",
                 base::Value(static_cast<double>(dm.download_time_ms)));
  }
  DCHECK(previous_version().IsValid());
  event.SetKey("previousversion", base::Value(previous_version().GetString()));
  if (next_version().IsValid())
    event.SetKey("nextversion", base::Value(next_version().GetString()));
  return event;
}

base::Value Component::MakeEventUninstalled() const {
  DCHECK(state() == ComponentState::kUninstalled);
  base::Value event(base::Value::Type::DICTIONARY);
  event.SetKey("eventtype", base::Value(4));
  event.SetKey("eventresult", base::Value(1));
  if (extra_code1())
    event.SetKey("extracode1", base::Value(extra_code1()));
  DCHECK(previous_version().IsValid());
  event.SetKey("previousversion", base::Value(previous_version().GetString()));
  DCHECK(next_version().IsValid());
  event.SetKey("nextversion", base::Value(next_version().GetString()));
  return event;
}

base::Value Component::MakeEventActionRun(bool succeeded,
                                          int error_code,
                                          int extra_code1) const {
  base::Value event(base::Value::Type::DICTIONARY);
  event.SetKey("eventtype", base::Value(42));
  event.SetKey("eventresult", base::Value(static_cast<int>(succeeded)));
  if (error_code)
    event.SetKey("errorcode", base::Value(error_code));
  if (extra_code1)
    event.SetKey("extracode1", base::Value(extra_code1));
  return event;
}

std::vector<base::Value> Component::GetEvents() const {
  std::vector<base::Value> events;
  for (const auto& event : events_)
    events.push_back(event.Clone());
  return events;
}

Component::State::State(Component* component, ComponentState state)
    : state_(state), component_(*component) {}

Component::State::~State() {}

void Component::State::Handle(CallbackNextState callback_next_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_next_state_ = std::move(callback_next_state);

  DoHandle();
}

void Component::State::TransitionState(std::unique_ptr<State> next_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(next_state);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_next_state_), std::move(next_state)));
}

void Component::State::EndState() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_next_state_), nullptr));
}

Component::StateNew::StateNew(Component* component)
    : State(component, ComponentState::kNew) {}

Component::StateNew::~StateNew() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateNew::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = State::component();
  if (component.crx_component()) {
    TransitionState(std::make_unique<StateChecking>(&component));
  } else {
    component.error_code_ = static_cast<int>(Error::CRX_NOT_FOUND);
    component.error_category_ = ErrorCategory::kService;
    TransitionState(std::make_unique<StateUpdateError>(&component));
  }
}

Component::StateChecking::StateChecking(Component* component)
    : State(component, ComponentState::kChecking) {}

Component::StateChecking::~StateChecking() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// Unlike how other states are handled, this function does not change the
// state right away. The state transition happens when the UpdateChecker
// calls Component::UpdateCheckComplete and |update_check_complete_| is invoked.
// This is an artifact of how multiple components must be checked for updates
// together but the state machine defines the transitions for one component
// at a time.
void Component::StateChecking::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = State::component();
  DCHECK(component.crx_component());

  component.last_check_ = base::TimeTicks::Now();
  component.update_check_complete_ = base::BindOnce(
      &Component::StateChecking::UpdateCheckComplete, base::Unretained(this));

  component.NotifyObservers(Events::COMPONENT_CHECKING_FOR_UPDATES);
}

void Component::StateChecking::UpdateCheckComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto& component = State::component();
  if (!component.error_code_) {
    if (component.status_ == "ok") {
      TransitionState(std::make_unique<StateCanUpdate>(&component));
      return;
    }

    if (component.status_ == "noupdate") {
      if (component.action_run_.empty())
        TransitionState(std::make_unique<StateUpToDate>(&component));
      else
        TransitionState(std::make_unique<StateRun>(&component));
      return;
    }
  }

  TransitionState(std::make_unique<StateUpdateError>(&component));
}

Component::StateUpdateError::StateUpdateError(Component* component)
    : State(component, ComponentState::kUpdateError) {}

Component::StateUpdateError::~StateUpdateError() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateUpdateError::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = State::component();

  DCHECK_NE(ErrorCategory::kNone, component.error_category_);
  DCHECK_NE(0, component.error_code_);

  // Create an event only when the server response included an update.
  if (component.IsUpdateAvailable())
    component.AppendEvent(component.MakeEventUpdateComplete());

  EndState();
  component.NotifyObservers(Events::COMPONENT_UPDATE_ERROR);
}

Component::StateCanUpdate::StateCanUpdate(Component* component)
    : State(component, ComponentState::kCanUpdate) {}

Component::StateCanUpdate::~StateCanUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateCanUpdate::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = State::component();
  DCHECK(component.crx_component());

  component.is_update_available_ = true;
  component.NotifyObservers(Events::COMPONENT_UPDATE_FOUND);

  if (component.crx_component()
          ->supports_group_policy_enable_component_updates &&
      !component.update_context_.enabled_component_updates) {
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::UPDATE_DISABLED);
    component.extra_code1_ = 0;
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  // Start computing the cost of the this update from here on.
  component.update_begin_ = base::TimeTicks::Now();

  if (CanTryDiffUpdate())
    TransitionState(std::make_unique<StateDownloadingDiff>(&component));
  else
    TransitionState(std::make_unique<StateDownloading>(&component));
}

// Returns true if a differential update is available, it has not failed yet,
// and the configuration allows this update.
bool Component::StateCanUpdate::CanTryDiffUpdate() const {
  const auto& component = Component::State::component();
  return HasDiffUpdate(component) && !component.diff_error_code_ &&
         component.update_context_.config->EnabledDeltas();
}

Component::StateUpToDate::StateUpToDate(Component* component)
    : State(component, ComponentState::kUpToDate) {}

Component::StateUpToDate::~StateUpToDate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateUpToDate::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = State::component();
  DCHECK(component.crx_component());

  component.NotifyObservers(Events::COMPONENT_NOT_UPDATED);
  EndState();
}

Component::StateDownloadingDiff::StateDownloadingDiff(Component* component)
    : State(component, ComponentState::kDownloadingDiff) {}

Component::StateDownloadingDiff::~StateDownloadingDiff() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateDownloadingDiff::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& component = Component::State::component();
  const auto& update_context = component.update_context_;

  DCHECK(component.crx_component());

  crx_downloader_ = update_context.crx_downloader_factory(
      component.CanDoBackgroundDownload(),
      update_context.config->GetNetworkFetcherFactory());

  const auto& id = component.id_;
  crx_downloader_->set_progress_callback(
      base::Bind(&Component::StateDownloadingDiff::DownloadProgress,
                 base::Unretained(this), id));
  crx_downloader_->StartDownload(
      component.crx_diffurls_, component.hashdiff_sha256_,
      base::BindOnce(&Component::StateDownloadingDiff::DownloadComplete,
                     base::Unretained(this), id));

  component.NotifyObservers(Events::COMPONENT_UPDATE_DOWNLOADING);
}

// Called when progress is being made downloading a CRX. Can be called multiple
// times due to how the CRX downloader switches between different downloaders
// and fallback urls.
void Component::StateDownloadingDiff::DownloadProgress(const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  component().NotifyObservers(Events::COMPONENT_UPDATE_DOWNLOADING);
}

void Component::StateDownloadingDiff::DownloadComplete(
    const std::string& id,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = Component::State::component();
  for (const auto& download_metrics : crx_downloader_->download_metrics())
    component.AppendEvent(component.MakeEventDownloadMetrics(download_metrics));

  crx_downloader_.reset();

  if (download_result.error) {
    DCHECK(download_result.response.empty());
    component.diff_error_category_ = ErrorCategory::kDownload;
    component.diff_error_code_ = download_result.error;

    TransitionState(std::make_unique<StateDownloading>(&component));
    return;
  }

  component.crx_path_ = download_result.response;

  TransitionState(std::make_unique<StateUpdatingDiff>(&component));
}

Component::StateDownloading::StateDownloading(Component* component)
    : State(component, ComponentState::kDownloading) {}

Component::StateDownloading::~StateDownloading() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateDownloading::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& component = Component::State::component();
  const auto& update_context = component.update_context_;

  DCHECK(component.crx_component());

  crx_downloader_ = update_context.crx_downloader_factory(
      component.CanDoBackgroundDownload(),
      update_context.config->GetNetworkFetcherFactory());

  const auto& id = component.id_;
  crx_downloader_->set_progress_callback(
      base::Bind(&Component::StateDownloading::DownloadProgress,
                 base::Unretained(this), id));
  crx_downloader_->StartDownload(
      component.crx_urls_, component.hash_sha256_,
      base::BindOnce(&Component::StateDownloading::DownloadComplete,
                     base::Unretained(this), id));

  component.NotifyObservers(Events::COMPONENT_UPDATE_DOWNLOADING);
}

// Called when progress is being made downloading a CRX. Can be called multiple
// times due to how the CRX downloader switches between different downloaders
// and fallback urls.
void Component::StateDownloading::DownloadProgress(const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  component().NotifyObservers(Events::COMPONENT_UPDATE_DOWNLOADING);
}

void Component::StateDownloading::DownloadComplete(
    const std::string& id,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = Component::State::component();

  for (const auto& download_metrics : crx_downloader_->download_metrics())
    component.AppendEvent(component.MakeEventDownloadMetrics(download_metrics));

  crx_downloader_.reset();

  if (download_result.error) {
    DCHECK(download_result.response.empty());
    component.error_category_ = ErrorCategory::kDownload;
    component.error_code_ = download_result.error;

    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  component.crx_path_ = download_result.response;

  TransitionState(std::make_unique<StateUpdating>(&component));
}

Component::StateUpdatingDiff::StateUpdatingDiff(Component* component)
    : State(component, ComponentState::kUpdatingDiff) {}

Component::StateUpdatingDiff::~StateUpdatingDiff() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateUpdatingDiff::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& component = Component::State::component();
  const auto& update_context = component.update_context_;

  DCHECK(component.crx_component());

  component.NotifyObservers(Events::COMPONENT_UPDATE_READY);

  base::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &update_client::StartInstallOnBlockingTaskRunner,
              base::ThreadTaskRunnerHandle::Get(),
              component.crx_component()->pk_hash, component.crx_path_,
              component.next_fp_, component.crx_component()->installer,
              update_context.config->GetUnzipperFactory()->Create(),
              update_context.config->GetPatcherFactory()->Create(),
              component.crx_component()->crx_format_requirement,
              base::BindOnce(&Component::StateUpdatingDiff::InstallComplete,
                             base::Unretained(this))));
}

void Component::StateUpdatingDiff::InstallComplete(ErrorCategory error_category,
                                                   int error_code,
                                                   int extra_code1) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = Component::State::component();

  component.diff_error_category_ = error_category;
  component.diff_error_code_ = error_code;
  component.diff_extra_code1_ = extra_code1;

  if (component.diff_error_code_ != 0) {
    TransitionState(std::make_unique<StateDownloading>(&component));
    return;
  }

  DCHECK_EQ(ErrorCategory::kNone, component.diff_error_category_);
  DCHECK_EQ(0, component.diff_error_code_);
  DCHECK_EQ(0, component.diff_extra_code1_);

  DCHECK_EQ(ErrorCategory::kNone, component.error_category_);
  DCHECK_EQ(0, component.error_code_);
  DCHECK_EQ(0, component.extra_code1_);

  if (component.action_run_.empty())
    TransitionState(std::make_unique<StateUpdated>(&component));
  else
    TransitionState(std::make_unique<StateRun>(&component));
}

Component::StateUpdating::StateUpdating(Component* component)
    : State(component, ComponentState::kUpdating) {}

Component::StateUpdating::~StateUpdating() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateUpdating::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& component = Component::State::component();
  const auto& update_context = component.update_context_;

  DCHECK(component.crx_component());

  component.NotifyObservers(Events::COMPONENT_UPDATE_READY);

  base::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &update_client::StartInstallOnBlockingTaskRunner,
                     base::ThreadTaskRunnerHandle::Get(),
                     component.crx_component()->pk_hash, component.crx_path_,
                     component.next_fp_, component.crx_component()->installer,
                     update_context.config->GetUnzipperFactory()->Create(),
                     update_context.config->GetPatcherFactory()->Create(),
                     component.crx_component()->crx_format_requirement,
                     base::BindOnce(&Component::StateUpdating::InstallComplete,
                                    base::Unretained(this))));
}

void Component::StateUpdating::InstallComplete(ErrorCategory error_category,
                                               int error_code,
                                               int extra_code1) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = Component::State::component();

  component.error_category_ = error_category;
  component.error_code_ = error_code;
  component.extra_code1_ = extra_code1;

  if (component.error_code_ != 0) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  DCHECK_EQ(ErrorCategory::kNone, component.error_category_);
  DCHECK_EQ(0, component.error_code_);
  DCHECK_EQ(0, component.extra_code1_);

  if (component.action_run_.empty())
    TransitionState(std::make_unique<StateUpdated>(&component));
  else
    TransitionState(std::make_unique<StateRun>(&component));
}

Component::StateUpdated::StateUpdated(Component* component)
    : State(component, ComponentState::kUpdated) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

Component::StateUpdated::~StateUpdated() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateUpdated::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = State::component();
  DCHECK(component.crx_component());

  component.crx_component_->version = component.next_version_;
  component.crx_component_->fingerprint = component.next_fp_;

  component.AppendEvent(component.MakeEventUpdateComplete());

  component.NotifyObservers(Events::COMPONENT_UPDATED);
  EndState();
}

Component::StateUninstalled::StateUninstalled(Component* component)
    : State(component, ComponentState::kUninstalled) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

Component::StateUninstalled::~StateUninstalled() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateUninstalled::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = State::component();
  DCHECK(component.crx_component());

  component.AppendEvent(component.MakeEventUninstalled());

  EndState();
}

Component::StateRun::StateRun(Component* component)
    : State(component, ComponentState::kRun) {}

Component::StateRun::~StateRun() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Component::StateRun::DoHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& component = State::component();
  DCHECK(component.crx_component());

  action_runner_ = std::make_unique<ActionRunner>(component);
  action_runner_->Run(
      base::BindOnce(&StateRun::ActionRunComplete, base::Unretained(this)));
}

void Component::StateRun::ActionRunComplete(bool succeeded,
                                            int error_code,
                                            int extra_code1) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& component = State::component();

  component.AppendEvent(
      component.MakeEventActionRun(succeeded, error_code, extra_code1));
  switch (component.previous_state_) {
    case ComponentState::kChecking:
      TransitionState(std::make_unique<StateUpToDate>(&component));
      return;
    case ComponentState::kUpdating:
    case ComponentState::kUpdatingDiff:
      TransitionState(std::make_unique<StateUpdated>(&component));
      return;
    default:
      break;
  }
  NOTREACHED();
}

}  // namespace update_client
