// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_displayed_event.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/ui_constants.h"

namespace updater::ui {

HRESULT UIDisplayedEventManager::CreateEvent(UpdaterScope scope) {
  DCHECK(!IsEventHandleInitialized());
  return CreateUniqueEventInEnvironment(
      kLegacyUiDisplayedEventEnvironmentVariableName, scope,
      ScopedKernelHANDLE::Receiver(GetUIDisplayedEvent()).get());
}

HRESULT UIDisplayedEventManager::GetEvent(UpdaterScope scope,
                                          HANDLE* ui_displayed_event) {
  DCHECK(ui_displayed_event);
  *ui_displayed_event = nullptr;
  if (IsEventHandleInitialized()) {
    *ui_displayed_event = GetUIDisplayedEvent().get();
    return S_OK;
  }

  HRESULT hr = OpenUniqueEventFromEnvironment(
      kLegacyUiDisplayedEventEnvironmentVariableName, scope,
      ScopedKernelHANDLE::Receiver(GetUIDisplayedEvent()).get());
  if (FAILED(hr)) {
    return hr;
  }

  *ui_displayed_event = GetUIDisplayedEvent().get();
  return S_OK;
}

void UIDisplayedEventManager::SignalEvent(UpdaterScope scope) {
  if (!IsEventHandleInitialized()) {
    HRESULT hr = GetEvent(
        scope, ScopedKernelHANDLE::Receiver(GetUIDisplayedEvent()).get());
    if (HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND) == hr) {
      hr = CreateEvent(scope);
    }
    if (FAILED(hr)) {
      // We may display two UIs in this case.
      GetUIDisplayedEvent().reset();
      return;
    }
  }

  DCHECK(IsEventHandleInitialized());
  ::SetEvent(GetUIDisplayedEvent().get());
}

bool UIDisplayedEventManager::IsEventHandleInitialized() {
  return GetUIDisplayedEvent().is_valid();
}

ScopedKernelHANDLE& UIDisplayedEventManager::GetUIDisplayedEvent() {
  static base::NoDestructor<ScopedKernelHANDLE> ui_displayed_event;
  return *ui_displayed_event;
}

}  // namespace updater::ui
