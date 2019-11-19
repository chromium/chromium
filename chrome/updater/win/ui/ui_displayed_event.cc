// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_displayed_event.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/updater/win/ui/constants.h"
#include "chrome/updater/win/util.h"

namespace updater {
namespace ui {

HRESULT UIDisplayedEventManager::CreateEvent(bool is_machine) {
  DCHECK(!IsEventHandleInitialized());
  return CreateUniqueEventInEnvironment(
      kLegacyUiDisplayedEventEnvironmentVariableName, is_machine,
      ScopedKernelHANDLE::Receiver(ui_displayed_event_).get());
}

HRESULT UIDisplayedEventManager::GetEvent(bool is_machine,
                                          HANDLE* ui_displayed_event) {
  DCHECK(ui_displayed_event);
  *ui_displayed_event = nullptr;
  if (IsEventHandleInitialized()) {
    *ui_displayed_event = ui_displayed_event_.get();
    return S_OK;
  }

  HRESULT hr = OpenUniqueEventFromEnvironment(
      kLegacyUiDisplayedEventEnvironmentVariableName, is_machine,
      ScopedKernelHANDLE::Receiver(ui_displayed_event_).get());
  if (FAILED(hr))
    return hr;

  *ui_displayed_event = ui_displayed_event_.get();
  return S_OK;
}

void UIDisplayedEventManager::SignalEvent(bool is_machine) {
  if (!IsEventHandleInitialized()) {
    HRESULT hr = GetEvent(
        is_machine, ScopedKernelHANDLE::Receiver(ui_displayed_event_).get());
    if (HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND) == hr)
      hr = CreateEvent(is_machine);
    if (FAILED(hr)) {
      // We may display two UIs in this case.
      ui_displayed_event_.reset();
      return;
    }
  }

  DCHECK(IsEventHandleInitialized());
  ::SetEvent(ui_displayed_event_.get());
}

bool UIDisplayedEventManager::IsEventHandleInitialized() {
  return ui_displayed_event_.is_valid();
}

ScopedKernelHANDLE UIDisplayedEventManager::ui_displayed_event_;

}  // namespace ui
}  // namespace updater
