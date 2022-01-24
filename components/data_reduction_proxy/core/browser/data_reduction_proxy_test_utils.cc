// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/network_delegate_impl.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "url/gurl.h"

namespace {

enum TestContextOptions {
  // Permits mocking of the underlying |DataReductionProxyConfig|.
  USE_MOCK_CONFIG = 0x1,
  // Construct, but do not initialize the |DataReductionProxySettings| object.
  // Primarily used for testing of the |DataReductionProxySettings| object
  // itself.
  SKIP_SETTINGS_INITIALIZATION = 0x2,
  // Permits mocking of the underlying |DataReductionProxyService|.
  USE_MOCK_SERVICE = 0x4,
};

}  // namespace

namespace data_reduction_proxy {

MockDataReductionProxyService::MockDataReductionProxyService(
    data_use_measurement::DataUseMeasurement* data_use_measurement,
    DataReductionProxySettings* settings,
    PrefService* prefs,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : DataReductionProxyService(settings,
                                prefs,
                                std::make_unique<TestDataStore>(),
                                data_use_measurement,
                                task_runner,
                                base::TimeDelta()) {}

MockDataReductionProxyService::~MockDataReductionProxyService() {}

TestDataReductionProxyService::TestDataReductionProxyService(
    data_use_measurement::DataUseMeasurement* data_use_measurement,
    DataReductionProxySettings* settings,
    PrefService* prefs,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner)
    : DataReductionProxyService(settings,
                                prefs,
                                std::make_unique<TestDataStore>(),
                                data_use_measurement,
                                db_task_runner,
                                base::TimeDelta()) {}

TestDataReductionProxyService::~TestDataReductionProxyService() {}

TestDataStore::TestDataStore() {}

TestDataStore::~TestDataStore() {}

DataStore::Status TestDataStore::Get(base::StringPiece key,
                                     std::string* value) {
  auto value_iter = map_.find(std::string(key));
  if (value_iter == map_.end())
    return NOT_FOUND;

  value->assign(value_iter->second);
  return OK;
}

DataStore::Status TestDataStore::Put(
    const std::map<std::string, std::string>& map) {
  for (auto iter = map.begin(); iter != map.end(); ++iter)
    map_[iter->first] = iter->second;

  return OK;
}

DataStore::Status TestDataStore::Delete(base::StringPiece key) {
  map_.erase(std::string(key));

  return OK;
}

DataStore::Status TestDataStore::RecreateDB() {
  map_.clear();

  return OK;
}

DataReductionProxyTestContext::Builder::Builder()
    : use_mock_config_(false),
      use_mock_service_(false),
      skip_settings_initialization_(false),
      data_use_measurement_(
          std::make_unique<data_use_measurement::DataUseMeasurement>(
              nullptr,
              network::TestNetworkConnectionTracker::GetInstance())) {}

DataReductionProxyTestContext::Builder::~Builder() {}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockConfig() {
  use_mock_config_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockDataReductionProxyService() {
  use_mock_service_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::SkipSettingsInitialization() {
  skip_settings_initialization_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithSettings(
    std::unique_ptr<DataReductionProxySettings> settings) {
  settings_ = std::move(settings);
  return *this;
}

std::unique_ptr<DataReductionProxyTestContext>
DataReductionProxyTestContext::Builder::Build() {
  unsigned int test_context_flags = 0;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  std::unique_ptr<TestingPrefServiceSimple> pref_service(
      new TestingPrefServiceSimple());

  if (use_mock_config_) {
    test_context_flags |= USE_MOCK_CONFIG;
  } else {
    test_context_flags ^= USE_MOCK_CONFIG;
  }


  if (!settings_)
    settings_ = std::make_unique<DataReductionProxySettings>(false);
  if (skip_settings_initialization_) {
    test_context_flags |= SKIP_SETTINGS_INITIALIZATION;
  }

  pref_service->registry()->RegisterBooleanPref(prefs::kDataSaverEnabled,
                                                false);
  RegisterSimpleProfilePrefs(pref_service->registry());

  std::unique_ptr<DataReductionProxyService> service;
  if (use_mock_service_) {
    test_context_flags |= USE_MOCK_SERVICE;
    service = std::make_unique<MockDataReductionProxyService>(
        data_use_measurement_.get(), settings_.get(), pref_service.get(),
        task_runner);
  } else {
    service = std::make_unique<TestDataReductionProxyService>(
        data_use_measurement_.get(), settings_.get(), pref_service.get(),
        task_runner);
  }

  std::unique_ptr<DataReductionProxyTestContext> test_context(
      new DataReductionProxyTestContext(
          std::move(data_use_measurement_), task_runner,
          std::move(pref_service), std::move(settings_), std::move(service),
          test_context_flags));

  if (!skip_settings_initialization_)
    test_context->InitSettingsWithoutCheck();

  return test_context;
}

DataReductionProxyTestContext::DataReductionProxyTestContext(
    std::unique_ptr<data_use_measurement::DataUseMeasurement>
        data_use_measurement,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<TestingPrefServiceSimple> simple_pref_service,
    std::unique_ptr<DataReductionProxySettings> settings,
    std::unique_ptr<DataReductionProxyService> service,
    unsigned int test_context_flags)
    : data_use_measurement_(std::move(data_use_measurement)),
      test_context_flags_(test_context_flags),
      task_runner_(task_runner),
      simple_pref_service_(std::move(simple_pref_service)),
      settings_(std::move(settings)),
      service_(std::move(service)) {
  DCHECK(data_use_measurement_);

  if (service_)
    data_reduction_proxy_service_ = service_.get();
  else
    data_reduction_proxy_service_ = settings_->data_reduction_proxy_service();
}

DataReductionProxyTestContext::~DataReductionProxyTestContext() {
  DestroySettings();
}

void DataReductionProxyTestContext::RegisterDataReductionProxyEnabledPref() {
  simple_pref_service_->registry()->RegisterBooleanPref(
      prefs::kDataSaverEnabled, false);
}

void DataReductionProxyTestContext::SetDataReductionProxyEnabled(bool enabled) {
  // Set the command line so that |IsDataSaverEnabledByUser| returns as expected
  // on all platforms.
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (enabled) {
    cmd->AppendSwitch(switches::kEnableDataReductionProxy);
  } else {
    cmd->RemoveSwitch(switches::kEnableDataReductionProxy);
  }

  simple_pref_service_->SetBoolean(prefs::kDataSaverEnabled, enabled);
}

bool DataReductionProxyTestContext::IsDataReductionProxyEnabled() const {
  return simple_pref_service_->GetBoolean(prefs::kDataSaverEnabled);
}

void DataReductionProxyTestContext::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void DataReductionProxyTestContext::InitSettings() {
  DCHECK(test_context_flags_ & SKIP_SETTINGS_INITIALIZATION);
  InitSettingsWithoutCheck();
}

void DataReductionProxyTestContext::DestroySettings() {
  // Force destruction of |DBDataOwner|, which lives on DB task runner and is
  // indirectly owned by |settings_|.
  if (settings_) {
    settings_.reset();
    RunUntilIdle();
  }
}

void DataReductionProxyTestContext::InitSettingsWithoutCheck() {
  DCHECK(service_);
  settings_->InitDataReductionProxySettings(simple_pref_service_.get(),
                                            std::move(service_));
}

std::unique_ptr<DataReductionProxyService>
DataReductionProxyTestContext::TakeService() {
  DCHECK(service_);
  DCHECK(test_context_flags_ & SKIP_SETTINGS_INITIALIZATION);
  return std::move(service_);
}

void DataReductionProxyTestContext::
    EnableDataReductionProxyWithSecureProxyCheckSuccess() {
  // |settings_| needs to have been initialized, since a
  // |DataReductionProxyService| is needed in order to issue the secure proxy
  // check.
  DCHECK(data_reduction_proxy_service());

  // Set the pref to cause the secure proxy check to be issued.
  SetDataReductionProxyEnabled(true);
  RunUntilIdle();
}



DataReductionProxyService*
DataReductionProxyTestContext::data_reduction_proxy_service() const {
  return data_reduction_proxy_service_;
}

TestDataReductionProxyService*
DataReductionProxyTestContext::test_data_reduction_proxy_service() const {
  DCHECK(!(test_context_flags_ & USE_MOCK_SERVICE));
  return static_cast<TestDataReductionProxyService*>(
      data_reduction_proxy_service());
}

MockDataReductionProxyService*
DataReductionProxyTestContext::mock_data_reduction_proxy_service() const {
  DCHECK(!(test_context_flags_ & SKIP_SETTINGS_INITIALIZATION));
  DCHECK(test_context_flags_ & USE_MOCK_SERVICE);
  return static_cast<MockDataReductionProxyService*>(
      data_reduction_proxy_service());
}

}  // namespace data_reduction_proxy
