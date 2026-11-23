// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class OrganizerPanelController;
class PrefService;
class Profile;
class TopChromeWebUIController;

namespace content {
class WebContents;
}  // namespace content

class OrganizerPanelPageHandler : public organizer_panel::mojom::PageHandler {
 public:
  OrganizerPanelPageHandler(
      mojo::PendingReceiver<organizer_panel::mojom::PageHandler> receiver,
      mojo::PendingRemote<organizer_panel::mojom::Page> page,
      content::WebContents* web_contents,
      TopChromeWebUIController* webui_controller);
  OrganizerPanelPageHandler(const OrganizerPanelPageHandler&) = delete;
  OrganizerPanelPageHandler& operator=(const OrganizerPanelPageHandler&) =
      delete;
  ~OrganizerPanelPageHandler() override;

  // organizer_panel::mojom::PageHandler:
  void ShowUI() override;
  void ClosePanel() override;
  void GetSectionState(organizer_panel::mojom::SectionId section_id,
                       GetSectionStateCallback callback) override;
  void SetSectionExpanded(organizer_panel::mojom::SectionId section_id,
                          bool expanded) override;
  void SetSectionShowAll(organizer_panel::mojom::SectionId section_id,
                         bool show_all) override;
  void RecordTimeToAction(organizer_panel::mojom::Action action,
                          bool is_filtered) override;

 private:
  void OnBrowserWindowInterfaceChanged();
  void OnSectionsStateChanged();

  void UpdateInTrayView(OrganizerPanelController* controller);
  PrefService& GetPrefs() const;

  mojo::Receiver<organizer_panel::mojom::PageHandler> receiver_;
  mojo::Remote<organizer_panel::mojom::Page> page_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<TopChromeWebUIController> webui_controller_;
  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
  std::optional<bool> in_tray_view_;
  base::CallbackListSubscription browser_window_changed_subscription_;
  base::CallbackListSubscription organizer_panel_subscription_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_
