// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_page_handler.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
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

namespace {

constexpr std::array<std::string_view, 4> kSectionIds = {
    "cross-device-tabs",
    "open-tabs",
    "recently-closed",
    "tab-groups",
};

void CheckValidSectionId(std::string_view section_id) {
  CHECK(std::ranges::contains(kSectionIds, section_id))
      << "Unknown section ID \"" << section_id
      << "\". Please add new section IDs to kSectionIds.";
}

}  // namespace

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
  auto pref_changed_callback =
      base::BindRepeating(&OrganizerPanelPageHandler::OnSectionsStateChanged,
                          base::Unretained(this));
  pref_change_registrar_.Add(prefs::kOrganizerPanelSectionsExpanded,
                             pref_changed_callback);
  pref_change_registrar_.Add(prefs::kOrganizerPanelSectionsShowAll,
                             pref_changed_callback);
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

void OrganizerPanelPageHandler::GetSectionState(
    const std::string& section_id,
    GetSectionStateCallback callback) {
  std::move(callback).Run(GetSectionStateForId(section_id));
}

void OrganizerPanelPageHandler::SetSectionExpanded(
    const std::string& section_id,
    bool expanded) {
  CheckValidSectionId(section_id);
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kOrganizerPanelSectionsExpanded);
  update->Set(section_id, expanded);
}

void OrganizerPanelPageHandler::SetSectionShowAll(const std::string& section_id,
                                                  bool show_all) {
  CheckValidSectionId(section_id);
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kOrganizerPanelSectionsShowAll);
  update->Set(section_id, show_all);
}

organizer_panel::mojom::SectionStatePtr
OrganizerPanelPageHandler::GetSectionStateForId(
    const std::string& section_id) const {
  CheckValidSectionId(section_id);
  PrefService* prefs = profile_->GetPrefs();
  auto state = organizer_panel::mojom::SectionState::New();
  if (std::optional<bool> expanded =
          prefs->GetDict(prefs::kOrganizerPanelSectionsExpanded)
              .FindBool(section_id)) {
    state->expanded = *expanded;
  }
  if (std::optional<bool> show_all =
          prefs->GetDict(prefs::kOrganizerPanelSectionsShowAll)
              .FindBool(section_id)) {
    state->show_all = *show_all;
  }
  return state;
}

void OrganizerPanelPageHandler::OnSectionsStateChanged() {
  base::flat_map<std::string, organizer_panel::mojom::SectionStatePtr>
      sections_state;
  for (std::string_view section_id : kSectionIds) {
    sections_state[std::string(section_id)] =
        GetSectionStateForId(std::string(section_id));
  }
  page_->OnSectionsStateChanged(std::move(sections_state));
}
