// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/actor_internals/actor_internals_ui_handler.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {
std::string ToString(actor::mojom::JournalEntryType type) {
  switch (type) {
    case actor::mojom::JournalEntryType::kBegin:
      return "B";
    case actor::mojom::JournalEntryType::kEnd:
      return "E";
    case actor::mojom::JournalEntryType::kInstant:
      return "I";
  }
  NOTREACHED();
}
}  // namespace

ActorInternalsUIHandler::ActorInternalsUIHandler(
    content::WebContents* web_contents,
    mojo::PendingRemote<actor_internals::mojom::Page> page,
    mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver)
    : web_contents_(web_contents),
      remote_(std::move(page)),
      receiver_(this, std::move(receiver)) {
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
  remote_->JournalEntryAdded(actor_internals::mojom::JournalEntry::New(
      entry.url, entry.data->event, ToString(entry.data->type),
      entry.data->details, entry.data->timestamp));
}

void ActorInternalsUIHandler::StartLogging() {
  if (select_file_dialog_) {
    return;  // Currently running, wait for existing save to complete.
  }

  base::FilePath default_file =
      base::FilePath().AppendASCII("actor_trace.pftrace");
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
