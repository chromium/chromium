// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_PROXY_SYSTEM_PROXY_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_PROXY_SYSTEM_PROXY_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_service.pb.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}

namespace ash {

// SystemProxyClient is used to communicate with the org.chromium.SystemProxy
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SYSTEM_PROXY) SystemProxyClient {
 public:
  using SetAuthenticationDetailsCallback = base::OnceCallback<void(
      const system_proxy::SetAuthenticationDetailsResponse& response)>;
  using WorkerActiveCallback = base::RepeatingCallback<void(
      const system_proxy::WorkerActiveSignalDetails& details)>;
  using AuthenticationRequiredCallback = base::RepeatingCallback<void(
      const system_proxy::AuthenticationRequiredDetails& details)>;
  using ClearUserCredentialsCallback = base::OnceCallback<void(
      const system_proxy::ClearUserCredentialsResponse& response)>;
  using ShutDownProcessCallback =
      base::OnceCallback<void(const system_proxy::ShutDownResponse& response)>;

  // Interface with testing functionality. Accessed through GetTestInterface(),
  // only implemented in the fake implementation.
  class TestInterface {
   public:
    // Returns how many times |SetAuthenticationDetails| was called.
    virtual int GetSetAuthenticationDetailsCallCount() const = 0;
    // Returns how many times |ShutDownProcess| was called.
    virtual int GetShutDownCallCount() const = 0;
    // Returns how many times |ClearUserCredentials| was called.
    virtual int GetClearUserCredentialsCount() const = 0;
    // Returns the content of the last request sent to the System-proxy service
    // to set authentication details.
    virtual system_proxy::SetAuthenticationDetailsRequest
    GetLastAuthenticationDetailsRequest() const = 0;
    // Simulates the |AuthenticationRequired| signal by calling the callback set
    // by |SetAuthenticationRequiredSignalCallback|. The callback is called only
    // if |FakeSystemProxyClient| was set up to listen for signals by calling
    // |ConnectToWorkerSignals|.
    virtual void SendAuthenticationRequiredSignal(
        const system_proxy::AuthenticationRequiredDetails& details) = 0;
    // Simulates the |WorkerActiveSignal| signal by calling the callback set
    // by |SetWorkerActiveSignalCallback|.
    virtual void SendWorkerActiveSignal(
        const system_proxy::WorkerActiveSignalDetails& details) = 0;

   protected:
    virtual ~TestInterface() {}
  };

  SystemProxyClient(const SystemProxyClient&) = delete;
  SystemProxyClient& operator=(const SystemProxyClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static SystemProxyClient* Get();

  // SystemProxy daemon D-Bus method calls. See org.chromium.SystemProxy.xml and
  // system_proxy_service.proto in Chromium OS code for the documentation of the
  // methods and request/response messages.
  virtual void SetAuthenticationDetails(
      const system_proxy::SetAuthenticationDetailsRequest& request,
      SetAuthenticationDetailsCallback callback) = 0;

  virtual void ClearUserCredentials(
      const system_proxy::ClearUserCredentialsRequest& request,
      ClearUserCredentialsCallback callback) = 0;

  // When receiving a shut down call, System-proxy will schedule a shut down
  // task and reply. |callback| is called when the daemon or one of the
  // processes starts to shut down.
  virtual void ShutDownProcess(const system_proxy::ShutDownRequest& request,
                               ShutDownProcessCallback callback) = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

  // Sets the callback to be called when System-proxy emits the
  // |WorkerActiveSignal| signal.
  virtual void SetWorkerActiveSignalCallback(WorkerActiveCallback callback) = 0;

  // Sets the callback to be called when System-proxy emits the
  // |AuthenticationRequired| signal.
  virtual void SetAuthenticationRequiredSignalCallback(
      AuthenticationRequiredCallback callback) = 0;

  // Waits for the System-proxy d-bus service to be available and then connects
  // to the signals for which a callback has been set with
  // |SetWorkerActiveSignalCallback| and
  // |SetAuthenticationRequiredSignalCallback|.
  virtual void ConnectToWorkerSignals() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  SystemProxyClient();
  virtual ~SystemProxyClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_PROXY_SYSTEM_PROXY_CLIENT_H_
