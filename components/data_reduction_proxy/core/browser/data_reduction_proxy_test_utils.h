// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/backoff_entry.h"
#include "net/base/proxy_server.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;
class TestingPrefServiceSimple;

namespace net {
class MockClientSocketFactory;
class URLRequestContext;
class URLRequestContextStorage;
}

namespace network {
class SharedURLLoaderFactory;
class TestNetworkQualityTracker;
class TestURLLoaderFactory;
}

namespace data_reduction_proxy {

class ClientConfig;
class DataReductionProxyConfigurator;
class DataReductionProxyMutableConfigValues;
class DataReductionProxyRequestOptions;
class DataReductionProxyServer;
class DataReductionProxySettings;
class MockDataReductionProxyConfig;
class TestDataReductionProxyConfig;
class TestDataReductionProxyParams;

// Test version of |DataReductionProxyRequestOptions|.
class TestDataReductionProxyRequestOptions
    : public DataReductionProxyRequestOptions {
 public:
  TestDataReductionProxyRequestOptions(Client client,
                                       const std::string& version,
                                       DataReductionProxyConfig* config);

  // Overrides of DataReductionProxyRequestOptions.
  std::string GetDefaultKey() const override;
  base::Time Now() const override;
  void RandBytes(void* output, size_t length) const override;

  // Time after the unix epoch that Now() reports.
  void set_offset(const base::TimeDelta& now_offset);

  using DataReductionProxyRequestOptions::GetHeaderValueForTesting;

 private:
  base::TimeDelta now_offset_;
};

// Mock version of |DataReductionProxyRequestOptions|.
class MockDataReductionProxyRequestOptions
    : public TestDataReductionProxyRequestOptions {
 public:
  MockDataReductionProxyRequestOptions(Client client,
                                       DataReductionProxyConfig* config);

  ~MockDataReductionProxyRequestOptions() override;

  MOCK_CONST_METHOD1(PopulateConfigResponse, void(ClientConfig* config));
};

// Test version of |DataReductionProxyConfigServiceClient|, which permits
// finely controlling the backoff timer.
class TestDataReductionProxyConfigServiceClient
    : public DataReductionProxyConfigServiceClient {
 public:
  TestDataReductionProxyConfigServiceClient(
      std::unique_ptr<DataReductionProxyParams> params,
      DataReductionProxyRequestOptions* request_options,
      DataReductionProxyMutableConfigValues* config_values,
      DataReductionProxyConfig* config,
      DataReductionProxyIOData* io_data,
      network::NetworkConnectionTracker* network_connection_tracker,
      ConfigStorer config_storer);

  ~TestDataReductionProxyConfigServiceClient() override;

  using DataReductionProxyConfigServiceClient::OnConnectionChanged;

  void SetNow(const base::Time& time);

  void SetCustomReleaseTime(const base::TimeTicks& release_time);

  base::TimeDelta GetDelay() const;

  int GetBackoffErrorCount();

  void SetConfigServiceURL(const GURL& service_url);

  int32_t failed_attempts_before_success() const;

#if defined(OS_ANDROID)
  bool IsApplicationStateBackground() const override;

  void set_application_state_background(bool new_state) {
    is_application_state_background_ = new_state;
  }

  bool foreground_fetch_pending() const { return foreground_fetch_pending_; }

  // Triggers the callback for Chromium status change to foreground.
  void TriggerApplicationStatusToForeground();
#endif

  void SetRemoteConfigApplied(bool remote_config_applied);

  bool RemoteConfigApplied() const override;

 protected:
  // Overrides of DataReductionProxyConfigServiceClient
  base::Time Now() override;
  net::BackoffEntry* GetBackoffEntry() override;

 private:
  // A clock which returns a fixed value in both base::Time and base::TimeTicks.
  class TestTickClock : public base::Clock, public base::TickClock {
   public:
    TestTickClock(const base::Time& initial_time);

    // base::TickClock implementation.
    base::TimeTicks NowTicks() const override;

    // base::Clock implementation.
    base::Time Now() const override;

    // Sets the current time.
    void SetTime(const base::Time& time);

   private:
    base::Time time_;
  };

#if defined(OS_ANDROID)
  bool is_application_state_background_;
#endif

  TestTickClock tick_clock_;
  net::BackoffEntry test_backoff_entry_;

  base::Optional<bool> remote_config_applied_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyConfigServiceClient);
};

