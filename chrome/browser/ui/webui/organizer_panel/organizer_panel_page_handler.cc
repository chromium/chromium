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
#include "chrome/browser/ui/tabs/organizer/organizer_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
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
  pref_change_registrar_.Init(&GetPrefs());
  auto pref_changed_callback =
      base::BindRepeating(&OrganizerPanelPageHandler::OnSectionsStateChanged,
                          base::Unretained(this));
  pref_change_registrar_.Add(prefs::kOrganizerPanelSectionsExpanded,
                             pref_changed_callback);
  pref_change_registrar_.Add(prefs::kOrganizerPanelSectionsShowAll,
                             pref_changed_callback);

  browser_window_changed_subscription_ =
      webui::RegisterBrowserWindowInterfaceChanged(
          web_contents_,
          base::BindRepeating(
              &OrganizerPanelPageHandler::OnBrowserWindowInterfaceChanged,
              base::Unretained(this)));
  OnBrowserWindowInterfaceChanged();
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
  const organizer::SectionState state = organizer::GetSectionState(
      GetPrefs(), organizer::GetSectionIdFromString(section_id));
  std::move(callback).Run(organizer_panel::mojom::SectionState::New(
      state.expanded, state.show_all));
}

void OrganizerPanelPageHandler::SetSectionExpanded(
    const std::string& section_id,
    bool expanded) {
  organizer::SetSectionExpanded(
      GetPrefs(), organizer::GetSectionIdFromString(section_id), expanded);
}

void OrganizerPanelPageHandler::SetSectionShowAll(const std::string& section_id,
                                                  bool show_all) {
  organizer::SetSectionShowAll(
      GetPrefs(), organizer::GetSectionIdFromString(section_id), show_all);
}

void OrganizerPanelPageHandler::OnBrowserWindowInterfaceChanged() {
  vertical_tab_strip_mode_subscription_ = {};
  organizer_panel_subscription_ = {};
  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    return;
  }
  if (auto* controller = tabs::VerticalTabStripStateController::From(browser)) {
    OnVerticalTabStripModeChanged(controller);
    vertical_tab_strip_mode_subscription_ =
        controller->RegisterOnModeChanged(base::BindRepeating(
            &OrganizerPanelPageHandler::OnVerticalTabStripModeChanged,
            base::Unretained(this)));
  }
  if (auto* panel_controller = OrganizerPanelController::From(browser)) {
    UpdateInTrayView(panel_controller);
    organizer_panel_subscription_ = panel_controller->RegisterOnStateChanged(
        base::BindRepeating(&OrganizerPanelPageHandler::UpdateInTrayView,
                            base::Unretained(this)));
  }
}

void OrganizerPanelPageHandler::OnVerticalTabStripModeChanged(
    tabs::VerticalTabStripStateController* controller) {
  page_->OnVerticalTabsEnabledChanged(controller->ShouldDisplayVerticalTabs());
}

void OrganizerPanelPageHandler::UpdateInTrayView(
    OrganizerPanelController* controller) {
  CHECK(controller);
  const auto location = controller->GetCurrentOrganizerPanelLocation();
  if (location == OrganizerPanelLocation::kNone) {
    return;
  }
  const bool in_tray = location == OrganizerPanelLocation::kOrganizerTray;
  if (!in_tray_view_.has_value() || *in_tray_view_ != in_tray) {
    in_tray_view_ = in_tray;
    page_->OnIsInTrayViewChanged(in_tray);
  }
}

void OrganizerPanelPageHandler::OnSectionsStateChanged() {
  page_->OnSectionsStateChanged(
      base::MakeFlatMap<std::string, organizer_panel::mojom::SectionStatePtr>(
          organizer::GetAllSectionStates(GetPrefs()), {},
          [](const organizer::SectionStateEntry& entry) {
            return std::make_pair(
                std::string(organizer::GetSectionIdString(entry.section_id)),
                organizer_panel::mojom::SectionState::New(
                    entry.state.expanded, entry.state.show_all));
          }));
}

PrefService& OrganizerPanelPageHandler::GetPrefs() const {
  PrefService* const prefs = profile_->GetPrefs();
  CHECK(prefs);
  return *prefs;
}
