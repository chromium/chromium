// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_page_handler.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"

OrganizerPanelPageHandler::OrganizerPanelPageHandler(
    mojo::PendingReceiver<organizer_panel::mojom::PageHandler> receiver,
    mojo::PendingRemote<organizer_panel::mojom::Page> page,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(
          web_contents ? web_contents->GetBrowserContext() : nullptr)) {
  CHECK(web_contents_);
  CHECK(profile_);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kOrganizerPanelSectionsExpanded,
      base::BindRepeating(&OrganizerPanelPageHandler::OnSectionsExpandedChanged,
                          base::Unretained(this)));
}

OrganizerPanelPageHandler::~OrganizerPanelPageHandler() = default;

void OrganizerPanelPageHandler::ClosePanel() {
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents_);
  CHECK(browser);

  auto* controller = OrganizerPanelController::From(browser);
  CHECK(controller);
  controller->SetOrganizerVisible(false);
}

void OrganizerPanelPageHandler::IsSectionExpanded(
    const std::string& section_id,
    IsSectionExpandedCallback callback) {
  const base::DictValue& sections_expanded =
      profile_->GetPrefs()->GetDict(prefs::kOrganizerPanelSectionsExpanded);
  std::move(callback).Run(
      sections_expanded.FindBool(section_id).value_or(true));
}

void OrganizerPanelPageHandler::SetSectionExpanded(
    const std::string& section_id,
    bool expanded) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kOrganizerPanelSectionsExpanded);
  update->Set(section_id, expanded);
}

void OrganizerPanelPageHandler::OnSectionsExpandedChanged() {
  const base::DictValue& sections_expanded_pref =
      profile_->GetPrefs()->GetDict(prefs::kOrganizerPanelSectionsExpanded);
  base::flat_map<std::string, bool> sections_expanded;
  for (const auto [section_id, expanded] : sections_expanded_pref) {
    CHECK(expanded.is_bool());
    sections_expanded[section_id] = expanded.GetBool();
  }
  page_->OnSectionsExpandedChanged(std::move(sections_expanded));
}
