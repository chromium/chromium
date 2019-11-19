// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/network_delegate_impl.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
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
  // Permits mocking of the underlying |DataReductionProxyRequestOptions|.
  USE_MOCK_REQUEST_OPTIONS = 0x8,
  // Specifies the use of the |DataReductionProxyConfigServiceClient|.
  USE_CONFIG_CLIENT = 0x10,
  // Specifies the use of the |TESTDataReductionProxyConfigServiceClient|.
  USE_TEST_CONFIG_CLIENT = 0x20,
};

const char kTestKey[] = "test-key";

net::BackoffEntry::Policy kTestBackoffPolicy;

const net::BackoffEntry::Policy& GetTestBackoffPolicy() {
  kTestBackoffPolicy = data_reduction_proxy::GetBackoffPolicy();
  // Remove jitter to bring certainty in the tests.
  kTestBackoffPolicy.jitter_factor = 0;
  return kTestBackoffPolicy;
}

}  // namespace

namespace data_reduction_proxy {

TestDataReductionProxyRequestOptions::TestDataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config)
    : DataReductionProxyRequestOptions(client, version, config) {}

std::string TestDataReductionProxyRequestOptions::GetDefaultKey() const {
  return kTestKey;
}

MockDataReductionProxyRequestOptions::MockDataReductionProxyRequestOptions(
    Client client,
    DataReductionProxyConfig* config)
    : TestDataReductionProxyRequestOptions(client, "1.2.3.4", config) {}

MockDataReductionProxyRequestOptions::~MockDataReductionProxyRequestOptions() {}

TestDataReductionProxyConfigServiceClient::
    TestDataReductionProxyConfigServiceClient(
        std::unique_ptr<DataReductionProxyParams> params,
        DataReductionProxyRequestOptions* request_options,
        DataReductionProxyMutableConfigValues* config_values,
        DataReductionProxyConfig* config,
        DataReductionProxyService* service,
        network::NetworkConnectionTracker* network_connection_tracker,
        ConfigStorer config_storer,
        const net::BackoffEntry::Policy& backoff_policy)
    : DataReductionProxyConfigServiceClient(backoff_policy,
                                            request_options,
                                            config_values,
                                            config,
                                            service,
                                            network_connection_tracker,
                                            config_storer),
#if defined(OS_ANDROID)
      is_application_state_background_(false),
#endif
      tick_clock_(base::Time::UnixEpoch()),
      test_backoff_entry_(&backoff_policy, &tick_clock_) {
}

TestDataReductionProxyConfigServiceClient::
    ~TestDataReductionProxyConfigServiceClient() {}

void TestDataReductionProxyConfigServiceClient::SetNow(const base::Time& time) {
  tick_clock_.SetTime(time);
}

void TestDataReductionProxyConfigServiceClient::SetCustomReleaseTime(
    const base::TimeTicks& release_time) {
  test_backoff_entry_.SetCustomReleaseTime(release_time);
}

base::TimeDelta TestDataReductionProxyConfigServiceClient::GetDelay() const {
  return config_refresh_timer_.GetCurrentDelay();
}

base::TimeDelta
TestDataReductionProxyConfigServiceClient::GetBackoffTimeUntilRelease() const {
  return test_backoff_entry_.GetTimeUntilRelease();
}

int TestDataReductionProxyConfigServiceClient::GetBackoffErrorCount() {
  return test_backoff_entry_.failure_count();
}

void TestDataReductionProxyConfigServiceClient::SetConfigServiceURL(
    const GURL& service_url) {
  config_service_url_ = service_url;
}

int32_t
TestDataReductionProxyConfigServiceClient::failed_attempts_before_success()
    const {
  return failed_attempts_before_success_;
}

base::Time TestDataReductionProxyConfigServiceClient::Now() {
  return tick_clock_.Now();
}

net::BackoffEntry*
TestDataReductionProxyConfigServiceClient::GetBackoffEntry() {
  return &test_backoff_entry_;
}

TestDataReductionProxyConfigServiceClient::TestTickClock::TestTickClock(
    const base::Time& initial_time)
    : time_(initial_time) {}

base::TimeTicks
TestDataReductionProxyConfigServiceClient::TestTickClock::NowTicks() const {
  return base::TimeTicks::UnixEpoch() + (time_ - base::Time::UnixEpoch());
}

base::Time TestDataReductionProxyConfigServiceClient::TestTickClock::Now()
    const {
  return time_;
}

void TestDataReductionProxyConfigServiceClient::TestTickClock::SetTime(
    const base::Time& time) {
  time_ = time;
}

