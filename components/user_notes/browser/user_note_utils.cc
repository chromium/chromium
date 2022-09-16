// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_utils.h"

#include "base/unguessable_token.h"
#include "components/user_notes/browser/frame_user_note_changes.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/browser/user_note_service.h"
#include "components/user_notes/interfaces/user_note_metadata_snapshot.h"
#include "components/user_notes/model/user_note_metadata.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "url/gurl.h"

namespace user_notes {

std::vector<std::unique_ptr<FrameUserNoteChanges>> CalculateNoteChanges(
    const UserNoteService& note_service,
    const std::vector<content::WeakDocumentPtr>& documents,
    const UserNoteMetadataSnapshot& metadata_snapshot) {
  std::vector<std::unique_ptr<FrameUserNoteChanges>> result;

  // Create an empty metadata map to simplify the algorithm below for cases
  // where there's no entry in the metadata snapshot for a frame's URL.
  UserNoteMetadataSnapshot::IdToMetadataMap empty_map;

  for (const content::WeakDocumentPtr& document : documents) {
    content::RenderFrameHost* rfh = document.AsRenderFrameHostIfValid();

    if (!rfh) {
      // The frame is no longer valid.
      continue;
    }

    // Notes should only be processed for the primary page.
    DCHECK(rfh->GetMainFrame()->IsInPrimaryMainFrame());

    UserNoteManager* notes_manager =
        UserNoteManager::GetForPage(rfh->GetPage());

    if (!notes_manager) {
      // Frame is part of an uninteresting page (for User Notes purposes), such
      // as the error document.
      continue;
    }

    const std::vector<UserNoteInstance*>& instances =
        notes_manager->GetAllNoteInstances();
    GURL url = rfh->GetLastCommittedURL();
    const UserNoteMetadataSnapshot::IdToMetadataMap* map_for_url =
        metadata_snapshot.GetMapForUrl(url);
    const UserNoteMetadataSnapshot::IdToMetadataMap* metadata_map =
        map_for_url ? map_for_url : &empty_map;

    std::vector<base::UnguessableToken> added;
    std::vector<base::UnguessableToken> modified;
    std::vector<base::UnguessableToken> removed;

    // First, iterate through the existing notes in the frame to detect notes
    // that have been removed and notes that have been modified.
    for (UserNoteInstance* instance : instances) {
      const UserNote& model = instance->model();
      const auto& metadata_it = metadata_map->find(model.id());

      if (metadata_it == metadata_map->end()) {
        // In-progress notes have an instance in the manager, but are not part
        // of the metadata snapshot because they haven't been persisted to disk
        // yet. Ignore them.
        if (note_service.IsNoteInProgress(model.id())) {
          continue;
        }

        if (url == model.target().target_page()) {
          // Note has been removed.
          removed.emplace_back(model.id());
        }
      } else if (metadata_it->second->modification_date() >
                     model.metadata().modification_date() ||
                 note_service.IsNoteInProgress(model.id())) {
        // This note has been modified or created locally (which is treated as a
        // modification since both the instance and the partial model exist).
        modified.emplace_back(model.id());
      }
    }

    // Finally, iterate through the notes in the metadata to detect notes that
    // have been added.
    for (const auto& metadata_it : *metadata_map) {
      const base::UnguessableToken& id = metadata_it.first;
      if (!notes_manager->GetNoteInstance(id)) {
        // This is a new note.
        added.emplace_back(id);
      }
    }

    if (!added.empty() || !removed.empty() || !modified.empty()) {
      result.emplace_back(std::make_unique<FrameUserNoteChanges>(
          note_service.GetSafeRef(), rfh->GetWeakDocumentPtr(),
          std::move(added), std::move(modified), std::move(removed)));
    }
  }

  return result;
}

}  // namespace user_notes
