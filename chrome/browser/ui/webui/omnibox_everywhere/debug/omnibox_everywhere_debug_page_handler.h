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

class Profile;

namespace omnibox_everywhere_debug {

class OmniboxEverywhereDebugPageHandler : public mojom::PageHandler {
 public:
  OmniboxEverywhereDebugPageHandler(
      Profile* profile,
      mojo::PendingRemote<mojom::Page> page,
      mojo::PendingReceiver<mojom::PageHandler> receiver);

  OmniboxEverywhereDebugPageHandler(const OmniboxEverywhereDebugPageHandler&) =
      delete;
  OmniboxEverywhereDebugPageHandler& operator=(
      const OmniboxEverywhereDebugPageHandler&) = delete;

  ~OmniboxEverywhereDebugPageHandler() override;

  // mojom::PageHandler:
  void SetBackgroundModeEnabled(bool enabled) override;
  void GetBackgroundModeEnabled(
      GetBackgroundModeEnabledCallback callback) override;

  void SetHotkeyEnabled(bool enabled) override;
  void GetHotkeyEnabled(GetHotkeyEnabledCallback callback) override;

  void InvokeOmniboxEverywhere(mojom::InvocationSource source) override;

 private:
  void OnPrefChanged(const std::string& pref_name);

  raw_ptr<Profile> profile_;
  mojo::Remote<mojom::Page> page_;
  mojo::Receiver<mojom::PageHandler> receiver_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace omnibox_everywhere_debug

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_DEBUG_OMNIBOX_EVERYWHERE_DEBUG_PAGE_HANDLER_H_
