// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_page_handler.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/user_notes/user_notes_controller.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/common/power_overview.h"
#include "components/power_bookmarks/common/search_params.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "components/user_notes/user_notes_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"

namespace {

const int kCurrentVersionNumber = 1;

side_panel::mojom::NoteOverviewPtr PowerOverviewToMojo(
    const power_bookmarks::PowerOverview& power_overview,
    const GURL& current_tab_url,
    base::WeakPtr<bookmarks::BookmarkModel> bookmark_model) {
  auto* power = power_overview.power();
  DCHECK(power->power_type() ==
         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE);
  DCHECK(power->power_entity()->has_note_entity());
  auto result = side_panel::mojom::NoteOverview::New();
  result->url = power->url();

  // Set title to the first bookmark with the same URL, otherwise fall back to
  // url.
  std::vector<const bookmarks::BookmarkNode*> nodes;
  if (bookmark_model) {
    bookmark_model->GetNodesByURL(power->url(), &nodes);
  }
  if (nodes.size() > 0) {
    result->title = base::UTF16ToUTF8(nodes[0]->GetTitle());
  } else {
    result->title = power->url().spec();
  }

  result->text = power->power_entity()->note_entity().plain_text();
  result->num_notes = power_overview.count();
  result->is_current_tab = (power->url() == current_tab_url);
  result->last_modification_time = power->time_modified();
  return result;
}

side_panel::mojom::NotePtr PowerToMojo(const power_bookmarks::Power& power) {
  DCHECK(power.power_type() ==
         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE);
  DCHECK(power.power_entity()->has_note_entity());
  auto note_entity = power.power_entity()->note_entity();
  auto result = side_panel::mojom::Note::New();
  result->guid = power.guid().AsLowercaseString();
  result->url = power.url();
  result->last_modification_time = power.time_modified();
  result->last_modification_time_text =
      base::UTF16ToUTF8(ui::TimeFormat::Simple(
          ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
          base::Time::Now() - power.time_modified()));
  result->text = note_entity.plain_text();
  return result;
}

bool IsNoteVisible(const power_bookmarks::Power& power) {
  DCHECK(power.power_type() ==
         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE);
  DCHECK(power.power_entity()->has_note_entity());
  return power.power_entity()->note_entity().current_note_version() <=
         kCurrentVersionNumber;
}

std::unique_ptr<power_bookmarks::Power> MakePower(const std::string& guid,
                                                  const std::string& text,
                                                  GURL url,
                                                  bool is_create) {
  auto power_entity = std::make_unique<sync_pb::PowerEntity>();
  power_entity->mutable_note_entity()->set_plain_text(text);
  power_entity->mutable_note_entity()->set_current_note_version(
      kCurrentVersionNumber);
  power_entity->mutable_note_entity()->set_target_type(
      sync_pb::NoteEntity::TARGET_TYPE_PAGE);
  auto result =
      std::make_unique<power_bookmarks::Power>(std::move(power_entity));
  result->set_guid(base::Uuid::ParseLowercase(guid));
  result->set_power_type(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE);
  if (is_create)
    result->set_time_added(base::Time::Now());
  result->set_time_modified(base::Time::Now());
  result->set_url(url);
  return result;
}

}  // namespace

UserNotesPageHandler::UserNotesPageHandler(
    mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::UserNotesPage> page,
    Profile* profile,
    Browser* browser,
    bool start_creation_flow,
    UserNotesSidePanelUI* user_notes_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile),
      service_(PowerBookmarkServiceFactory::GetForBrowserContext(profile_)),
      browser_(browser),
      user_notes_ui_(user_notes_ui) {
  if (!UserNotesController::IsUserNotesSupported(profile_)) {
    return;
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  if (bookmark_model) {
    bookmark_model_ = bookmark_model->AsWeakPtr();
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kUserNotesSortByNewest,
      base::BindRepeating(&UserNotesPageHandler::OnSortByNewestPrefChanged,
                          base::Unretained(this)));

  service_->AddObserver(this);
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);
  Observe(browser_->tab_strip_model()->GetActiveWebContents());
  UpdateCurrentTabUrl();
  if (start_creation_flow) {
    StartNoteCreation(false);
  }
}

