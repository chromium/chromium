// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/titlebar_config.h"

#include <Windows.h>
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/win/mica_titlebar.h"
#include "chrome/common/chrome_switches.h"

bool ShouldCustomDrawSystemTitlebar() {
  // Some extra code added here because those with pre-win8 and no DWM will have to fallback on the custom titlebar.
  BOOL result = FALSE;
	
  typedef HRESULT(WINAPI* DwmIsCompositionEnabledFunc)(BOOL* enabled);
  DwmIsCompositionEnabledFunc func_ = nullptr;
	
  HMODULE dwmapi_library_ = LoadLibraryW(L"dwmapi.dll");
  if (dwmapi_library_) {
    func_ = reinterpret_cast<DwmIsCompositionEnabledFunc>(
        GetProcAddress(dwmapi_library_, "DwmIsCompositionEnabled"));
  }
  else
	  return true;
  
  if (func_) {
	  func_(&result);
  }
  else
	  return true;
  // Cache flag lookup.
  static const bool custom_titlebar_disabled =
      base::CommandLine::InitializedForCurrentProcess() &&
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "disable-windows10-custom-titlebar") ||
	  base::CommandLine::ForCurrentProcess()->HasSwitch(
          "windows11-mica-titlebar"));

  return (!custom_titlebar_disabled &&
         base::win::GetVersion() >= base::win::Version::WIN10) || !result;
}

bool ShouldBrowserCustomDrawTitlebar(BrowserView* browser_view) {
  return browser_view->GetIsWebAppType() ||
         ShouldCustomDrawSystemTitlebar();
}
