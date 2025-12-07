// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_IA2_H_
#define CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_IA2_H_

#include <windows.h>

#include <wrl/client.h>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/win/scoped_handle.h"
#include "chrome/test/accessibility/ax_client/ax_client_impl.h"

struct IAccessible;

namespace ax_client {

struct EventHookHandleTraits {
  using Handle = HWINEVENTHOOK;
  static bool CloseHandle(Handle handle);
  static bool IsHandleValid(Handle handle) { return handle != 0; }
  static Handle NullHandle() { return 0U; }
};

class AxClientIa2 : public AxClientImpl {
 public:
  AxClientIa2();
  ~AxClientIa2() override;

  // AxClientImpl:
  HRESULT Initialize(HWND hwnd) override;
  HRESULT FindAll() override;
  void Shutdown() override;

 private:
  using ScopedEventHookHandle =
      base::win::GenericScopedHandle<EventHookHandleTraits,
                                     base::win::DummyVerifierTraits>;

  // The WinEvent proc by which the client is notified of changes in the remote
  // process's windows.
  static void CALLBACK OnWinEvent(HWINEVENTHOOK win_event_hook,
                                  DWORD event,
                                  HWND hwnd,
                                  LONG id_object,
                                  LONG id_child,
                                  DWORD id_event_thread,
                                  DWORD event_time_ms);

  void OnObjectDestroy(HWND hwnd, LONG id_object, LONG id_child);

  // The top-level window on which this client operates.
  HWND main_window_ = 0;

  // The accessible object for the top-level window.
  Microsoft::WRL::ComPtr<IAccessible> main_window_acc_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The hook to receive events from the remote process.
  ScopedEventHookHandle event_hook_handle_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AxClientIa2> weak_ptr_factory_{this};
};

}  // namespace ax_client

#endif  // CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_IA2_H_