#if defined(OS_ANDROID)
bool TestDataReductionProxyConfigServiceClient::IsApplicationStateBackground()
    const {
  return is_application_state_background_;
}

void TestDataReductionProxyConfigServiceClient::
    TriggerApplicationStatusToForeground() {
  OnApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
}
#endif  // OS_ANDROID

void TestDataReductionProxyConfigServiceClient::SetRemoteConfigApplied(
    bool remote_config_applied) {
  remote_config_applied_ = remote_config_applied;
}

bool TestDataReductionProxyConfigServiceClient::RemoteConfigApplied() const {
  if (!remote_config_applied_) {
    return DataReductionProxyConfigServiceClient::RemoteConfigApplied();
  }
  return remote_config_applied_.value();
}

MockDataReductionProxyService::MockDataReductionProxyService(
    DataReductionProxySettings* settings,
    network::TestNetworkQualityTracker* test_network_quality_tracker,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : DataReductionProxyService(
          settings,
          prefs,
          std::move(url_loader_factory),
          std::make_unique<TestDataStore>(),
          test_network_quality_tracker,
          network::TestNetworkConnectionTracker::GetInstance(),
          nullptr,
          task_runner,
          base::TimeDelta(),
          Client::UNKNOWN,
          std::string(),
          std::string()) {}

MockDataReductionProxyService::~MockDataReductionProxyService() {}

TestDataReductionProxyService::TestDataReductionProxyService(
    DataReductionProxySettings* settings,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkQualityTracker* network_quality_tracker,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner)
    : DataReductionProxyService(
          settings,
          prefs,
          url_loader_factory,
          std::make_unique<TestDataStore>(),
          network_quality_tracker,
          network::TestNetworkConnectionTracker::GetInstance(),
          nullptr,
          db_task_runner,
          base::TimeDelta(),
          Client::UNKNOWN,
          std::string(),
          std::string()) {}

TestDataReductionProxyService::~TestDataReductionProxyService() {}

void TestDataReductionProxyService::SetIgnoreLongTermBlackListRules(
    bool ignore_long_term_black_list_rules) {
  ignore_blacklist_ = ignore_long_term_black_list_rules;
}

TestDataStore::TestDataStore() {}

TestDataStore::~TestDataStore() {}

DataStore::Status TestDataStore::Get(base::StringPiece key,
                                     std::string* value) {
  auto value_iter = map_.find(key.as_string());
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
  map_.erase(key.as_string());

  return OK;
}

DataStore::Status TestDataStore::RecreateDB() {
  map_.clear();

  return OK;
}

DataReductionProxyTestContext::Builder::Builder()
    : client_(Client::UNKNOWN),
      use_mock_config_(false),
      use_mock_service_(false),
      use_mock_request_options_(false),
      use_config_client_(false),
      use_test_config_client_(false),
      skip_settings_initialization_(false) {}

DataReductionProxyTestContext::Builder::~Builder() {}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithClient(Client client) {
  client_ = client;
  return *this;
}

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
DataReductionProxyTestContext::Builder::WithMockRequestOptions() {
  use_mock_request_options_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithConfigClient() {
  use_config_client_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithTestConfigClient() {
  use_config_client_ = true;
  use_test_config_client_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::SkipSettingsInitialization() {
  skip_settings_initialization_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithProxiesForHttp(
    const std::vector<DataReductionProxyServer>& proxy_servers) {
  DCHECK(!proxy_servers.empty());
  proxy_servers_ = proxy_servers;
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
  // Check for invalid builder combinations.
  DCHECK(!(use_mock_config_ && use_config_client_));

  unsigned int test_context_flags = 0;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  std::unique_ptr<TestingPrefServiceSimple> pref_service(
      new TestingPrefServiceSimple());
  auto* test_network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  std::unique_ptr<TestConfigStorer> config_storer(
      new TestConfigStorer(pref_service.get()));

  // In case no |url_loader_factory_| is specified, an instance will be
  // created in DataReductionProxyTestContext's ctor.
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory;
  if (url_loader_factory_) {
    url_loader_factory = url_loader_factory_;
  } else {
    test_url_loader_factory = std::make_unique<network::TestURLLoaderFactory>();
    url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory.get());
  }

  std::unique_ptr<DataReductionProxyConfigurator> configurator(
      new DataReductionProxyConfigurator());

  std::unique_ptr<TestDataReductionProxyConfig> config;
  std::unique_ptr<DataReductionProxyConfigServiceClient> config_client;
  DataReductionProxyMutableConfigValues* raw_mutable_config = nullptr;
  std::unique_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams());
  TestDataReductionProxyParams* raw_params = params.get();
  if (use_config_client_) {
    test_context_flags |= USE_CONFIG_CLIENT;
    std::unique_ptr<DataReductionProxyMutableConfigValues> mutable_config =
        std::make_unique<DataReductionProxyMutableConfigValues>();
    if (!proxy_servers_.empty()) {
      mutable_config->UpdateValues(proxy_servers_);
    }
    raw_mutable_config = mutable_config.get();
    config.reset(new TestDataReductionProxyConfig(std::move(mutable_config),
                                                  configurator.get()));
  } else if (use_mock_config_) {
    test_context_flags |= USE_MOCK_CONFIG;
    config.reset(new MockDataReductionProxyConfig(std::move(params),
                                                  configurator.get()));
  } else {
    test_context_flags ^= USE_MOCK_CONFIG;
    if (!proxy_servers_.empty()) {
      params->SetProxiesForHttp(proxy_servers_);
    }
    config.reset(new TestDataReductionProxyConfig(std::move(params),
                                                  configurator.get()));
  }

  std::unique_ptr<TestDataReductionProxyRequestOptions> request_options;

  if (use_mock_request_options_) {
    test_context_flags |= USE_MOCK_REQUEST_OPTIONS;
    request_options.reset(
        new MockDataReductionProxyRequestOptions(client_, config.get()));
  } else {
    request_options.reset(new TestDataReductionProxyRequestOptions(
        client_, "1.2.3.4", config.get()));
  }

  if (!settings_)
    settings_ = std::make_unique<DataReductionProxySettings>(false);
  if (skip_settings_initialization_) {
    test_context_flags |= SKIP_SETTINGS_INITIALIZATION;
  }

  pref_service->registry()->RegisterBooleanPref(prefs::kDataSaverEnabled,
                                                false);
  RegisterSimpleProfilePrefs(pref_service->registry());

  auto test_network_quality_tracker =
      std::make_unique<network::TestNetworkQualityTracker>();
  std::unique_ptr<DataReductionProxyService> service;
  if (use_mock_service_) {
    test_context_flags |= USE_MOCK_SERVICE;
    service = std::make_unique<MockDataReductionProxyService>(
        settings_.get(), test_network_quality_tracker.get(), pref_service.get(),
        url_loader_factory, task_runner);
  } else {
    service = std::make_unique<TestDataReductionProxyService>(
        settings_.get(), pref_service.get(), url_loader_factory,
        test_network_quality_tracker.get(), task_runner);
  }

  if (use_test_config_client_) {
    test_context_flags |= USE_TEST_CONFIG_CLIENT;
    config_client.reset(new TestDataReductionProxyConfigServiceClient(
        std::move(params), request_options.get(), raw_mutable_config,
        config.get(), service.get(), test_network_connection_tracker,
        base::BindRepeating(&TestConfigStorer::StoreSerializedConfig,
                            base::Unretained(config_storer.get())),
        GetTestBackoffPolicy()));
  } else if (use_config_client_) {
    config_client.reset(new DataReductionProxyConfigServiceClient(
        GetBackoffPolicy(), request_options.get(), raw_mutable_config,
        config.get(), service.get(), test_network_connection_tracker,
        base::Bind(&TestConfigStorer::StoreSerializedConfig,
                   base::Unretained(config_storer.get()))));
  }

  service->SetDependenciesForTesting(
      std::move(config), std::move(request_options), std::move(configurator),
      std::move(config_client));

  std::unique_ptr<DataReductionProxyTestContext> test_context(
      new DataReductionProxyTestContext(
          task_runner, std::move(pref_service), url_loader_factory,
          std::move(test_url_loader_factory), std::move(settings_),
          std::move(service), std::move(test_network_quality_tracker),
          std::move(config_storer), raw_params, test_context_flags));

  if (!skip_settings_initialization_)
    test_context->InitSettingsWithoutCheck();

  return test_context;
}

DataReductionProxyTestContext::DataReductionProxyTestContext(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<TestingPrefServiceSimple> simple_pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory,
    std::unique_ptr<DataReductionProxySettings> settings,
    std::unique_ptr<DataReductionProxyService> service,
    std::unique_ptr<network::TestNetworkQualityTracker>
        test_network_quality_tracker,
    std::unique_ptr<TestConfigStorer> config_storer,
    TestDataReductionProxyParams* params,
    unsigned int test_context_flags)
    : test_context_flags_(test_context_flags),
      task_runner_(task_runner),
      simple_pref_service_(std::move(simple_pref_service)),
      test_shared_url_loader_factory_(url_loader_factory),
      test_url_loader_factory_(std::move(test_url_loader_factory)),
      settings_(std::move(settings)),
      test_network_quality_tracker_(std::move(test_network_quality_tracker)),
      service_(std::move(service)),
      config_storer_(std::move(config_storer)),
      params_(params) {
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

  // This method should be called in case DataReductionProxyTestContext is
  // constructed without a valid SharedURLLoaderFactory instance.
  DCHECK(test_url_loader_factory_);

  // Enable the Data Reduction Proxy, simulating a successful secure proxy
  // check.
  test_url_loader_factory_->AddResponse(params::GetSecureProxyCheckURL().spec(),
                                        "OK");

  // Set the pref to cause the secure proxy check to be issued.
  SetDataReductionProxyEnabled(true);
  RunUntilIdle();
}

void DataReductionProxyTestContext::DisableWarmupURLFetch() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch);
}

void DataReductionProxyTestContext::DisableWarmupURLFetchCallback() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetchCallback);
}