UserNotesPageHandler::~UserNotesPageHandler() {
  if (!UserNotesController::IsUserNotesSupported(profile_)) {
    return;
  }
  service_->RemoveObserver(this);
  browser_->tab_strip_model()->RemoveObserver(this);
  Observe(nullptr);
}

void UserNotesPageHandler::ShowUI() {
  auto embedder = user_notes_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void UserNotesPageHandler::GetNoteOverviews(const std::string& user_input,
                                            GetNoteOverviewsCallback callback) {
  auto service_callback =
      [](GetNoteOverviewsCallback callback, const GURL& current_tab_url,
         base::WeakPtr<bookmarks::BookmarkModel> bookmark_model,
         std::vector<std::unique_ptr<power_bookmarks::PowerOverview>>
             power_overviews) {
        std::vector<side_panel::mojom::NoteOverviewPtr> results;
        for (auto& power_overview : power_overviews) {
          results.push_back(PowerOverviewToMojo(
              *power_overview, current_tab_url, bookmark_model));
        }
        std::move(callback).Run(std::move(results));
      };

  if (user_input.empty()) {
    service_->GetPowerOverviewsForType(
        sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
        base::BindOnce(service_callback, std::move(callback), current_tab_url_,
                       bookmark_model_));
  } else {
    service_->SearchPowerOverviews(
        {.query = user_input,
         .power_type = sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE},
        base::BindOnce(service_callback, std::move(callback), current_tab_url_,
                       bookmark_model_));
  }
}

void UserNotesPageHandler::GetNotesForCurrentTab(
    GetNotesForCurrentTabCallback callback) {
  service_->GetPowersForURL(
      current_tab_url_, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
      base::BindOnce(
          [](GetNotesForCurrentTabCallback callback,
             std::vector<std::unique_ptr<power_bookmarks::Power>> powers) {
            std::vector<side_panel::mojom::NotePtr> results;
            for (auto& power : powers) {
              if (!IsNoteVisible(*power))
                continue;
              results.push_back(PowerToMojo(*power));
            }
            std::move(callback).Run(std::move(results));
          },
          std::move(callback)));
}
void UserNotesPageHandler::NewNoteFinished(const std::string& text,
                                           NewNoteFinishedCallback callback) {
  if (current_tab_url_.is_empty()) {
    LOG(ERROR) << "Note cannot be created with empty url.";
    std::move(callback).Run(false);
    return;
  }
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  service_->CreatePower(
      MakePower(guid, text, current_tab_url_, /*is_create=*/true),
      base::BindOnce([](NewNoteFinishedCallback callback,
                        bool success) { std::move(callback).Run(success); },
                     std::move(callback)));
}

void UserNotesPageHandler::UpdateNote(const std::string& guid,
                                      const std::string& text,
                                      UpdateNoteCallback callback) {
  if (current_tab_url_.is_empty()) {
    LOG(ERROR) << "Note cannot be updated with empty url.";
    std::move(callback).Run(false);
    return;
  }
  service_->UpdatePower(
      MakePower(guid, text, current_tab_url_, /*is_create=*/false),
      base::BindOnce([](UpdateNoteCallback callback,
                        bool success) { std::move(callback).Run(success); },
                     std::move(callback)));
}

void UserNotesPageHandler::DeleteNote(const std::string& guid,
                                      DeleteNoteCallback callback) {
  service_->DeletePower(
      base::Uuid::ParseLowercase(guid),
      base::BindOnce([](DeleteNoteCallback callback,
                        bool success) { std::move(callback).Run(success); },
                     std::move(callback)));
}

void UserNotesPageHandler::DeleteNotesForUrl(
    const ::GURL& url,
    DeleteNotesForUrlCallback callback) {
  service_->DeletePowersForURL(
      url, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
      base::BindOnce([](DeleteNotesForUrlCallback callback,
                        bool success) { std::move(callback).Run(success); },
                     std::move(callback)));
}

void UserNotesPageHandler::NoteOverviewSelected(
    const ::GURL& url,
    ui::mojom::ClickModifiersPtr click_modifiers) {
  WindowOpenDisposition open_location = ui::DispositionFromClick(
      click_modifiers->middle_button, click_modifiers->alt_key,
      click_modifiers->ctrl_key, click_modifiers->meta_key,
      click_modifiers->shift_key);

  content::OpenURLParams params(url, content::Referrer(), open_location,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser_->OpenURL(params);
}

void UserNotesPageHandler::SetSortOrder(bool sort_by_newest) {
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service && pref_service->GetBoolean(prefs::kUserNotesSortByNewest) !=
                          sort_by_newest) {
    pref_service->SetBoolean(prefs::kUserNotesSortByNewest, sort_by_newest);
  }
}

void UserNotesPageHandler::HasNotesInAnyPages(
    HasNotesInAnyPagesCallback callback) {
  // TODO(crbug.com/1419697) Implement a more efficient API to retrieve number
  // of powers for a specific type instead of using GetPowerOverviewsForType
  service_->GetPowerOverviewsForType(
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
      base::BindOnce(
          [](HasNotesInAnyPagesCallback callback,
             std::vector<std::unique_ptr<power_bookmarks::PowerOverview>>
                 power_overviews) {
            bool has_notes = power_overviews.size() > 0;
            std::move(callback).Run(has_notes);
          },
          std::move(callback)));
}

void UserNotesPageHandler::OpenInNewTab(const ::GURL& url) {
  OpenUrl(url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void UserNotesPageHandler::OpenInNewWindow(const ::GURL& url) {
  OpenUrl(url, WindowOpenDisposition::NEW_WINDOW);
}

void UserNotesPageHandler::OpenInIncognitoWindow(const ::GURL& url) {
  OpenUrl(url, WindowOpenDisposition::OFF_THE_RECORD);
}

void UserNotesPageHandler::OnSortByNewestPrefChanged() {
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service) {
    page_->SortByNewestPrefChanged(
        pref_service->GetBoolean(prefs::kUserNotesSortByNewest));
  }
}

void UserNotesPageHandler::StartNoteCreation(bool wait_for_tab_change) {
  if (wait_for_tab_change) {
    start_creation_after_tab_change_ = true;
  } else {
    page_->StartNoteCreation();
  }
}

void UserNotesPageHandler::OnPowersChanged() {
  page_->NotesChanged();
}

void UserNotesPageHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  Observe(selection.new_contents);
  UpdateCurrentTabUrl();
}

void UserNotesPageHandler::PrimaryPageChanged(content::Page& page) {
  UpdateCurrentTabUrl();
}

void UserNotesPageHandler::UpdateCurrentTabUrl() {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (web_contents && current_tab_url_ != web_contents->GetLastCommittedURL()) {
    current_tab_url_ = web_contents->GetLastCommittedURL();
    page_->CurrentTabUrlChanged(start_creation_after_tab_change_);
    start_creation_after_tab_change_ = false;
  }
}

void UserNotesPageHandler::OpenUrl(const ::GURL& url,
                                   WindowOpenDisposition open_location) {
  // We will open in incognito if the user is already in incognito or requesting
  // to open in incognito.
  bool opening_in_incognito =
      (browser_->profile() && browser_->profile()->IsIncognitoProfile()) ||
      open_location == WindowOpenDisposition::OFF_THE_RECORD;
  if (opening_in_incognito &&
      !IsURLAllowedInIncognito(url, browser_->profile())) {
    return;
  }

  NavigateParams params(browser_->profile(), url,
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = open_location;
  params.browser = browser_;
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);

  // If we have opened in a new window, we need to open the side panel to user
  // notes.
  bool opening_in_new_window =
      open_location == WindowOpenDisposition::NEW_WINDOW ||
      open_location == WindowOpenDisposition::OFF_THE_RECORD;
  if (opening_in_new_window && handle) {
    content::WebContents* opened_tab = handle->GetWebContents();
    auto* new_browser = chrome::FindBrowserWithWebContents(opened_tab);
    UserNotesController::ShowUserNotesForBrowser(new_browser);
  }
}
