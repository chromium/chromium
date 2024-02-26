// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/user_notes/interfaces/user_note_service_delegate.h"
#include "components/user_notes/interfaces/user_note_storage.h"
#include "components/user_notes/interfaces/user_notes_ui_delegate.h"
#include "components/user_notes/model/user_note.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

class UserNoteUICoordinatorTest;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace user_notes {

class FrameUserNoteChanges;
class UserNoteManager;
class UserNoteMetadataSnapshot;

// Keyed service coordinating the different parts (Renderer, UI layer, storage
// layer) of the User Notes feature for the current user profile.
class UserNoteService : public KeyedService,
                        public UserNotesUIDelegate,
                        public UserNoteStorage::Observer {
 public:
  using IdSet =
      std::unordered_set<base::UnguessableToken, base::UnguessableTokenHash>;

  UserNoteService(std::unique_ptr<UserNoteServiceDelegate> delegate,
                  std::unique_ptr<UserNoteStorage> storage);
  ~UserNoteService() override;
  UserNoteService(const UserNoteService&) = delete;
  UserNoteService& operator=(const UserNoteService&) = delete;

  base::SafeRef<UserNoteService> GetSafeRef() const;

  // Returns a pointer to the note model associated with the given ID, or
  // `nullptr` if none exists.
  const UserNote* GetNoteModel(const base::UnguessableToken& id) const;

  // Returns true if the provided ID is found in the creation map, indicating
  // the note is still in progress, and false otherwise.
  bool IsNoteInProgress(const base::UnguessableToken& id) const;

  // Called by the embedder when a frame navigates to a new URL. Queries the
  // storage to find notes associated with that URL, and if there are any, kicks
  // off the logic to display them in the page.
  virtual void OnFrameNavigated(content::RenderFrameHost* rfh);

  // Called by `UserNoteManager` objects when a `UserNoteInstance` is added to
  // the page they're attached to. Updates the model map to add a ref to the
  // given `UserNoteManager` for the note with the specified ID.
  void OnNoteInstanceAddedToPage(const base::UnguessableToken& id,
                                 UserNoteManager* manager);

  // Same as `OnNoteInstanceAddedToPage`, except for when a note is removed from
  // a page. Updates the model map to remove the ref to the given
  // `UserNoteManager`. If this is the last page where the note was displayed,
  // also deletes the model from the model map.
  void OnNoteInstanceRemovedFromPage(const base::UnguessableToken& id,
                                     UserNoteManager* manager);

  // Called by a note manager when the user selects "Add a note" from the
  // associated page's context menu. Kicks off the note creation process.
  void OnAddNoteRequested(content::RenderFrameHost* frame,
                          bool has_selected_text);

  // Called by a note manager when a user selects a web highlight in the page.
  // This causes the associated note to become focused in the UserNotesUI.
  void OnWebHighlightFocused(const base::UnguessableToken& id,
                             content::RenderFrameHost* rfh);

  // UserNotesUIDelegate implementation.
  void OnNoteSelected(const base::UnguessableToken& id,
                      content::RenderFrameHost* rfh) override;
  void OnNoteDeleted(const base::UnguessableToken& id) override;
  void OnNoteCreationDone(const base::UnguessableToken& id,
                          const std::u16string& note_content) override;
  void OnNoteCreationCancelled(const base::UnguessableToken& id) override;
  void OnNoteEdited(const base::UnguessableToken& id,
                    const std::u16string& note_content) override;

  // UserNoteStorage implementation
  void OnNotesChanged() override;

 private:
  struct ModelMapEntry {
    explicit ModelMapEntry(std::unique_ptr<UserNote> m);
    ~ModelMapEntry();
    ModelMapEntry(ModelMapEntry&& other);
    ModelMapEntry(const ModelMapEntry&) = delete;
    ModelMapEntry& operator=(const ModelMapEntry&) = delete;

    std::unique_ptr<UserNote> model;
    std::unordered_set<raw_ptr<UserNoteManager, CtnExperimental>> managers;
  };

  friend class MockUserNoteService;
  friend class UserNoteBaseTest;
  friend class UserNoteInstanceTest;
  friend class UserNoteUtilsTest;
  friend class ::UserNoteUICoordinatorTest;
  FRIEND_TEST_ALL_PREFIXES(UserNoteServiceTest,
                           OnNoteMetadataFetchedForNavigationSomeNotes);
  FRIEND_TEST_ALL_PREFIXES(
      UserNoteServiceTest,
      OnNoteMetadataFetchedForNavigationSomeNotesBackground);
  FRIEND_TEST_ALL_PREFIXES(UserNoteServiceTest,
                           OnNoteMetadataFetchedForNavigationNoNotes);
  FRIEND_TEST_ALL_PREFIXES(UserNoteServiceTest,
                           OnNoteMetadataFetchedForNavigationNoNotesBackground);
  FRIEND_TEST_ALL_PREFIXES(UserNoteServiceTest, OnNoteMetadataFetched);
  FRIEND_TEST_ALL_PREFIXES(UserNoteServiceTest, OnNoteModelsFetched);
  FRIEND_TEST_ALL_PREFIXES(UserNoteServiceTest, OnFrameChangesApplied);

  void InitializeNewNoteForCreation(
      content::WeakDocumentPtr document,
      bool is_page_level,
      mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
      mojo::PendingRemote<blink::mojom::AnnotationAgent> agent_remote,
      const std::string& serialized_selector,
      const std::u16string& selected_text);

  // Private helpers used when processing note storage changes. Marked virtual
  // for tests to override.
  virtual void OnNoteMetadataFetchedForNavigation(
      const std::vector<content::WeakDocumentPtr>& all_frames,
      UserNoteMetadataSnapshot metadata_snapshot);
  virtual void OnNoteMetadataFetched(
      const std::vector<content::WeakDocumentPtr>& all_frames,
      UserNoteMetadataSnapshot metadata_snapshot);
  virtual void OnNoteModelsFetched(
      const IdSet& new_notes,
      std::vector<std::unique_ptr<FrameUserNoteChanges>> note_changes,
      std::vector<std::unique_ptr<UserNote>> notes);
  virtual void OnFrameChangesApplied(base::UnguessableToken change_id);

  // Source of truth for the in-memory note models. Any note currently being
  // displayed in a tab is stored in this data structure. Each entry also
  // contains a set of pointers to all `UserNoteManager` objects holding an
  // instance of that note, which is necessary to clean up the models when
  // they're no longer in use and to remove notes from affected web pages when
  // they're deleted by the user.
  std::unordered_map<base::UnguessableToken,
                     ModelMapEntry,
                     base::UnguessableTokenHash>
      model_map_;

  // A map to store in-progress notes during the note creation process. This is
  // needed in order to separate incomplete notes from the rest of the real
  // note models. Completed notes are eventually moved to the model map.
  std::unordered_map<base::UnguessableToken,
                     ModelMapEntry,
                     base::UnguessableTokenHash>
      creation_map_;

  // A place to store the user note changes of a frame while they are being
  // applied.
  std::unordered_map<base::UnguessableToken,
                     std::unique_ptr<FrameUserNoteChanges>,
                     base::UnguessableTokenHash>
      note_changes_in_progress_;

  std::unique_ptr<UserNoteServiceDelegate> delegate_;
  std::unique_ptr<UserNoteStorage> storage_;
  base::WeakPtrFactory<UserNoteService> weak_ptr_factory_{this};
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_
