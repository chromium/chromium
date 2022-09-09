// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
#define CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_

#include <guiddef.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <ios>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/win_util.h"
#include "base/win/windows_types.h"

class WorkItemList;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace updater {

enum class UpdaterScope;

bool RegisterWakeTask(const base::CommandLine& run_command, UpdaterScope scope);
void UnregisterWakeTask(UpdaterScope scope);

std::wstring GetComServerClsidRegistryPath(REFCLSID clsid);
std::wstring GetComServerAppidRegistryPath(REFGUID appid);
std::wstring GetComIidRegistryPath(REFIID iid);
std::wstring GetComTypeLibRegistryPath(REFIID iid);

// Returns the resource index for the type library where the interface specified
// by the `iid` is defined. For encapsulation reasons, the updater interfaces
// are segregated in multiple IDL files, which get compiled to multiple type
// libraries. The type libraries are inserted in the compiled binary as
// resources with different resource indexes. The resource index becomes a
// suffix of the path to where the type library exists, such as
// `...\updater.exe\\1`. See the Windows SDK documentation for LoadTypeLib for
// details.
std::wstring GetComTypeLibResourceIndex(REFIID iid);

// Returns the interfaces ids of all interfaces declared in IDL of the updater
// that can be installed side-by-side with other instances of the updater.
std::vector<IID> GetSideBySideInterfaces();

// Returns the interfaces ids of all interfaces declared in IDL of the updater
// that can only be installed for the active instance of the updater.
std::vector<IID> GetActiveInterfaces();

// Returns the interfaces ids of all interfaces declared in IDL of the updater
// that can be installed side-by-side (if `is_internal` is `true`) or for the
// active instance (if `is_internal` is `false`) .
std::vector<IID> GetInterfaces(bool is_internal);

// Returns the CLSIDs of servers that can be installed side-by-side with other
// instances of the updater.
std::vector<CLSID> GetSideBySideServers(UpdaterScope scope);

// Returns the CLSIDs of servers that can only be installed for the active
// instance of the updater.
std::vector<CLSID> GetActiveServers(UpdaterScope scope);

// Returns the CLSIDs of servers that can be installed side-by-side (if
// `is_internal` is `true`) or for the active instance (if `is_internal` is
// `false`) .
std::vector<CLSID> GetServers(bool is_internal, UpdaterScope scope);

// Helper function that joins two vectors and returns the resultant vector.
template <typename T>
std::vector<T> JoinVectors(const std::vector<T>& vector1,
                           const std::vector<T>& vector2) {
  std::vector<T> joined_vector = vector1;
  joined_vector.insert(joined_vector.end(), vector2.begin(), vector2.end());
  return joined_vector;
}

// Adds work items to `list` to install the interface `iid`.
void AddInstallComInterfaceWorkItems(HKEY root,
                                     const base::FilePath& typelib_path,
                                     GUID iid,
                                     WorkItemList* list);

// Adds work items to `list` to install the server `iid`.
void AddInstallServerWorkItems(HKEY root,
                               CLSID iid,
                               const base::FilePath& executable_path,
                               bool internal_service,
                               WorkItemList* list);

// Adds work items to register the per-user COM server.
void AddComServerWorkItems(const base::FilePath& com_server_path,
                           bool is_internal,
                           WorkItemList* list);

// Adds work items to register the COM service.
void AddComServiceWorkItems(const base::FilePath& com_service_path,
                            bool internal_service,
                            WorkItemList* list);

// Adds a worklist item to set a value in the Run key in the user registry under
// the value `run_value_name` to start the specified `command`.
void RegisterUserRunAtStartup(const std::wstring& run_value_name,
                              const base::CommandLine& command,
                              WorkItemList* list);

// Deletes the value in the Run key in the user registry under the value
// `run_value_name`.
bool UnregisterUserRunAtStartup(const std::wstring& run_value_name);

// Loads the typelib and typeinfo for all interfaces from updater.exe. Logs on
// failure.
// If the typelib loads successfully, logs the registry entries for the typelib.
// TODO(crbug.com/1341471) - revert the CL that introduced the check after the
// bug is resolved.
void CheckComInterfaceTypeLib(UpdaterScope scope, bool is_internal);

// Marshals interface T implemented by an instance of V and unmarshals it into
// another thread. The test also checks for successful creation of proxy/stubs
// for the interface.
// TODO(crbug.com/1341471) - revert the CL that introduced the check after the
// bug is resolved.
template <typename T, typename V>
void MarshalInterface() {
  constexpr REFIID iid = __uuidof(T);

  // Create proxy/stubs for the interface.
  // Look up the ProxyStubClsid32.
  CLSID psclsid = {};
  HRESULT hr = ::CoGetPSClsid(iid, &psclsid);

  CHECK(SUCCEEDED(hr)) << std::hex << hr;
  CHECK_EQ(base::ToUpperASCII(
               base::WideToASCII(base::win::WStringFromGUID(psclsid))),
           "{00020424-0000-0000-C000-000000000046}");

  // Get the proxy/stub factory buffer.
  Microsoft::WRL::ComPtr<IPSFactoryBuffer> psfb;
  hr = ::CoGetClassObject(psclsid, CLSCTX_INPROC, 0, IID_PPV_ARGS(&psfb));

  CHECK(SUCCEEDED(hr)) << std::hex << hr;

  // Create the interface proxy.
  Microsoft::WRL::ComPtr<IRpcProxyBuffer> proxy_buffer;
  Microsoft::WRL::ComPtr<T> object;
  hr = psfb->CreateProxy(nullptr, iid, &proxy_buffer,
                         IID_PPV_ARGS_Helper(&object));
  LOG_IF(ERROR, FAILED(hr))
      << __func__ << ": CreateProxy failed: " << std::hex << hr;

  // Create the interface stub.
  Microsoft::WRL::ComPtr<IRpcStubBuffer> stub_buffer;
  hr = psfb->CreateStub(iid, nullptr, &stub_buffer);
  LOG_IF(ERROR, FAILED(hr))
      << __func__ << ": CreateStub failed: " << std::hex << hr;

  // Marshal and unmarshal a T interface implemented by V.
  object.Reset();
  hr = Microsoft::WRL::MakeAndInitialize<V>(&object);
  CHECK(SUCCEEDED(hr)) << std::hex << hr;

  Microsoft::WRL::ComPtr<IStream> stream;
  hr = ::CoMarshalInterThreadInterfaceInStream(iid, object.Get(), &stream);
  CHECK(SUCCEEDED(hr)) << std::hex << hr;

  base::ScopedAllowBaseSyncPrimitivesForTesting blocking_allowed_here;
  base::WaitableEvent unmarshal_complete_event;

  base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](Microsoft::WRL::ComPtr<IStream> stream,
                 base::WaitableEvent& event) {
                const base::ScopedClosureRunner signal_event(base::BindOnce(
                    [](base::WaitableEvent& event) { event.Signal(); },
                    std::ref(event)));

                Microsoft::WRL::ComPtr<T> object;
                HRESULT hr =
                    ::CoUnmarshalInterface(stream.Get(), IID_PPV_ARGS(&object));
                CHECK(SUCCEEDED(hr)) << std::hex << hr;
              },
              stream, std::ref(unmarshal_complete_event)));

  if (!unmarshal_complete_event.TimedWait(base::Seconds(60))) {
    NOTREACHED();
  }
}

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
