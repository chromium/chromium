// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_FAKE_BROWSER_DM_TOKEN_STORAGE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_FAKE_BROWSER_DM_TOKEN_STORAGE_H_

#include "base/gtest_prod_util.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

namespace policy {

// A fake BrowserDMTokenStorage implementation for testing. Test cases can set
// CBCM related values on the underlying delegate instead of reading them from
// the operating system.
class FakeBrowserDMTokenStorage : public BrowserDMTokenStorage {
 public:
  FakeBrowserDMTokenStorage();
  FakeBrowserDMTokenStorage(const std::string& client_id,
                            const std::string& enrollment_token,
                            const std::string& dm_token,
                            bool enrollment_error_option);

  FakeBrowserDMTokenStorage(const FakeBrowserDMTokenStorage&) = delete;
  FakeBrowserDMTokenStorage& operator=(const FakeBrowserDMTokenStorage&) =
      delete;

  ~FakeBrowserDMTokenStorage() override;

  void SetClientId(const std::string& client_id);
  void SetEnrollmentToken(const std::string& enrollment_token);
  void SetDMToken(const std::string& dm_token);
  void SetEnrollmentErrorOption(bool option);
  // Determines if SaveDMToken will be successful or not.
  void EnableStorage(bool storage_enabled);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageTest, SetDelegate);

  // A fake BrowserDMTokenStorage::Delegate implementation for testing.
  class MockDelegate : public BrowserDMTokenStorage::Delegate {
   public:
    MockDelegate();
    MockDelegate(const std::string& client_id,
                 const std::string& enrollment_token,
                 const std::string& dm_token,
                 bool enrollment_error_option);

    MockDelegate(const MockDelegate&) = delete;
    MockDelegate& operator=(const MockDelegate&) = delete;

    ~MockDelegate() override;

    void SetClientId(const std::string& client_id);
    void SetEnrollmentToken(const std::string& enrollment_token);
    void SetDMToken(const std::string& dm_token);
    void SetEnrollmentErrorOption(bool option);
    void EnableStorage(bool storage_enabled);

    // policy::BrowserDMTokenStorage::Delegate
    std::string InitClientId() override;
    std::string InitEnrollmentToken() override;
    std::string InitDMToken() override;
    bool InitEnrollmentErrorOption() override;
    bool CanInitEnrollmentToken() const override;
    BrowserDMTokenStorage::StoreTask SaveDMTokenTask(
        const std::string& token,
        const std::string& client_id) override;
    BrowserDMTokenStorage::StoreTask DeleteDMTokenTask(
        const std::string& client_id) override;
    scoped_refptr<base::TaskRunner> SaveDMTokenTaskRunner() override;

   private:
    std::string client_id_;
    std::string enrollment_token_;
    std::string dm_token_;
    bool enrollment_error_option_ = true;

    bool storage_enabled_ = true;
  };
};

}  // namespace policy

#endif  // COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_FAKE_BROWSER_DM_TOKEN_STORAGE_H_