// Test version of |DataReductionProxyService|, which permits mocking of various
// methods.
class MockDataReductionProxyService : public DataReductionProxyService {
 public:
  MockDataReductionProxyService(
      DataReductionProxySettings* settings,
      network::TestNetworkQualityTracker* test_network_quality_tracker,
      PrefService* prefs,
      net::URLRequestContextGetter* request_context,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~MockDataReductionProxyService() override;

  MOCK_METHOD2(SetProxyPrefs, void(bool enabled, bool at_startup));
  MOCK_METHOD8(
      UpdateContentLengths,
      void(int64_t data_used,
           int64_t original_size,
           bool data_reduction_proxy_enabled,
           data_reduction_proxy::DataReductionProxyRequestType request_type,
           const std::string& mime_type,
           bool is_user_traffic,
           data_use_measurement::DataUseUserData::DataUseContentType
               content_type,
           int32_t service_hash_code));
  MOCK_METHOD3(UpdateDataUseForHost,
               void(int64_t network_bytes,
                    int64_t original_bytes,
                    const std::string& host));
};

// Test version of |DataReductionProxyIOData|, which bypasses initialization in
// the constructor in favor of explicitly passing in its owned classes. This
// permits the use of test/mock versions of those classes.
class TestDataReductionProxyIOData : public DataReductionProxyIOData {
 public:
  TestDataReductionProxyIOData(
      PrefService* prefs,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<DataReductionProxyConfig> config,
      std::unique_ptr<TestDataReductionProxyRequestOptions> request_options,
      std::unique_ptr<DataReductionProxyConfigurator> configurator,
      network::NetworkConnectionTracker* network_connection_tracker,
      bool enabled);
  ~TestDataReductionProxyIOData() override;

  void SetDataReductionProxyService(base::WeakPtr<DataReductionProxyService>
                                        data_reduction_proxy_service) override;

  DataReductionProxyConfigurator* configurator() const {
    return configurator_.get();
  }

  void set_config_client(
      std::unique_ptr<DataReductionProxyConfigServiceClient> config_client) {
    config_client_ = std::move(config_client);
  }
  DataReductionProxyConfigServiceClient* config_client() const {
    return config_client_.get();
  }

  TestDataReductionProxyRequestOptions* test_request_options() const {
    return test_request_options_;
  }

  void set_proxy_delegate(
      std::unique_ptr<DataReductionProxyDelegate> proxy_delegate) {
    proxy_delegate_ = std::move(proxy_delegate);
  }

  base::WeakPtr<DataReductionProxyIOData> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Records the reporting fraction that was set by parsing a config.
  void SetPingbackReportingFraction(float pingback_reporting_fraction) override;

  // Records |ignore_long_term_black_list_rules| as |ignore_blacklist_|.
  void SetIgnoreLongTermBlackListRules(
      bool ignore_long_term_black_list_rules) override;

  float pingback_reporting_fraction() const {
    return pingback_reporting_fraction_;
  }

  bool ignore_blacklist() const { return ignore_blacklist_; }

 private:
  // Allowed SetDataReductionProxyService to be re-entrant.
  bool service_set_;

  // Reporting fraction last set via SetPingbackReportingFraction.
  float pingback_reporting_fraction_;

  // Whether the long term blacklist rules should be ignored.
  bool ignore_blacklist_ = false;

  TestDataReductionProxyRequestOptions* test_request_options_;
};

// Test version of |DataStore|. Uses an in memory hash map to store data.
class TestDataStore : public data_reduction_proxy::DataStore {
 public:
  TestDataStore();

  ~TestDataStore() override;

  void InitializeOnDBThread() override {}

  DataStore::Status Get(base::StringPiece key, std::string* value) override;

  DataStore::Status Put(const std::map<std::string, std::string>& map) override;

  DataStore::Status Delete(base::StringPiece key) override;

  DataStore::Status RecreateDB() override;

  std::map<std::string, std::string>* map() { return &map_; }

 private:
  std::map<std::string, std::string> map_;
};

// Builds a test version of the Data Reduction Proxy stack for use in tests.
// Takes in various |TestContextOptions| which controls the behavior of the
// underlying objects.
class DataReductionProxyTestContext {
 public:
  // Allows for a fluent builder interface to configure what kind of objects
  // (test vs mock vs real) are used by the |DataReductionProxyTestContext|.
  class Builder {
   public:
    Builder();

    ~Builder();

