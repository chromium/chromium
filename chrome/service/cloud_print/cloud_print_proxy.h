// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"
#include "chrome/service/cloud_print/cloud_print_wipeout.h"

class ServiceProcessPrefs;

namespace network {
class NetworkConnectionTracker;
}

namespace cloud_print {

struct CloudPrintProxyInfo;

// CloudPrintProxy is the layer between the service process UI thread
// and the cloud print proxy backend.
class CloudPrintProxy : public CloudPrintProxyFrontend,
                        public CloudPrintWipeout::Client {
 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void OnCloudPrintProxyEnabled(bool persist_state) {}
    virtual void OnCloudPrintProxyDisabled(bool persist_state) {}
  };
  CloudPrintProxy();
  ~CloudPrintProxy() override;

  // Provides a CloudPrintProxy instance, which may be lazily instantiated.
  class Provider {
   public:
    virtual CloudPrintProxy* GetCloudPrintProxy() = 0;
  };

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize(
      ServiceProcessPrefs* service_prefs,
      Client* client,
      network::NetworkConnectionTracker* network_connection_tracker);

  // Enables/disables cloud printing for the user
  void EnableForUser();
  void EnableForUserWithRobot(const std::string& robot_auth_code,
                              const std::string& robot_email,
                              const std::string& user_email,
                              base::Value user_settings);
  void UnregisterPrintersAndDisableForUser();
  void DisableForUser();
  // Returns the proxy info.
  void GetProxyInfo(CloudPrintProxyInfo* info);
  // Return accessible printers.
  void GetPrinters(std::vector<std::string>* printers);

  const std::string& user_email() const {
    return user_email_;
  }

  // CloudPrintProxyFrontend implementation. Called on UI thread.
  void OnAuthenticated(const std::string& robot_oauth_refresh_token,
                       const std::string& robot_email,
                       const std::string& user_email) override;
  void OnAuthenticationFailed() override;
  void OnPrintSystemUnavailable() override;
  void OnUnregisterPrinters(const std::string& auth_token,
                            const std::list<std::string>& printer_ids) override;
  void OnXmppPingUpdated(int ping_timeout) override;

  // CloudPrintWipeout::Client implementation.
  void OnUnregisterPrintersComplete() override;

 protected:
  void ShutdownBackend();
  bool CreateBackend();

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  std::unique_ptr<CloudPrintProxyBackend> backend_;
  // This class does not own this. It is guaranteed to remain valid for the
  // lifetime of this class.
  ServiceProcessPrefs* service_prefs_ = nullptr;
  // This class does not own this. If non-NULL, It is guaranteed to remain
  // valid for the lifetime of this class.
  Client* client_ = nullptr;
  // Used to listen for network connection changes.
  network::NetworkConnectionTracker* network_connection_tracker_ = nullptr;
  // The email address of the account used to authenticate to the Cloud Print
  // service.
  std::string user_email_;
  // This is set to true when the Cloud Print proxy is enabled and after
  // successful authentication with the Cloud Print service.
  bool enabled_ = false;
  // This is a cleanup class for unregistering printers on proxy disable.
  std::unique_ptr<CloudPrintWipeout> wipeout_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxy);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_H_
