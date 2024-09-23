// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/parental_controls.h"

#include <windows.h>

#include <combaseapi.h>
#include <winerror.h>
#include <wpcapi.h>
#include <wrl/client.h>

#include <string>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/strcat_win.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_types.h"

namespace {

static bool g_has_called_initialize_win_parental_controls_ = false;

// This singleton allows us to attempt to calculate the Platform Parental
// Controls enabled value on a worker thread before the UI thread needs the
// value. If the UI thread finishes sooner than we expect, that's no worse than
// today where we block.
class WinParentalControlsValue {
 public:
  static WinParentalControlsValue* GetInstance() {
    return base::Singleton<WinParentalControlsValue>::get();
  }

  const WinParentalControls& parental_controls() const {
    return parental_controls_;
  }

 private:
  friend struct base::DefaultSingletonTraits<WinParentalControlsValue>;

  WinParentalControlsValue() : parental_controls_(GetParentalControls()) {}

  ~WinParentalControlsValue() = default;

  WinParentalControlsValue(const WinParentalControlsValue&) = delete;
  WinParentalControlsValue& operator=(const WinParentalControlsValue&) = delete;

  // Returns the Windows Parental control enablements. This feature is available
  // on Windows 7 and beyond. This function should be called on a COM
  // Initialized thread and is potentially blocking.
  static WinParentalControls GetParentalControlsFromApi() {
    // This call may block and be called from the UI thread, which is
    // unfortunate, but we want to at least make sure that we've attempted to
    // call InitializeWinParentalControls() in an attempt to load it early so
    // that we don't need to block.
    //
    // Note that this CHECK replaced a previous base::ScopedBlockingCall, which
    // was incorrect because there were no guarantees that
    // InitializeWinParentalControls() would finish executing asynchronously
    // before the value was needed. See https://crbug.com/1411815#c7.
    if (!g_has_called_initialize_win_parental_controls_) {
      // This uses CHECK_IS_TEST() to skip verifying that
      // InitializeWinParentalControls() got called in tests because updating
      // all test fixtures to call it seemed daunting.
      CHECK_IS_TEST();
    }
    Microsoft::WRL::ComPtr<IWindowsParentalControlsCore> parent_controls;
    HRESULT hr = ::CoCreateInstance(__uuidof(WindowsParentalControls), nullptr,
                                    CLSCTX_ALL, IID_PPV_ARGS(&parent_controls));
    if (FAILED(hr))
      return WinParentalControls();

    Microsoft::WRL::ComPtr<IWPCSettings> settings;
    hr = parent_controls->GetUserSettings(nullptr, &settings);
    if (FAILED(hr))
      return WinParentalControls();

    DWORD restrictions = 0;
    settings->GetRestrictions(&restrictions);

    WinParentalControls controls = {
        restrictions != WPCFLAG_NO_RESTRICTION /* any_restrictions */,
        (restrictions & WPCFLAG_LOGGING_REQUIRED) == WPCFLAG_LOGGING_REQUIRED
        /* logging_required */,
        (restrictions & WPCFLAG_WEB_FILTERED) == WPCFLAG_WEB_FILTERED
        /* web_filter */
    };
    return controls;
  }

  // Update |controls| with parental controls found to be active by reading
  // parental controls configuration from the registry. May be necessary on
  // Win10 where the APIs are not fully supported and may not always accurately
  // report such state.
  //
  // TODO(ericorth@chromium.org): Detect |logging_required| configuration,
  // rather than just web filtering.
  static void UpdateParentalControlsFromRegistry(
      WinParentalControls* controls) {
    DCHECK(controls);

    std::wstring user_sid;
    if (!base::win::GetUserSidString(&user_sid))
      return;

    std::wstring web_filter_key_path =
        base::StrCat({L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Parental "
                      L"Controls\\Users\\",
                      user_sid, L"\\Web"});
    base::win::RegKey web_filter_key(
        HKEY_LOCAL_MACHINE, web_filter_key_path.c_str(), KEY_QUERY_VALUE);
    if (!web_filter_key.Valid())
      return;

    // Web filtering is in use if the key contains any non-zero "Filter On"
    // value.
    DWORD filter_on_value;
    if (web_filter_key.ReadValueDW(L"Filter On", &filter_on_value) ==
            ERROR_SUCCESS &&
        filter_on_value) {
      controls->any_restrictions = true;
      controls->web_filter = true;
    }
  }

  static WinParentalControls GetParentalControls() {
    WinParentalControls controls = GetParentalControlsFromApi();

    // Parental controls APIs are not fully supported in Win10 and beyond, so
    // check registry properties for restictions.
    UpdateParentalControlsFromRegistry(&controls);

    return controls;
  }

  const WinParentalControls parental_controls_;
};

}  // namespace

void InitializeWinParentalControls() {
  g_has_called_initialize_win_parental_controls_ = true;
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE, base::BindOnce(base::IgnoreResult(
                                &WinParentalControlsValue::GetInstance)));
}

const WinParentalControls& GetWinParentalControls() {
  return WinParentalControlsValue::GetInstance()->parental_controls();
}