    // The |Client| enum to use for |DataReductionProxyRequestOptions|.
    Builder& WithClient(Client client);

    // Specifies a |net::URLRequestContext| to use. The |request_context| is
    // owned by the caller.
    Builder& WithURLRequestContext(net::URLRequestContext* request_context);

    // Specifies a |network::URLLoaderFactory| to use.
    Builder& WithURLLoaderFactory(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

    // Specifies a |net::MockClientSocketFactory| to use. The
    // |mock_socket_factory| is owned by the caller. If a non-NULL
    // |request_context_| is also specified, then the caller is responsible for
    // attaching |mock_socket_factory| to |request_context_|. Otherwise,
    // |mock_socket_factory| will be attached to the dummy
    // |net::URLRequestContext| generated during Build().
    Builder& WithMockClientSocketFactory(
        net::MockClientSocketFactory* mock_socket_factory);

    // Specifies the use of |MockDataReductionProxyConfig| instead of
    // |TestDataReductionProxyConfig|.
    Builder& WithMockConfig();

    // Specifies the use of |MockDataReductionProxyService| instead of
    // |DataReductionProxyService|.
    Builder& WithMockDataReductionProxyService();

    // Specifies the use of |MockDataReductionProxyRequestOptions| instead of
    // |DataReductionProxyRequestOptions|.
    Builder& WithMockRequestOptions();

    // Specifies the use of the |DataReductionProxyConfigServiceClient|.
    Builder& WithConfigClient();

    // Specifies the use of the a |TestDataReductionProxyConfigServiceClient|
    // instead of a |DataReductionProxyConfigServiceClient|.
    Builder& WithTestConfigClient();

    // Construct, but do not initialize the |DataReductionProxySettings| object.
    Builder& SkipSettingsInitialization();

    // Specifies the data reduction proxy servers.
    Builder& WithProxiesForHttp(
        const std::vector<DataReductionProxyServer>& proxy_servers);

    // Specifies a settings object to use.
    Builder& WithSettings(std::unique_ptr<DataReductionProxySettings> settings);

    // Creates a |DataReductionProxyTestContext|. Owned by the caller.
    std::unique_ptr<DataReductionProxyTestContext> Build();

   private:
    Client client_;
    net::URLRequestContext* request_context_;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    net::MockClientSocketFactory* mock_socket_factory_;

    bool use_mock_config_;
    bool use_mock_service_;
    bool use_mock_request_options_;
    bool use_config_client_;
    bool use_test_config_client_;
    bool skip_settings_initialization_;
    std::vector<DataReductionProxyServer> proxy_servers_;
    std::unique_ptr<DataReductionProxySettings> settings_;
  };

  virtual ~DataReductionProxyTestContext();

  // Returns the name of the preference used to enable the Data Reduction
  // Proxy.
  const char* GetDataReductionProxyEnabledPrefName() const;

  // Registers, sets, and gets the preference used to enable the Data Reduction
  // Proxy, respectively.
  void RegisterDataReductionProxyEnabledPref();
  void SetDataReductionProxyEnabled(bool enabled);
  bool IsDataReductionProxyEnabled() const;

  // Waits while executing all tasks on the current SingleThreadTaskRunner.
  void RunUntilIdle();

  // Initializes the |DataReductionProxySettings| object. Can only be called if
  // built with SkipSettingsInitialization.
  void InitSettings();

  // Destroys the |DataReductionProxySettings| object and waits until objects on
  // the DB task runner are destroyed.
  void DestroySettings();

  // Creates a |DataReductionProxyService| object, or a
  // |MockDataReductionProxyService| if built with
  // WithMockDataReductionProxyService. Can only be called if built with
  // SkipSettingsInitialization.
  std::unique_ptr<DataReductionProxyService> CreateDataReductionProxyService(
      DataReductionProxySettings* settings);

  // This creates a |DataReductionProxyNetworkDelegate| and
  // |DataReductionProxyInterceptor|, using them in the |net::URLRequestContext|
  // for |request_context_storage|. |request_context_storage| takes ownership of
  // the created objects.
  void AttachToURLRequestContext(
      net::URLRequestContextStorage* request_context_storage) const;

  // Enable the Data Reduction Proxy, simulating a successful secure proxy
  // check. This can only be called if not built with WithTestConfigurator,
  // |settings_| has been initialized, and |this| was built with a
  // |net::MockClientSocketFactory| specified.
  void EnableDataReductionProxyWithSecureProxyCheckSuccess();

