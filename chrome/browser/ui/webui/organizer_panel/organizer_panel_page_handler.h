// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

class OrganizerPanelPageHandler : public organizer_panel::mojom::PageHandler {
 public:
  OrganizerPanelPageHandler(
      mojo::PendingReceiver<organizer_panel::mojom::PageHandler> receiver,
      mojo::PendingRemote<organizer_panel::mojom::Page> page,
      content::WebContents* web_contents);
  OrganizerPanelPageHandler(const OrganizerPanelPageHandler&) = delete;
  OrganizerPanelPageHandler& operator=(const OrganizerPanelPageHandler&) =
      delete;
  ~OrganizerPanelPageHandler() override;

  // organizer_panel::mojom::PageHandler:
  void ClosePanel() override;
  void IsSectionExpanded(const std::string& section_id,
                         IsSectionExpandedCallback callback) override;
  void SetSectionExpanded(const std::string& section_id,
                          bool expanded) override;

 private:
  void OnSectionsExpandedChanged();

  mojo::Receiver<organizer_panel::mojom::PageHandler> receiver_;
  mojo::Remote<organizer_panel::mojom::Page> page_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_
