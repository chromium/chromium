// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/backoff_entry.h"
#include "net/base/proxy_server.h"
#include "testing/gmock/include/gmock/gmock.h"

class TestingPrefServiceSimple;

namespace data_reduction_proxy {

class DataReductionProxySettings;


// Test version of |DataReductionProxyService|, which permits mocking of various
// methods.
class MockDataReductionProxyService : public DataReductionProxyService {
 public:
  MockDataReductionProxyService(
      DataReductionProxySettings* settings,
      PrefService* prefs,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~MockDataReductionProxyService() override;

  MOCK_METHOD2(SetProxyPrefs, void(bool enabled, bool at_startup));
};

// Test version of |DataReductionProxyService|, which bypasses initialization in
// the constructor in favor of explicitly passing in its owned classes. This
// permits the use of test/mock versions of those classes.
class TestDataReductionProxyService : public DataReductionProxyService {
 public:
  TestDataReductionProxyService(
      DataReductionProxySettings* settings,
      PrefService* prefs,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner);
  ~TestDataReductionProxyService() override;
};

class DataReductionProxyTestContext {
 public:
  // Allows for a fluent builder interface to configure what kind of objects
  // (test vs mock vs real) are used by the |DataReductionProxyTestContext|.
  class Builder {
   public:
    Builder();

    ~Builder();

    // Specifies the use of |MockDataReductionProxyConfig| instead of
    // |TestDataReductionProxyConfig|.
    Builder& WithMockConfig();

    // Specifies the use of |MockDataReductionProxyService| instead of
    // |DataReductionProxyService|.
    Builder& WithMockDataReductionProxyService();

    // Construct, but do not initialize the |DataReductionProxySettings| object.
    Builder& SkipSettingsInitialization();

    // Specifies a settings object to use.
    Builder& WithSettings(std::unique_ptr<DataReductionProxySettings> settings);

    // Creates a |DataReductionProxyTestContext|. Owned by the caller.
    std::unique_ptr<DataReductionProxyTestContext> Build();

   private:
    bool use_mock_config_;
    bool use_mock_service_;
    bool skip_settings_initialization_;
    std::unique_ptr<DataReductionProxySettings> settings_;
  };

  DataReductionProxyTestContext(const DataReductionProxyTestContext&) = delete;
  DataReductionProxyTestContext& operator=(
      const DataReductionProxyTestContext&) = delete;

  virtual ~DataReductionProxyTestContext();

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

  // Takes ownership of the |DataReductionProxyService| object. Can only be
  // called if built with SkipSettingsInitialization.
  std::unique_ptr<DataReductionProxyService> TakeService();

  // Enable the Data Reduction Proxy, simulating a successful secure proxy
  // check. This can only be called if not built with WithTestConfigurator,
  // |settings_| has been initialized, and |this| was built with a
  // |net::MockClientSocketFactory| specified.
  void EnableDataReductionProxyWithSecureProxyCheckSuccess();


  DataReductionProxyService* data_reduction_proxy_service() const;

  // Returns the underlying |TestDataReductionProxyService|. This can only be
  // called if not built with WithMockDataReductionProxyService.
  TestDataReductionProxyService* test_data_reduction_proxy_service() const;

  // Returns the underlying |MockDataReductionProxyService|. This can only
  // be called if built with WithMockDataReductionProxyService.
  MockDataReductionProxyService* mock_data_reduction_proxy_service() const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  TestingPrefServiceSimple* pref_service() {
    return simple_pref_service_.get();
  }

  DataReductionProxySettings* settings() const { return settings_.get(); }

  void InitSettingsWithoutCheck();


 private:
  DataReductionProxyTestContext(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<TestingPrefServiceSimple> simple_pref_service,
      std::unique_ptr<DataReductionProxySettings> settings,
      std::unique_ptr<DataReductionProxyService> service,
      unsigned int test_context_flags);

  unsigned int test_context_flags_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<TestingPrefServiceSimple> simple_pref_service_;

  std::unique_ptr<DataReductionProxySettings> settings_;
  raw_ptr<DataReductionProxyService> data_reduction_proxy_service_;
  std::unique_ptr<DataReductionProxyService> service_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