  // Disables the fetch of the warmup URL. Useful for testing to avoid setting
  // up the network mock sockets.
  void DisableWarmupURLFetch();

  // Disables the warmup URL fetcher to callback into DRP to report the result
  // of the warmup fetch. The callback can result in DRP proxies getting
  // disabled. This method is useful for testing.
  void DisableWarmupURLFetchCallback();

  // Returns the underlying |MockDataReductionProxyConfig|. This can only be
  // called if built with WithMockConfig.
  MockDataReductionProxyConfig* mock_config() const;

  DataReductionProxyService* data_reduction_proxy_service() const;

  // Returns the underlying |MockDataReductionProxyService|. This can only
  // be called if built with WithMockDataReductionProxyService.
  MockDataReductionProxyService* mock_data_reduction_proxy_service() const;

  // Returns the underlying |MockDataReductionProxyRequestOptions|. This can
  // only be called if built with WithMockRequestOptions.
  MockDataReductionProxyRequestOptions* mock_request_options() const;

  // Returns the underlying |TestDataReductionProxyConfig|.
  TestDataReductionProxyConfig* config() const;

  // Returns the underlying |DataReductionProxyMutableConfigValues|. This can
  // only be called if built with WithConfigClient.
  DataReductionProxyMutableConfigValues* mutable_config_values();

  // Returns the underlying |TestDataReductionProxyConfigServiceClient|. This
  // can only be called if built with WithTestConfigClient.
  TestDataReductionProxyConfigServiceClient* test_config_client();

  // Obtains a callback for notifying that the Data Reduction Proxy is no
  // longer reachable.
  DataReductionProxyBypassStats::UnreachableCallback
  unreachable_callback() const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  TestingPrefServiceSimple* pref_service() {
    return simple_pref_service_.get();
  }

  net::URLRequestContextGetter* request_context_getter() const {
    return request_context_getter_.get();
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() const {
    return test_shared_url_loader_factory_;
  }

  DataReductionProxyBypassStats* bypass_stats() const {
    return io_data_->bypass_stats();
  }

  DataReductionProxyConfigurator* configurator() const {
    return io_data_->configurator();
  }

  TestDataReductionProxyIOData* io_data() const {
    return io_data_.get();
  }

  DataReductionProxySettings* settings() const {
    return settings_.get();
  }

  TestDataReductionProxyParams* test_params() const {
    return params_;
  }

  network::TestNetworkQualityTracker* test_network_quality_tracker() const {
    return test_network_quality_tracker_.get();
  }

  void InitSettingsWithoutCheck();

  // Returns the proxies that are currently configured for "http://" requests,
  // excluding any that are invalid or direct.
  std::vector<net::ProxyServer> GetConfiguredProxiesForHttp() const;

 private:
  // Used to storage a serialized Data Reduction Proxy config.
  class TestConfigStorer {
   public:
    // |prefs| must not be null and outlive |this|.
    TestConfigStorer(PrefService* prefs);

    // Stores |serialized_config| in |prefs_|.
    void StoreSerializedConfig(const std::string& serialized_config);

   private:
    PrefService* prefs_;
  };

  DataReductionProxyTestContext(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<TestingPrefServiceSimple> simple_pref_service,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      net::MockClientSocketFactory* mock_socket_factory,
      std::unique_ptr<TestDataReductionProxyIOData> io_data,
      std::unique_ptr<DataReductionProxySettings> settings,
      std::unique_ptr<TestConfigStorer> config_storer,
      TestDataReductionProxyParams* params,
      unsigned int test_context_flags);

  std::unique_ptr<DataReductionProxyService>
  CreateDataReductionProxyServiceInternal(DataReductionProxySettings* settings);

  unsigned int test_context_flags_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<TestingPrefServiceSimple> simple_pref_service_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  // Non-owned pointer. Will be NULL if |this| was built without specifying a
  // |net::MockClientSocketFactory|.
  net::MockClientSocketFactory* mock_socket_factory_;

  std::unique_ptr<TestDataReductionProxyIOData> io_data_;
  std::unique_ptr<DataReductionProxySettings> settings_;
  std::unique_ptr<TestConfigStorer> config_storer_;
  std::unique_ptr<network::TestNetworkQualityTracker>
      test_network_quality_tracker_;

  TestDataReductionProxyParams* params_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyTestContext);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
