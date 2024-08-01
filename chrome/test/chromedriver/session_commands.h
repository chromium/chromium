// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_COMMANDS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/test/chromedriver/command.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/session_connection_map.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

struct Capabilities;
class DeviceManager;
struct Session;
class Status;

struct InitSessionParams {
  InitSessionParams(
      network::mojom::URLLoaderFactory* factory,
      const SyncWebSocketFactory& socket_factory,
      DeviceManager* device_manager,
      const scoped_refptr<base::SingleThreadTaskRunner> cmd_task_runner,
      SessionConnectionMap* session_map);
  InitSessionParams(const InitSessionParams& other);
  ~InitSessionParams();

  raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory;
  SyncWebSocketFactory socket_factory;
  raw_ptr<DeviceManager> device_manager;
  scoped_refptr<base::SingleThreadTaskRunner> cmd_task_runner;
  raw_ptr<SessionConnectionMap> session_map;
};

bool GetW3CSetting(const base::Value::Dict& params);

bool MergeCapabilities(const base::Value::Dict& always_match,
                       const base::Value::Dict& first_match,
                       base::Value::Dict& merged);

bool MatchCapabilities(const base::Value::Dict& capabilities);

Status ProcessCapabilities(const base::Value::Dict& params,
                           base::Value::Dict& result_capabilities);

// Initializes a session.
Status ExecuteInitSession(const InitSessionParams& bound_params,
                          Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value);

// Quits a session.
Status ExecuteQuit(bool allow_detach,
                   Session* session,
                   const base::Value::Dict& params,
                   std::unique_ptr<base::Value>* value);

// Quits a session.
Status ExecuteBidiSessionEnd(Session* session,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value);

// Gets the capabilities of a particular session.
Status ExecuteGetSessionCapabilities(Session* session,
                                     const base::Value::Dict& params,
                                     std::unique_ptr<base::Value>* value);

// Retrieve the handle of the target window.
Status ExecuteGetCurrentWindowHandle(Session* session,
                                     const base::Value::Dict& params,
                                     std::unique_ptr<base::Value>* value);

// Close the target window.
Status ExecuteClose(Session* session,
                    const base::Value::Dict& params,
                    std::unique_ptr<base::Value>* value);

// Retrieve the list of all window handles available to the session.
Status ExecuteGetWindowHandles(Session* session,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value);

// Change target window to another. The window to target at may be specified by
// its server assigned window handle, or by the value of its name attribute.
Status ExecuteSwitchToWindow(Session* session,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value);

// Configure the amount of time that a particular type of operation can execute
// for before they are aborted and a timeout error is returned to the client.
Status ExecuteSetTimeouts(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value);

// Get the implicit, script and page load timeouts in milliseconds.
Status ExecuteGetTimeouts(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value);

// Set the timeout for asynchronous scripts.
Status ExecuteSetScriptTimeout(Session* session,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value);

// Set the amount of time the driver should wait when searching for elements.
Status ExecuteImplicitlyWait(Session* session,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value);

Status ExecuteIsLoading(Session* session,
                        const base::Value::Dict& params,
                        std::unique_ptr<base::Value>* value);

Status ExecuteCreateVirtualSensor(Session* session,
                                  const base::Value::Dict& params,
                                  std::unique_ptr<base::Value>* value);
Status ExecuteUpdateVirtualSensor(Session* session,
                                  const base::Value::Dict& params,
                                  std::unique_ptr<base::Value>* value);
Status ExecuteRemoveVirtualSensor(Session* session,
                                  const base::Value::Dict& params,
                                  std::unique_ptr<base::Value>* value);
Status ExecuteGetVirtualSensorInformation(Session* session,
                                          const base::Value::Dict& params,
                                          std::unique_ptr<base::Value>* value);

Status ExecuteGetLocation(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value);

Status ExecuteGetNetworkConnection(Session* session,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value);

Status ExecuteGetNetworkConditions(Session* session,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value);

Status ExecuteSetNetworkConnection(Session* session,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value);

Status ExecuteGetWindowPosition(Session* session,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value);

Status ExecuteSetWindowPosition(Session* session,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value);

Status ExecuteGetWindowSize(Session* session,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value);

Status ExecuteSetWindowSize(Session* session,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value);

Status ExecuteGetAvailableLogTypes(Session* session,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value);

Status ExecuteGetLog(Session* session,
                     const base::Value::Dict& params,
                     std::unique_ptr<base::Value>* value);

Status ExecuteUploadFile(Session* session,
                         const base::Value::Dict& params,
                         std::unique_ptr<base::Value>* value);

Status ExecuteSetSPCTransactionMode(Session* session,
                                    const base::Value::Dict& params,
                                    std::unique_ptr<base::Value>* value);

Status ExecuteGenerateTestReport(Session* session,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value);

Status ExecuteSetTimeZone(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value);

Status ExecuteCreateVirtualPressureSource(Session* session,
                                          const base::Value::Dict& params,
                                          std::unique_ptr<base::Value>* value);

Status ExecuteUpdateVirtualPressureSource(Session* session,
                                          const base::Value::Dict& params,
                                          std::unique_ptr<base::Value>* value);

Status ExecuteRemoveVirtualPressureSource(Session* session,
                                          const base::Value::Dict& params,
                                          std::unique_ptr<base::Value>* value);

// Forwards a BiDi command to BiDiMapper
Status ForwardBidiCommand(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value);

namespace internal {
Status ConfigureHeadlessSession(Session* session,
                                const Capabilities& capabilities);

// On return `desired_caps` either points at `merged_caps`, or points to some
// field in `params`.
Status ConfigureSession(Session* session,
                        const base::Value::Dict& params,
                        const base::Value::Dict*& desired_caps,
                        base::Value::Dict& merged_caps,
                        Capabilities* capabilities);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_COMMANDS_H_
