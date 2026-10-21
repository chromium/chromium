// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_DIRECT_LAUNCH_FAKE_DIRECT_LAUNCH_SHELL_H_
#define CHROME_BROWSER_WIN_DIRECT_LAUNCH_FAKE_DIRECT_LAUNCH_SHELL_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/win/windows_types.h"

struct INamedPropertyStore;

namespace direct_launch {

// Stands in for the shell's direct launch API for as long as it exists, so that
// `DirectLaunchManager` registers with this fake instead of with the shell. It
// registers a class object for the API's CLSID with COM in this process, which
// COM then resolves the API to. This lets the manager's registration code run
// on any version of Windows, and keeps tests from ever reaching the shell.
//
// Must be created, used and destroyed on a UI thread where COM is initialized
// as an STA. The fake is not thread-safe, so the manager under test must also
// call into the shell on that thread, rather than on its default ThreadPool
// sequence.
class FakeDirectLaunchShell {
 public:
  // The arguments of an accepted URI launch registration.
  struct UriRegistration {
    UriRegistration();
    UriRegistration(const UriRegistration&);
    UriRegistration& operator=(const UriRegistration&);
    ~UriRegistration();

    HWND target_window = nullptr;
    std::vector<std::wstring> schemes;
    uint32_t copy_data_format_id = 0;
  };

  // The arguments of an accepted tile launch registration.
  struct TileRegistration {
    HWND target_window = nullptr;
    UINT message_id = 0;
  };

  // The methods of the API.
  static constexpr wchar_t kRegisterUriLaunch[] =
      L"RegisterAcceleratedUriLaunch";
  static constexpr wchar_t kUnregisterUriLaunch[] =
      L"UnregisterAcceleratedUriLaunch";
  static constexpr wchar_t kRegisterTileLaunch[] =
      L"RegisterAcceleratedTileLaunch";
  static constexpr wchar_t kUnregisterTileLaunch[] =
      L"UnregisterAcceleratedTileLaunch";

  FakeDirectLaunchShell();
  FakeDirectLaunchShell(const FakeDirectLaunchShell&) = delete;
  FakeDirectLaunchShell& operator=(const FakeDirectLaunchShell&) = delete;
  ~FakeDirectLaunchShell();

  // Makes creating the API fail with `result`, like on an OS without the API.
  void set_create_result(HRESULT result) { create_result_ = result; }

  // Makes `method` fail with `result`, like the shell declining a
  // registration, or work again when `result` is S_OK. A method that fails has
  // no effect.
  void SetInvokeResult(std::wstring_view method, HRESULT result);

  // Makes the API report `method` as unsupported, or as supported again. Like
  // the shell, the API invokes an unsupported method without effect, and
  // reports that through the method's `is_supported` out parameter only, while
  // still returning S_OK.
  void SetMethodSupported(std::wstring_view method, bool is_supported);

  // The number of times the API was created, including the failed attempts.
  int create_count() const { return create_count_; }

  // The methods invoked so far, in order, including the ones that failed or
  // were unsupported.
  const std::vector<std::wstring>& invoked_methods() const {
    return invoked_methods_;
  }

  // The current registrations, which are nullopt while not registered.
  const std::optional<UriRegistration>& uri_registration() const {
    return uri_registration_;
  }
  const std::optional<TileRegistration>& tile_registration() const {
    return tile_registration_;
  }

  // Delivers a URI launch of `uri` to the registered window, the way the shell
  // does: as a NUL-terminated `WM_COPYDATA` payload tagged with the registered
  // copy data format id. Requires a URI launch registration.
  void LaunchUri(std::wstring_view uri);

  // Delivers a tile launch to the registered window, as the registered
  // message. Requires a tile launch registration.
  void LaunchTile();

 private:
  class ClassFactory;
  class Invoker;

  bool IsMethodSupported(std::wstring_view method) const;

  // Called when the API is created, and when one of its methods is invoked.
  HRESULT OnCreate();
  HRESULT OnInvoke(std::wstring_view method,
                   INamedPropertyStore* args,
                   BOOL* is_supported);

  HRESULT create_result_;
  int create_count_ = 0;
  base::flat_map<std::wstring, HRESULT> invoke_results_;
  base::flat_set<std::wstring> unsupported_methods_;
  std::vector<std::wstring> invoked_methods_;
  std::optional<UriRegistration> uri_registration_;
  std::optional<TileRegistration> tile_registration_;
  // Identifies the class object registered with COM.
  DWORD class_object_cookie_ = 0;
  base::WeakPtrFactory<FakeDirectLaunchShell> weak_factory_{this};
};

}  // namespace direct_launch

#endif  // CHROME_BROWSER_WIN_DIRECT_LAUNCH_FAKE_DIRECT_LAUNCH_SHELL_H_
