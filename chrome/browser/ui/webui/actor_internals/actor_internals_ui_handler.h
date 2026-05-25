// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/aggregated_journal_file_serializer.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/internals/browser/actor_internals_handler.h"
#include "components/actor/public/mojom/actor_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/webui/mojo_web_ui_controller.h"

// Desktop UI Handler for chrome://actor-internals/
class ActorInternalsUIHandler
    : public actor::AggregatedJournal::Observer,
      public actor_internals::ActorInternalsHandler::Delegate,
      public ui::SelectFileDialog::Listener {
 public:
  ActorInternalsUIHandler(
      content::WebContents* web_contents,
      mojo::PendingRemote<actor_internals::mojom::Page> page,
      mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver);

  ActorInternalsUIHandler(const ActorInternalsUIHandler&) = delete;
  ActorInternalsUIHandler& operator=(const ActorInternalsUIHandler&) = delete;

  ~ActorInternalsUIHandler() override;

  // AggregatedJournalObserver implementation:
  void WillAddJournalEntry(
      const actor::AggregatedJournal::Entry& entry) override;

  // actor_internals::ActorInternalsHandler::Delegate:
  void StartLogging() override;
  void StopLogging() override;

  // SelectFileDialog::Listener implementation:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  void TraceFileInitDone(bool success);

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<actor::AggregatedJournalFileSerializer> trace_logger_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  std::unique_ptr<actor_internals::ActorInternalsHandler> handler_;

  base::WeakPtrFactory<ActorInternalsUIHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_HANDLER_H_
