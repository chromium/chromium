// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_FOREIGN_TABS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_FOREIGN_TABS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs.mojom.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include <optional>

#include "base/callback_list.h"

class OrganizerPanelController;
#endif

class Profile;

namespace content {
class WebContents;
}  // namespace content

class ForeignTabsPageHandler
    : public organizer_panel::mojom::ForeignTabsPageHandler {
 public:
  ForeignTabsPageHandler(
      mojo::PendingReceiver<organizer_panel::mojom::ForeignTabsPageHandler>
          receiver,
      content::WebContents* web_contents);
  ForeignTabsPageHandler(const ForeignTabsPageHandler&) = delete;
  ForeignTabsPageHandler& operator=(const ForeignTabsPageHandler&) = delete;
  ~ForeignTabsPageHandler() override;

  // organizer_panel::mojom::ForeignTabsPageHandler:
  void GetForeignTabs(GetForeignTabsCallback callback) override;
  void OpenTab(const std::string& session_tag, int32_t session_tab_id) override;
  void ShowCrossDeviceSignInPromo() override;

 private:
  bool CanShowCrossDeviceSignInPromo(bool has_tabs);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void OnBrowserWindowInterfaceChanged();
  void OnOrganizerPanelStateChanged(OrganizerPanelController* controller);
#endif

  mojo::Receiver<organizer_panel::mojom::ForeignTabsPageHandler> receiver_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<Profile> profile_;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bool is_panel_visible_ = false;
  std::optional<bool> can_show_cross_device_sign_in_promo_;
  base::CallbackListSubscription browser_window_changed_subscription_;
  base::CallbackListSubscription organizer_panel_subscription_;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_FOREIGN_TABS_PAGE_HANDLER_H_