MockDataReductionProxyConfig* DataReductionProxyTestContext::mock_config()
    const {
  DCHECK(test_context_flags_ & USE_MOCK_CONFIG);
  return reinterpret_cast<MockDataReductionProxyConfig*>(
      data_reduction_proxy_service()->config());
}

DataReductionProxyService*
DataReductionProxyTestContext::data_reduction_proxy_service() const {
  return data_reduction_proxy_service_;
}

TestDataReductionProxyService*
DataReductionProxyTestContext::test_data_reduction_proxy_service() const {
  DCHECK(!(test_context_flags_ & USE_MOCK_SERVICE));
  return reinterpret_cast<TestDataReductionProxyService*>(
      data_reduction_proxy_service());
}

MockDataReductionProxyService*
DataReductionProxyTestContext::mock_data_reduction_proxy_service() const {
  DCHECK(!(test_context_flags_ & SKIP_SETTINGS_INITIALIZATION));
  DCHECK(test_context_flags_ & USE_MOCK_SERVICE);
  return reinterpret_cast<MockDataReductionProxyService*>(
      data_reduction_proxy_service());
}

MockDataReductionProxyRequestOptions*
DataReductionProxyTestContext::mock_request_options() const {
  DCHECK(test_context_flags_ & USE_MOCK_REQUEST_OPTIONS);
  return reinterpret_cast<MockDataReductionProxyRequestOptions*>(
      data_reduction_proxy_service()->request_options());
}

