// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_page_handler.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/power_bookmarks/core/powers/power.h"
#include "components/power_bookmarks/core/powers/power_overview.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "ui/base/l10n/time_format.h"

namespace {

const int kCurrentVersionNumber = 1;

side_panel::mojom::NoteOverviewPtr PowerOverviewToMojo(
    const power_bookmarks::PowerOverview& power_overview) {
  auto* power = power_overview.power();
  DCHECK(power->power_type() ==
         sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE);
  DCHECK(power->power_entity()->has_note_entity());
  auto result = side_panel::mojom::NoteOverview::New();
  result->url = power->url();
  // TODO(crbug.com/1378131): Get title from the corresponding bookmark.
  result->title = power->url().spec();
  result->text = power->power_entity()->note_entity().plain_text();
  result->num_notes = power_overview.count();
  result->is_current_tab = false;
  // TODO(crbug.com/1378131): Get the last_modification_time of the overview
  // item for sorting.
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
  result->set_guid(base::GUID::ParseLowercase(guid));
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
    Profile* profile,
    UserNotesSidePanelUI* user_notes_ui)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      service_(PowerBookmarkServiceFactory::GetForBrowserContext(profile_)),
      user_notes_ui_(user_notes_ui) {}

UserNotesPageHandler::~UserNotesPageHandler() = default;

void UserNotesPageHandler::ShowUI() {
  auto embedder = user_notes_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void UserNotesPageHandler::GetNoteOverviews(const std::string& user_input,
                                            GetNoteOverviewsCallback callback) {
  service_->GetPowerOverviewsForType(
      sync_pb::PowerBookmarkSpecifics::POWER_TYPE_NOTE,
      base::BindOnce(
          [](GetNoteOverviewsCallback callback,
             std::vector<std::unique_ptr<power_bookmarks::PowerOverview>>
                 power_overviews) {
            std::vector<side_panel::mojom::NoteOverviewPtr> results;
            for (auto& power_overview : power_overviews) {
              results.push_back(PowerOverviewToMojo(*power_overview));
            }
            std::move(callback).Run(std::move(results));
          },
          std::move(callback)));
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
  std::string guid = base::GUID::GenerateRandomV4().AsLowercaseString();
  service_->CreatePower(
      MakePower(guid, text, current_tab_url_, /*is_create=*/true),
      base::BindOnce([](NewNoteFinishedCallback callback,
                        bool success) { std::move(callback).Run(success); },
                     std::move(callback)));
}

void UserNotesPageHandler::UpdateNote(const std::string& guid,
                                      const std::string& text,
                                      UpdateNoteCallback callback) {
  service_->UpdatePower(
      MakePower(guid, text, current_tab_url_, /*is_create=*/false),
      base::BindOnce([](UpdateNoteCallback callback,
                        bool success) { std::move(callback).Run(success); },
                     std::move(callback)));
}

void UserNotesPageHandler::DeleteNote(const std::string& guid,
                                      DeleteNoteCallback callback) {
  service_->DeletePower(
      base::GUID::ParseLowercase(guid),
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
