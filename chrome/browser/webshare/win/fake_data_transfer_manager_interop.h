// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_INTEROP_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_INTEROP_H_

#include <shlobj.h>
#include <wrl/implements.h>
#include <map>

#include "base/functional/callback_forward.h"

namespace webshare {

class FakeDataTransferManager;

// Provides an implementation of IDataTransferManagerInterop for use in GTests.
//
// Like the Windows implementation, this class cloaks its implementation of
// IDataTransferManagerInterop to closely match casting behaviors.
class FakeDataTransferManagerInterop final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
          Microsoft::WRL::CloakedIid<IDataTransferManagerInterop>> {
 public:
  // Behavior options for the ShowShareUIForWindow API
  enum ShowShareUIForWindowBehavior {
    // Returns a failed value without invoking/scheduling the DataRequested
    // event/handler.
    FailImmediately,
    // Invokes the DataRequested event/handler synchronously as part of the
    // original invoking call. This matches the behavior exposed by Windows
    // under various edge-case scenarios.
    InvokeEventSynchronously,
    // Invokes the DataRequested event/handler synchronously as part of the
    // original invoking call, but then returns a failure result.
    InvokeEventSynchronouslyAndReturnFailure,
    // Schedules the invocation of the DataRequested event/handler to happen
    // automatically, outside of the original invoking call. This matches the
    // the most common behavior exposed by Windows.
    ScheduleEvent,
    // Returns a success value without invoking/scheduling the DataRequested
    // event/handler. To later invoke the DataRequested event/handler, see
    // |GetDataRequestedInvoker|.
    SucceedWithoutAction
  };

  FakeDataTransferManagerInterop();
  FakeDataTransferManagerInterop(const FakeDataTransferManagerInterop&) =
      delete;
  FakeDataTransferManagerInterop& operator=(
      const FakeDataTransferManagerInterop&) = delete;
  ~FakeDataTransferManagerInterop() final;

  // IDataTransferManagerInterop:
  IFACEMETHODIMP GetForWindow(HWND app_window,
                              REFIID riid,
                              void** data_transfer_manager) final;
  IFACEMETHODIMP ShowShareUIForWindow(HWND app_window) final;

  // Returns a callback that captures a reference to the current DataRequested
  // event handler and, when invoked, triggers that handler.
  //
  // If the registered handler changes after this method is called the callback
  // will still trigger the previous event handler, not a newly registered one.
  base::OnceClosure GetDataRequestedInvoker(HWND app_window);

  // Checks if there are any listeners registered for the DataRequested event
  // on the given |app_window|.
  bool HasDataRequestedListener(HWND app_window);

  void SetShowShareUIForWindowBehavior(ShowShareUIForWindowBehavior behavior);

 private:
  ShowShareUIForWindowBehavior show_share_ui_for_window_behavior_ =
      ShowShareUIForWindowBehavior::ScheduleEvent;
  std::map<HWND, Microsoft::WRL::ComPtr<FakeDataTransferManager>> managers_;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_INTEROP_H_
