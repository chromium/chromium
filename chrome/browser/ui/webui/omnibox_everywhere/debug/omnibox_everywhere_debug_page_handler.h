// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_DEBUG_OMNIBOX_EVERYWHERE_DEBUG_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_DEBUG_OMNIBOX_EVERYWHERE_DEBUG_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/debug/omnibox_everywhere_debug.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebUI;
}

class Profile;
class UserEducationInternalsPageHandlerImpl;

namespace omnibox_everywhere_debug {

class OmniboxEverywhereDebugPageHandler : public mojom::PageHandler {
 public:
  OmniboxEverywhereDebugPageHandler(
      content::WebUI* web_ui,
      Profile* profile,
      mojo::PendingRemote<mojom::Page> page,
      mojo::PendingReceiver<mojom::PageHandler> receiver);

  OmniboxEverywhereDebugPageHandler(const OmniboxEverywhereDebugPageHandler&) =
      delete;
  OmniboxEverywhereDebugPageHandler& operator=(
      const OmniboxEverywhereDebugPageHandler&) = delete;

  ~OmniboxEverywhereDebugPageHandler() override;

  void SetBackgroundModeEnabled(bool enabled) override;
  void GetBackgroundModeEnabled(
      GetBackgroundModeEnabledCallback callback) override;

  void SetLaunchOnStartupEnabled(bool enabled) override;
  void GetLaunchOnStartupEnabled(
      GetLaunchOnStartupEnabledCallback callback) override;

  void SetHotkeyEnabled(bool enabled) override;
  void GetHotkeyEnabled(GetHotkeyEnabledCallback callback) override;

  void SetEphemeralModelEnabled(bool enabled) override;
  void GetEphemeralModelEnabled(
      GetEphemeralModelEnabledCallback callback) override;

  void InvokeOmniboxEverywhere(mojom::InvocationSource source) override;

  void ShowLensIph() override;

  void CreateStartMenuShortcut(
      CreateStartMenuShortcutCallback callback) override;

  void PinToTaskbar(PinToTaskbarCallback callback) override;

 private:
  void OnPrefChanged(const std::string& pref_name);

  raw_ptr<Profile> profile_;
  mojo::Remote<mojom::Page> page_;
  mojo::Receiver<mojom::PageHandler> receiver_;

  std::unique_ptr<UserEducationInternalsPageHandlerImpl>
      user_education_internals_page_handler_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace omnibox_everywhere_debug

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_DEBUG_OMNIBOX_EVERYWHERE_DEBUG_PAGE_HANDLER_H_
