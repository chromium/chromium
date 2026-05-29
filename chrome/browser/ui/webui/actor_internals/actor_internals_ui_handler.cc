// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/actor_internals/actor_internals_ui_handler.h"

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"



ActorInternalsUIHandler::ActorInternalsUIHandler(
    content::WebContents* web_contents,
    mojo::PendingRemote<actor_internals::mojom::Page> page,
    mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver)
    : web_contents_(web_contents) {
  handler_ = std::make_unique<actor_internals::ActorInternalsHandler>(
      std::move(page), std::move(receiver), this);

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto& journal = actor::ActorKeyedService::Get(profile)->GetJournal();
  journal.AddObserver(this);

  for (auto it = journal.Items(); it; ++it) {
    WillAddJournalEntry(***it);
  }
}

ActorInternalsUIHandler::~ActorInternalsUIHandler() {
  auto* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto& journal = actor::ActorKeyedService::Get(profile)->GetJournal();
  journal.RemoveObserver(this);
}

void ActorInternalsUIHandler::WillAddJournalEntry(
    const actor::AggregatedJournal::Entry& entry) {
  base::flat_map<std::string, std::string> details;
  for (const auto& detail : entry.data->details) {
    details[detail->key] = detail->value;
  }

  handler_->OnJournalEntryAdded(actor_internals::mojom::JournalEntry::New(
      entry.url, entry.data->event,
      std::string(actor::JournalEntryTypeToString(entry.data->type)),
      std::move(details), entry.data->timestamp, entry.data->task_id.value(),
      actor::TrackToString(entry.data->track_uuid, entry.data->task_id),
      entry.screenshot));
}

void ActorInternalsUIHandler::StartLogging() {
  if (select_file_dialog_) {
    return;  // Currently running, wait for existing save to complete.
  }

  PrefService* local_state = g_browser_process->local_state();
  if (local_state->FindPreference(prefs::kAllowFileSelectionDialogs) &&
      !local_state->GetBoolean(prefs::kAllowFileSelectionDialogs)) {
    return;
  }

  base::FilePath default_file;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_file);
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  default_file = default_file.AppendASCII(
      absl::StrFormat("actor_trace_%.2d_%.2d_%.2d_%.2d_%.2d_%.2d.pftrace",
                      exploded.year, exploded.month, exploded.day_of_month,
                      exploded.hour, exploded.minute, exploded.second));
  select_file_dialog_ = ui::SelectFileDialog::Create(this, /*policy=*/nullptr);
  select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                  std::u16string(), default_file, nullptr, 0,
                                  FILE_PATH_LITERAL(".pftrace"),
                                  web_contents_->GetTopLevelNativeWindow());
}

void ActorInternalsUIHandler::StopLogging() {
  select_file_dialog_.reset();
  trace_logger_.reset();
}

void ActorInternalsUIHandler::FileSelected(const ui::SelectedFileInfo& file,
                                           int index) {
  auto* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto& journal = actor::ActorKeyedService::Get(profile)->GetJournal();
  trace_logger_ =
      std::make_unique<actor::AggregatedJournalFileSerializer>(journal);

  trace_logger_->Init(
      file.path(), base::BindOnce(&ActorInternalsUIHandler::TraceFileInitDone,
                                  weak_ptr_factory_.GetWeakPtr()));
  select_file_dialog_.reset();
}

void ActorInternalsUIHandler::TraceFileInitDone(bool success) {
  if (!success) {
    trace_logger_.reset();
  }
}

void ActorInternalsUIHandler::FileSelectionCanceled() {
  select_file_dialog_.reset();
  trace_logger_.reset();
}