TestDataReductionProxyConfig* DataReductionProxyTestContext::config() const {
  return reinterpret_cast<TestDataReductionProxyConfig*>(
      data_reduction_proxy_service()->config());
}

DataReductionProxyMutableConfigValues*
DataReductionProxyTestContext::mutable_config_values() {
  DCHECK(test_context_flags_ & USE_CONFIG_CLIENT);
  return reinterpret_cast<DataReductionProxyMutableConfigValues*>(
      config()->config_values());
}

TestDataReductionProxyConfigServiceClient*
DataReductionProxyTestContext::test_config_client() {
  DCHECK(test_context_flags_ & USE_TEST_CONFIG_CLIENT);
  return reinterpret_cast<TestDataReductionProxyConfigServiceClient*>(
      data_reduction_proxy_service()->config_client());
}

DataReductionProxyTestContext::TestConfigStorer::TestConfigStorer(
    PrefService* prefs)
    : prefs_(prefs) {
  DCHECK(prefs);
}

void DataReductionProxyTestContext::TestConfigStorer::StoreSerializedConfig(
    const std::string& serialized_config) {
  prefs_->SetString(prefs::kDataReductionProxyConfig, serialized_config);
  prefs_->SetInt64(prefs::kDataReductionProxyLastConfigRetrievalTime,
                   (base::Time::Now() - base::Time()).InMicroseconds());
}

std::vector<net::ProxyServer>
DataReductionProxyTestContext::GetConfiguredProxiesForHttp() const {
  const GURL kHttpUrl("http://test_http_url.net");
  // The test URL shouldn't match any of the bypass rules in the proxy rules.
  DCHECK(!configurator()->GetProxyConfig().proxy_rules().bypass_rules.Matches(
      kHttpUrl));

  net::ProxyInfo proxy_info;
  configurator()->GetProxyConfig().proxy_rules().Apply(kHttpUrl, &proxy_info);

  std::vector<net::ProxyServer> proxies_without_direct;
  for (const net::ProxyServer& proxy : proxy_info.proxy_list().GetAll())
    if (proxy.is_valid() && !proxy.is_direct())
      proxies_without_direct.push_back(proxy);
  return proxies_without_direct;
}

}  // namespace data_reduction_proxy
