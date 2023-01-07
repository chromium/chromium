// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_SHOW_SHARE_UI_FOR_WINDOW_OPERATION_H_
#define CHROME_BROWSER_WEBSHARE_WIN_SHOW_SHARE_UI_FOR_WINDOW_OPERATION_H_

#include <wrl/client.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/win/core_winrt_util.h"

namespace ABI {
namespace Windows {
namespace ApplicationModel {
namespace DataTransfer {
class IDataRequestedEventArgs;
class IDataTransferManager;
}  // namespace DataTransfer
}  // namespace ApplicationModel
}  // namespace Windows
}  // namespace ABI

namespace webshare {

// Represents a call to ShowShareUIForWindow in an async fashion.
class ShowShareUIForWindowOperation {
 public:
  using DataRequestedCallback = base::OnceCallback<void(
      ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs*)>;

  explicit ShowShareUIForWindowOperation(const HWND hwnd);
  ShowShareUIForWindowOperation(const ShowShareUIForWindowOperation&) = delete;
  ShowShareUIForWindowOperation& operator=(
      const ShowShareUIForWindowOperation&) = delete;
  ~ShowShareUIForWindowOperation();

  // Test hook for overriding the base RoGetActivationFactory function
  static void SetRoGetActivationFactoryFunctionForTesting(
      decltype(&base::win::RoGetActivationFactory) value);

  static constexpr base::TimeDelta max_execution_time_for_testing() {
    return kMaxExecutionTime;
  }

  // Requests the Window's Share operation for the previously supplied |hwnd|
  // and uses the |data_requested_callback| to supply the operation with the
  // data to share. This call does not impact the lifetime of this class, so the
  // caller must keep this instance alive until it has completed or the caller
  // no longer desires the operation to continue.
  //
  // The provided |data_requested_callback| will be invoked either
  // synchronously as part of this call, or asynchronously at a
  // later point when the OS Share operation requests it. In both cases, when
  // the |data_requested_callback| is invoked it will be on the UI thread with
  // |IDataRequestedEventArgs| from the OS. If an error is encountered the
  // |data_requested_callback| will be invoked without any arguments.
  //
  // This should only be called from the UI thread.
  void Run(DataRequestedCallback data_requested_callback);

 private:
  static constexpr base::TimeDelta kMaxExecutionTime = base::Seconds(30);

  void Cancel();
  void OnDataRequested(
      ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager*
          data_transfer_manager,
      ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs*
          event_args);

  DataRequestedCallback data_requested_callback_;
  const HWND hwnd_;
  base::OnceClosure remove_data_requested_listener_;
  base::WeakPtrFactory<ShowShareUIForWindowOperation> weak_factory_{this};
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_SHOW_SHARE_UI_FOR_WINDOW_OPERATION_H_
