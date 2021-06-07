// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/service/cloud_print/connector_settings.h"
#include "printing/backend/print_backend.h"

namespace gaia {
struct OAuthClientInfo;
}

namespace network {
class NetworkConnectionTracker;
}

namespace cloud_print {

// CloudPrintProxyFrontend is the interface used by CloudPrintProxyBackend to
// communicate with the entity that created it and, presumably, is interested in
// cloud print proxy related activity.
// NOTE: All methods will be invoked by a CloudPrintProxyBackend on the same
// thread used to create that CloudPrintProxyBackend.
class CloudPrintProxyFrontend {
 public:
  CloudPrintProxyFrontend() {}

  // We successfully authenticated with the cloud print server. This callback
  // allows the frontend to persist the tokens.
  virtual void OnAuthenticated(const std::string& robot_oauth_refresh_token,
                               const std::string& robot_email,
                               const std::string& user_email) = 0;
  // We have invalid/expired credentials.
  virtual void OnAuthenticationFailed() = 0;
  // The print system could not be initialized.
  virtual void OnPrintSystemUnavailable() = 0;
  // Receive auth token and list of printers.
  virtual void OnUnregisterPrinters(
      const std::string& auth_token,
      const std::list<std::string>& printer_ids) = 0;
  // Update and store service settings.
  virtual void OnXmppPingUpdated(int ping_timeout) = 0;

 protected:
  // Don't delete through CloudPrintProxyFrontend interface.
  virtual ~CloudPrintProxyFrontend() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyFrontend);
};

class CloudPrintProxyBackend {
 public:
  CloudPrintProxyBackend(
      CloudPrintProxyFrontend* frontend,
      const ConnectorSettings& settings,
      const gaia::OAuthClientInfo& oauth_client_info,
      bool enable_job_poll,
      network::NetworkConnectionTracker* network_connection_tracker);
  ~CloudPrintProxyBackend();

  // Legacy mechanism when we have saved user credentials but no saved robot
  // credentials.
  bool InitializeWithToken(const std::string& cloud_print_token);
  // Called when we have saved robot credentials.
  bool InitializeWithRobotToken(const std::string& robot_oauth_refresh_token,
                                const std::string& robot_email);
  // Called when an external entity passed in the auth code for the robot.
  bool InitializeWithRobotAuthCode(const std::string& robot_oauth_auth_code,
                                   const std::string& robot_email);
  void Shutdown();
  void RegisterPrinters(const printing::PrinterList& printer_list);
  void UnregisterPrinters();

 private:
  bool PostCoreTask(const base::Location& from_here, base::OnceClosure task);

  // The real guts of CloudPrintProxyBackend, to keep the public client API
  // clean.
  class Core;

  // A thread dedicated for use to perform initialization and authentication.
  base::Thread core_thread_;

  // The core, which communicates with AuthWatcher for GAIA authentication and
  // which contains printer registration code.
  scoped_refptr<Core> core_;

  // A reference to the TaskRunner used to construct |this|, so we know how to
  // safely talk back to the CloudPrintProxyFrontend.
  const scoped_refptr<base::SingleThreadTaskRunner> frontend_task_runner_;

  // The frontend which is responsible for displaying UI and updating Prefs.
  // Outlives this backend.
  CloudPrintProxyFrontend* const frontend_;

  friend class base::RefCountedThreadSafe<CloudPrintProxyBackend::Core>;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyBackend);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_
