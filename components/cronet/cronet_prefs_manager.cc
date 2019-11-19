// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_prefs_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/cronet/host_cache_persistence_manager.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "net/http/http_server_properties.h"
#include "net/nqe/network_qualities_prefs_manager.h"
#include "net/url_request/url_request_context_builder.h"

namespace cronet {
namespace {

// Name of the pref used for HTTP server properties persistence.
const char kHttpServerPropertiesPref[] = "net.http_server_properties";
// Name of preference directory.
const base::FilePath::CharType kPrefsDirectoryName[] =
    FILE_PATH_LITERAL("prefs");
// Name of preference file.
const base::FilePath::CharType kPrefsFileName[] =
    FILE_PATH_LITERAL("local_prefs.json");
// Current version of disk storage.
const int32_t kStorageVersion = 1;
// Version number used when the version of disk storage is unknown.
const uint32_t kStorageVersionUnknown = 0;
// Name of the pref used for host cache persistence.
const char kHostCachePref[] = "net.host_cache";
// Name of the pref used for NQE persistence.
const char kNetworkQualitiesPref[] = "net.network_qualities";

bool IsCurrentVersion(const base::FilePath& version_filepath) {
  if (!base::PathExists(version_filepath))
    return false;
  base::File version_file(version_filepath,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
  uint32_t version = kStorageVersionUnknown;
  int bytes_read =
      version_file.Read(0, reinterpret_cast<char*>(&version), sizeof(version));
  if (bytes_read != sizeof(version)) {
    DLOG(WARNING) << "Cannot read from version file.";
    return false;
  }
  return version == kStorageVersion;
}

// TODO(xunjieli): Handle failures.
void InitializeStorageDirectory(const base::FilePath& dir) {
  // Checks version file and clear old storage.
  base::FilePath version_filepath(dir.AppendASCII("version"));
  if (IsCurrentVersion(version_filepath)) {
    // The version is up to date, so there is nothing to do.
    return;
  }
  // Delete old directory recursively and create a new directory.
  // base::DeleteFileRecursively() returns true if the directory does not exist,
  // so it is fine if there is nothing on disk.
  if (!(base::DeleteFileRecursively(dir) && base::CreateDirectory(dir))) {
    DLOG(WARNING) << "Cannot purge directory.";
    return;
  }
  base::File new_version_file(version_filepath, base::File::FLAG_CREATE_ALWAYS |
                                                    base::File::FLAG_WRITE);

  if (!new_version_file.IsValid()) {
    DLOG(WARNING) << "Cannot create a version file.";
    return;
  }

  DCHECK(new_version_file.created());
  uint32_t new_version = kStorageVersion;
  int bytes_written = new_version_file.Write(
      0, reinterpret_cast<char*>(&new_version), sizeof(new_version));
  if (bytes_written != sizeof(new_version)) {
    DLOG(WARNING) << "Cannot write to version file.";
    return;
  }
  base::FilePath prefs_dir = dir.Append(kPrefsDirectoryName);
  if (!base::CreateDirectory(prefs_dir)) {
    DLOG(WARNING) << "Cannot create prefs directory";
    return;
  }
}

// Connects the HttpServerProperties's storage to the prefs.
class PrefServiceAdapter : public net::HttpServerProperties::PrefDelegate {
 public:
  explicit PrefServiceAdapter(PrefService* pref_service)
      : pref_service_(pref_service), path_(kHttpServerPropertiesPref) {
    pref_change_registrar_.Init(pref_service_);
  }

  ~PrefServiceAdapter() override {}

  // PrefDelegate implementation.
  const base::DictionaryValue* GetServerProperties() const override {
    return pref_service_->GetDictionary(path_);
  }

  void SetServerProperties(const base::DictionaryValue& value,
                           base::OnceClosure callback) override {
    pref_service_->Set(path_, value);
    if (callback)
      pref_service_->CommitPendingWrite(std::move(callback));
  }

  void WaitForPrefLoad(base::OnceClosure callback) override {
    // Notify the pref manager that settings are already loaded, as a result
    // of initializing the pref store synchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
  }

 private:
  PrefService* pref_service_;
  const std::string path_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceAdapter);
};  // class PrefServiceAdapter

class NetworkQualitiesPrefDelegateImpl
    : public net::NetworkQualitiesPrefsManager::PrefDelegate {
 public:
  // Caller must guarantee that |pref_service| outlives |this|.
  explicit NetworkQualitiesPrefDelegateImpl(PrefService* pref_service)
      : pref_service_(pref_service), lossy_prefs_writing_task_posted_(false) {
    DCHECK(pref_service_);
  }

  ~NetworkQualitiesPrefDelegateImpl() override {}

  // net::NetworkQualitiesPrefsManager::PrefDelegate implementation.
  void SetDictionaryValue(const base::DictionaryValue& value) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    pref_service_->Set(kNetworkQualitiesPref, value);
    if (lossy_prefs_writing_task_posted_)
      return;

    // Post the task that schedules the writing of the lossy prefs.
    lossy_prefs_writing_task_posted_ = true;

    // Delay after which the task that schedules the writing of the lossy prefs.
    // This is needed in case the writing of the lossy prefs is not scheduled
    // automatically. The delay was chosen so that it is large enough that it
    // does not affect the startup performance.
    static const int32_t kUpdatePrefsDelaySeconds = 10;

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &NetworkQualitiesPrefDelegateImpl::SchedulePendingLossyWrites,
            weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kUpdatePrefsDelaySeconds));
  }
  std::unique_ptr<base::DictionaryValue> GetDictionaryValue() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    UMA_HISTOGRAM_EXACT_LINEAR("NQE.Prefs.ReadCount", 1, 2);
    return pref_service_->GetDictionary(kNetworkQualitiesPref)
        ->CreateDeepCopy();
  }

 private:
  // Schedules the writing of the lossy prefs.
  void SchedulePendingLossyWrites() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    UMA_HISTOGRAM_EXACT_LINEAR("NQE.Prefs.WriteCount", 1, 2);
    pref_service_->SchedulePendingLossyWrites();
    lossy_prefs_writing_task_posted_ = false;
  }

  PrefService* pref_service_;

  // True if the task that schedules the writing of the lossy prefs has been
  // posted.
  bool lossy_prefs_writing_task_posted_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<NetworkQualitiesPrefDelegateImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(NetworkQualitiesPrefDelegateImpl);
};

}  // namespace

