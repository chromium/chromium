// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/user_notes/interfaces/user_notes_ui_delegate.h"
#include "components/user_notes/model/user_note.h"

namespace user_notes {

class UserNotesManager;

// Keyed service coordinating the different parts (Renderer, UI layer, storage
// layer) of the User Notes feature for the current user profile.
class UserNoteService : public KeyedService, public UserNotesUIDelegate {
 public:
  explicit UserNoteService();
  ~UserNoteService() override;
  UserNoteService(const UserNoteService&) = delete;
  UserNoteService& operator=(const UserNoteService&) = delete;

  base::SafeRef<UserNoteService> GetSafeRef();

  // Called by |UserNotesManager| objects when a |UserNoteInstance| is added to
  // the page they're attached to. Updates the model map to add a ref to the
  // given |UserNotesManager| for the note with the specified GUID.
  void OnNoteInstanceAddedToPage(const std::string& guid,
                                 UserNotesManager* manager);

  // Same as |OnNoteInstanceAddedToPage|, except for when a note is removed from
  // a page. Updates the model map to remove the ref to the given
  // |UserNotesManager|. If this is the last page where the note was displayed,
  // also deletes the model from the model map.
  void OnNoteInstanceRemovedFromPage(const std::string& guid,
                                     UserNotesManager* manager);

  // UserNotesUIDelegate implementation.
  void OnNoteFocused(const std::string& guid) override;
  void OnNoteCreationDone(const std::string& guid,
                          const std::string& note_content) override;
  void OnNoteCreationCancelled(const std::string& guid) override;

 private:
  struct ModelMapEntry {
    explicit ModelMapEntry(std::unique_ptr<UserNote> m);
    ~ModelMapEntry();
    ModelMapEntry(const ModelMapEntry&) = delete;
    ModelMapEntry(ModelMapEntry&& other);
    ModelMapEntry& operator=(const ModelMapEntry&) = delete;

    std::unique_ptr<UserNote> model;
    std::unordered_set<UserNotesManager*> managers;
  };

  friend class UserNoteServiceTest;
  FRIEND_TEST_ALL_PREFIXES(UserNoteServiceTest, UserNoteServiceTest);

  // Source of truth for the in-memory note models. Any note currently being
  // displayed in a tab is stored in this data structure. Each entry also
  // contains a set of pointers to all |UserNotesManager| objects holding an
  // instance of that note, which is necessary to clean up the models when
  // they're no longer in use and to remove notes from affected web pages when
  // they're deleted by the user.
  std::unordered_map<std::string, ModelMapEntry> model_map_;

  base::WeakPtrFactory<UserNoteService> weak_ptr_factory_{this};
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_
