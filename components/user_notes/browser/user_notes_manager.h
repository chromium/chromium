// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTES_MANAGER_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTES_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/user_notes/browser/user_note_service.h"
#include "content/public/browser/page_user_data.h"

namespace content {
class Page;
}

namespace user_notes {

class UserNoteInstance;

// A class responsible for holding the note instances that appear on a specific
// |Page|. Its lifecycle is tied to the |Page| it is associated with, so it
// implements |PageUserData|. A tab helper is responsible for attaching an
// instance of this class to each new |Page|.
class UserNotesManager : public content::PageUserData<UserNotesManager> {
 public:
  ~UserNotesManager() override;
  UserNotesManager(const UserNotesManager&) = delete;
  UserNotesManager& operator=(const UserNotesManager&) = delete;

  // Returns the note instance for the given GUID, or nullptr if this page does
  // not have an instance of that note.
  UserNoteInstance* GetNoteInstance(const std::string& guid);

  // Returns all note instances for the |Page| this object is attached to.
  const std::vector<UserNoteInstance*> GetAllNoteInstances();

  // Destroys the note instance associated with the given GUID.
  void RemoveNote(const std::string& guid);

  // Stores the given note instance into this object's note instance container.
  void AddNoteInstance(std::unique_ptr<UserNoteInstance> note);

 private:
  friend class content::PageUserData<UserNotesManager>;

  UserNotesManager(content::Page& page, base::WeakPtr<UserNoteService> service);

  PAGE_USER_DATA_KEY_DECL();

  // A weak pointer to the note service. The service is generally expected to
  // outlive this class, but due to destruction order on browser shut down,
  // using a weak pointer here is safer.
  base::WeakPtr<UserNoteService> service_;

  // The list of note instances displayed in this page, mapped by their note ID.
  std::unordered_map<std::string, std::unique_ptr<UserNoteInstance>>
      instance_map_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTES_MANAGER_H_
