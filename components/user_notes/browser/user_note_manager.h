// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_MANAGER_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/safe_ref.h"
#include "base/unguessable_token.h"
#include "components/user_notes/browser/user_note_instance.h"
#include "components/user_notes/browser/user_note_service.h"
#include "content/public/browser/page_user_data.h"

namespace content {
class Page;
}

namespace user_notes {

// A class responsible for holding the note instances that appear on a specific
// |Page|. Its lifecycle is tied to the |Page| it is associated with, so it
// implements |PageUserData|. A tab helper is responsible for attaching an
// instance of this class to each new |Page|.
class UserNoteManager : public content::PageUserData<UserNoteManager> {
 public:
  ~UserNoteManager() override;
  UserNoteManager(const UserNoteManager&) = delete;
  UserNoteManager& operator=(const UserNoteManager&) = delete;

  // Returns the note instance for the given ID, or nullptr if this page does
  // not have an instance of that note.
  UserNoteInstance* GetNoteInstance(const base::UnguessableToken& id);

  // Returns all note instances for the |Page| this object is attached to.
  const std::vector<UserNoteInstance*> GetAllNoteInstances();

  // Destroys the note instance associated with the given GUID.
  void RemoveNote(const base::UnguessableToken& id);

  // Stores the given note instance into this object's instance map, then kicks
  // off its asynchronous initialization in the renderer process, passing it the
  // provided callback for when it finishes.
  // TODO(gujen): Remove the overload without the callback after tests are
  //              fixed.
  void AddNoteInstance(std::unique_ptr<UserNoteInstance> note);
  void AddNoteInstance(std::unique_ptr<UserNoteInstance> note,
                       base::OnceClosure initialize_callback);

 private:
  friend class content::PageUserData<UserNoteManager>;
  friend class UserNoteBaseTest;
  friend class UserNoteUtilsTest;

  UserNoteManager(content::Page& page, base::SafeRef<UserNoteService> service);

  PAGE_USER_DATA_KEY_DECL();

  // A ref to the note service. The service is always expected to outlive this
  // class.
  base::SafeRef<UserNoteService> service_;

  // The list of note instances displayed in this page, mapped by their note ID.
  // TODO(crbug.com/1313967): Holding the instances in an ID -> Instance map
  // works while only top-level frames are supported, but won't always work if
  // subframes are supported. For example, if website A has notes and website B
  // embeds website A multiple times via iframes, then there will be multiple
  // note instances in this manager for the same note ID, which this data
  // structure can't handle. In that case the data structure will probably need
  // to be something like ID -> Frame -> Instance.
  std::unordered_map<base::UnguessableToken,
                     std::unique_ptr<UserNoteInstance>,
                     base::UnguessableTokenHash>
      instance_map_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_MANAGER_H_