CronetPrefsManager::CronetPrefsManager(
    const std::string& storage_path,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    bool enable_network_quality_estimator,
    bool enable_host_cache_persistence,
    net::NetLog* net_log,
    net::URLRequestContextBuilder* context_builder) {
  DCHECK(network_task_runner->BelongsToCurrentThread());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(OS_WIN)
  base::FilePath storage_file_path(
      base::FilePath::FromUTF8Unsafe(storage_path));
#else
  base::FilePath storage_file_path(storage_path);
#endif

  // Make sure storage directory has correct version.
  {
    base::ScopedAllowBlocking allow_blocking;
    InitializeStorageDirectory(storage_file_path);
  }

  base::FilePath filepath =
      storage_file_path.Append(kPrefsDirectoryName).Append(kPrefsFileName);

  json_pref_store_ = new JsonPrefStore(filepath, std::unique_ptr<PrefFilter>(),
                                       file_task_runner);

  // Register prefs and set up the PrefService.
  PrefServiceFactory factory;
  factory.set_user_prefs(json_pref_store_);
  scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple());
  registry->RegisterDictionaryPref(kHttpServerPropertiesPref);

  if (enable_network_quality_estimator) {
    // Use lossy prefs to limit the overhead of reading/writing the prefs.
    registry->RegisterDictionaryPref(kNetworkQualitiesPref,
                                     PrefRegistry::LOSSY_PREF);
  }

  if (enable_host_cache_persistence) {
    registry->RegisterListPref(kHostCachePref);
  }

  {
    base::ScopedAllowBlocking allow_blocking;
    pref_service_ = factory.Create(registry.get());
  }

  context_builder->SetHttpServerProperties(
      std::make_unique<net::HttpServerProperties>(
          std::make_unique<PrefServiceAdapter>(pref_service_.get()), net_log));
}

CronetPrefsManager::~CronetPrefsManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void CronetPrefsManager::SetupNqePersistence(
    net::NetworkQualityEstimator* nqe) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  network_qualities_prefs_manager_ =
      std::make_unique<net::NetworkQualitiesPrefsManager>(
          std::make_unique<NetworkQualitiesPrefDelegateImpl>(
              pref_service_.get()));

  network_qualities_prefs_manager_->InitializeOnNetworkThread(nqe);
}

void CronetPrefsManager::SetupHostCachePersistence(
    net::HostCache* host_cache,
    int host_cache_persistence_delay_ms,
    net::NetLog* net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  host_cache_persistence_manager_ =
      std::make_unique<HostCachePersistenceManager>(
          host_cache, pref_service_.get(), kHostCachePref,
          base::TimeDelta::FromMilliseconds(host_cache_persistence_delay_ms),
          net_log);
}

void CronetPrefsManager::PrepareForShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (pref_service_)
    pref_service_->CommitPendingWrite();

  // Shutdown managers on the Pref sequence.
  if (network_qualities_prefs_manager_)
    network_qualities_prefs_manager_->ShutdownOnPrefSequence();

  host_cache_persistence_manager_.reset();
}

}  // namespace cronet
