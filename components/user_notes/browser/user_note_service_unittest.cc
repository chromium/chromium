// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_service.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/unguessable_token.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "components/user_notes/browser/frame_user_note_changes.h"
#include "components/user_notes/browser/user_note_base_test.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/interfaces/user_note_service_delegate.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "components/user_notes/model/user_note_metadata.h"
#include "components/user_notes/model/user_note_model_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;
using testing::Mock;

namespace user_notes {

using IdList = std::vector<base::UnguessableToken>;

class MockUserNoteServiceDelegate : public UserNoteServiceDelegate {
 public:
  MOCK_METHOD(std::vector<content::RenderFrameHost*>,
              GetAllFramesForUserNotes,
              (),
              (override));
  MOCK_METHOD(UserNotesUI*,
              GetUICoordinatorForFrame,
              (const content::RenderFrameHost* rfh),
              (override));
  MOCK_METHOD(bool,
              IsFrameInActiveTab,
              (const content::RenderFrameHost* rfh),
              (override));

  void SetFramesForUserNotes(
      const std::vector<content::RenderFrameHost*>& frames) {
    frames_ = frames;
  }

  std::vector<content::RenderFrameHost*> MockGetAllFramesForUserNotes() {
    return frames_;
  }

 private:
  std::vector<content::RenderFrameHost*> frames_;
};

class MockUserNoteStorage : public UserNoteStorage {
 public:
  MOCK_METHOD(void,
              GetNoteMetadataForUrls,
              (const UserNoteStorage::UrlSet& urls,
               base::OnceCallback<void(UserNoteMetadataSnapshot)> callback),
              (override));

  MOCK_METHOD(void,
              GetNotesById,
              (const UserNoteStorage::IdSet& ids,
               base::OnceCallback<void(std::vector<std::unique_ptr<UserNote>>)>
                   callback),
              (override));

  // The following must be mocked even though they're not used in the tests,
  // because they're abstract.
  MOCK_METHOD(void,
              UpdateNote,
              (const UserNote* model,
               std::u16string note_body_text,
               bool is_creation),
              (override));

  MOCK_METHOD(void,
              DeleteNote,
              (const base::UnguessableToken& guid),
              (override));

  MOCK_METHOD(void, DeleteAllForUrl, (const GURL& url), (override));

  MOCK_METHOD(void,
              DeleteAllForOrigin,
              (const url::Origin& origin),
              (override));

  MOCK_METHOD(void, DeleteAllNotes, (), (override));

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));

  const UserNoteStorage::UrlSet& requested_metadata_urls() {
    return requested_metadata_urls_;
  }
  const UserNoteStorage::IdSet& requested_model_ids() {
    return requested_model_ids_;
  }

  void MockGetNoteMetadataForUrls(
      const UserNoteStorage::UrlSet& urls,
      base::OnceCallback<void(UserNoteMetadataSnapshot)> callback) {
    requested_metadata_urls_ = urls;
    std::move(callback).Run(UserNoteMetadataSnapshot());
  }

  void MockGetNotesById(
      const UserNoteStorage::IdSet& ids,
      base::OnceCallback<void(std::vector<std::unique_ptr<UserNote>>)>
          callback) {
    requested_model_ids_ = ids;
    std::move(callback).Run({});
  }

 private:
  UserNoteStorage::UrlSet requested_metadata_urls_;
  UserNoteStorage::IdSet requested_model_ids_;
};

// Partially mock the object under test so tests can control side effects.
class MockUserNoteService : public UserNoteService {
 public:
  MockUserNoteService(std::unique_ptr<UserNoteServiceDelegate> delegate,
                      std::unique_ptr<UserNoteStorage> storage)
      : UserNoteService(std::move(delegate), std::move(storage)) {}

  const UserNoteService::IdSet& computed_new_notes() {
    return computed_new_notes_;
  }

  const IdList& changes_applied() { return changes_applied_; }

  MOCK_METHOD(void,
              OnNoteMetadataFetchedForNavigation,
              (const std::vector<content::WeakDocumentPtr>& all_frames,
               UserNoteMetadataSnapshot metadata_snapshot),
              (override));
  MOCK_METHOD(void,
              OnNoteMetadataFetched,
              (const std::vector<content::WeakDocumentPtr>& all_frames,
               UserNoteMetadataSnapshot metadata_snapshot),
              (override));
  MOCK_METHOD(void,
              OnNoteModelsFetched,
              (const UserNoteService::IdSet& new_notes,
               std::vector<std::unique_ptr<FrameUserNoteChanges>> note_changes,
               std::vector<std::unique_ptr<UserNote>> notes),
              (override));
  MOCK_METHOD(void,
              OnFrameChangesApplied,
              (base::UnguessableToken change_id),
              (override));

  void MockOnNoteModelsFetched(
      const UserNoteService::IdSet& new_notes,
      std::vector<std::unique_ptr<FrameUserNoteChanges>> note_changes,
      std::vector<std::unique_ptr<UserNote>> notes) {
    computed_new_notes_ = new_notes;
  }

  void MockOnFrameChangesApplied(base::UnguessableToken change_id) {
    changes_applied_.emplace_back(change_id);
  }

  void CallBaseClassOnNoteMetadataFetchedForNavigation(
      const std::vector<content::WeakDocumentPtr>& all_frames,
      UserNoteMetadataSnapshot metadata_snapshot) {
    UserNoteService::OnNoteMetadataFetchedForNavigation(
        all_frames, std::move(metadata_snapshot));
  }

  void CallBaseClassOnNoteMetadataFetched(
      const std::vector<content::WeakDocumentPtr>& all_frames,
      UserNoteMetadataSnapshot metadata_snapshot) {
    UserNoteService::OnNoteMetadataFetched(all_frames,
                                           std::move(metadata_snapshot));
  }

  void CallBaseClassOnNoteModelsFetched(
      const UserNoteService::IdSet& new_notes,
      std::vector<std::unique_ptr<FrameUserNoteChanges>> note_changes,
      std::vector<std::unique_ptr<UserNote>> notes) {
    UserNoteService::OnNoteModelsFetched(new_notes, std::move(note_changes),
                                         std::move(notes));
  }

  void CallBaseClassOnFrameChangesApplied(base::UnguessableToken change_id) {
    UserNoteService::OnFrameChangesApplied(change_id);
  }

 private:
  UserNoteService::IdSet computed_new_notes_;
  IdList changes_applied_;
};

class MockUserNoteInstance : public UserNoteInstance {
 public:
  explicit MockUserNoteInstance(base::SafeRef<UserNote> model_ref,
                                UserNoteManager* manager)
      : UserNoteInstance(model_ref, manager) {}

  void InitializeHighlightInternal() override {
    DidFinishAttachment(gfx::Rect());
  }
};

class MockFrameUserNoteChanges : public FrameUserNoteChanges {
 public:
  MockFrameUserNoteChanges(base::SafeRef<UserNoteService> service,
                           content::WeakDocumentPtr document,
                           const ChangeList& notes_added,
                           const ChangeList& notes_modified,
                           const ChangeList& notes_removed)
      : FrameUserNoteChanges(service,
                             document,
                             notes_added,
                             notes_modified,
                             notes_removed) {}

 private:
  std::unique_ptr<UserNoteInstance> MakeNoteInstance(
      const UserNote* note_model,
      UserNoteManager* manager) const override {
    return std::make_unique<MockUserNoteInstance>(note_model->GetSafeRef(),
                                                  manager);
  }
};

class MockUserNotesUI : public UserNotesUI {
 public:
  MOCK_METHOD(void, InvalidateIfVisible, (), (override));

  // The following methods are not used for these tests but they still need to
  // be mocked because they are sbstract.
  MOCK_METHOD(void,
              FocusNote,
              (const base::UnguessableToken& guid),
              (override));
  MOCK_METHOD(void,
              StartNoteCreation,
              (UserNoteInstance * instance),
              (override));
  MOCK_METHOD(void, Show, (), (override));
};

class UserNoteServiceTest : public UserNoteBaseTest {
 protected:
  void SetUp() override {
    UserNoteBaseTest::SetUp();
    AddNewNotesToService(2);
  }

  void CreateService() override {
    auto service_delegate = std::make_unique<MockUserNoteServiceDelegate>();
    service_delegate_ = service_delegate.get();

    auto storage = std::make_unique<MockUserNoteStorage>();
    EXPECT_CALL(*storage, UpdateNote).Times(0);
    EXPECT_CALL(*storage, DeleteNote).Times(0);
    EXPECT_CALL(*storage, DeleteAllForUrl).Times(0);
    EXPECT_CALL(*storage, DeleteAllForOrigin).Times(0);
    EXPECT_CALL(*storage, DeleteAllNotes).Times(0);
    storage_ = storage.get();

    note_service_ = std::make_unique<MockUserNoteService>(
        std::move(service_delegate), std::move(storage));
    mock_service_ = (MockUserNoteService*)note_service_.get();
  }

  std::vector<content::RenderFrameHost*> GetAllFramesInUse() {
    std::vector<content::RenderFrameHost*> frames;
    for (const std::unique_ptr<content::WebContents>& wc : web_contents_list_) {
      frames.emplace_back(wc->GetPrimaryMainFrame());
    }
    return frames;
  }

  std::vector<content::WeakDocumentPtr> GetAllFramesInUseAsWeakPtr() {
    std::vector<content::WeakDocumentPtr> weak_documents;
    for (content::RenderFrameHost* rfh : GetAllFramesInUse()) {
      weak_documents.emplace_back(rfh->GetWeakDocumentPtr());
    }
    return weak_documents;
  }

  raw_ptr<MockUserNoteService> mock_service_;
  raw_ptr<MockUserNoteServiceDelegate> service_delegate_;
  raw_ptr<MockUserNoteStorage> storage_;
};

// Tests that note models are returned correctly by the service.
TEST_F(UserNoteServiceTest, GetNoteModel) {
  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 0u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 0u);

  // Getting existing note models should return the expected model.
  const UserNote* model1 = note_service_->GetNoteModel(note_ids_[0]);
  const UserNote* model2 = note_service_->GetNoteModel(note_ids_[1]);
  ASSERT_TRUE(model1);
  ASSERT_TRUE(model2);
  EXPECT_EQ(model1->id(), note_ids_[0]);
  EXPECT_EQ(model2->id(), note_ids_[1]);

  // Getting a note model that doesn't exist should return `nullptr` and not
  // crash.
  EXPECT_EQ(note_service_->GetNoteModel(base::UnguessableToken::Create()),
            nullptr);
}

// Tests that references to note managers are correctly added to the model map.
TEST_F(UserNoteServiceTest, OnNoteInstanceAddedToPage) {
  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 0u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 0u);

  // Simulate note instances being created in managers.
  UserNoteManager* m1 = ConfigureNewManager();
  UserNoteManager* m2 = ConfigureNewManager();
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m1);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m2);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], m1);

  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
}

// Tests that references to note managers are correctly removed from the model
// map.
TEST_F(UserNoteServiceTest, OnNoteInstanceRemovedFromPage) {
  // Initial setup.
  UserNoteManager* m1 = ConfigureNewManager();
  UserNoteManager* m2 = ConfigureNewManager();
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m1);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m2);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], m1);

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);

  // Simulate a note instance being removed from a page. Its ref should be
  // removed from the model map, but only for the removed note.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[0], m1);
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);

  // Simulate the last instance of a note being removed from its page. Its model
  // should be cleaned up from the model map.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[0], m2);
  EXPECT_EQ(ModelMapSize(), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_FALSE(DoesModelExist(note_ids_[0]));

  // Repeat with the other note instance.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[1], m1);
  EXPECT_EQ(ModelMapSize(), 0u);
  EXPECT_FALSE(DoesModelExist(note_ids_[0]));
  EXPECT_FALSE(DoesModelExist(note_ids_[1]));
}

// Tests that partial notes are correctly identified as such.
TEST_F(UserNoteServiceTest, IsNoteInProgress) {
  EXPECT_EQ(CreationMapSize(), 0u);
  AddPartialNotesToService(2);
  EXPECT_EQ(CreationMapSize(), 2u);

  EXPECT_FALSE(note_service_->IsNoteInProgress(note_ids_[0]));
  EXPECT_FALSE(note_service_->IsNoteInProgress(note_ids_[1]));
  EXPECT_TRUE(note_service_->IsNoteInProgress(note_ids_[2]));
  EXPECT_TRUE(note_service_->IsNoteInProgress(note_ids_[3]));

  // The method should also return false for notes that don't exist.
  EXPECT_FALSE(
      note_service_->IsNoteInProgress(base::UnguessableToken::Create()));
}

// Tests that adding an instance of a partial note to a page does not impact
// the model map and the note manager references.
TEST_F(UserNoteServiceTest, AddPartialNoteInstance) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], manager);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], manager);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);

  // Create an in-progress note.
  EXPECT_EQ(CreationMapSize(), 0u);
  AddPartialNotesToService(1);
  EXPECT_EQ(CreationMapSize(), 1u);

  // Simulate the instance of the in-progress note being added to the note
  // manager.
  note_service_->OnNoteInstanceAddedToPage(note_ids_[2], manager);

  // Verify the model map hasn't been impacted and that the creation map is
  // still as expected.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_TRUE(note_service_->IsNoteInProgress(note_ids_[2]));
}

// Tests that removing an instance of a partial note from a page does not impact
// the model map and the note manager references, and correctly clears the
// partial note from the creation map.
TEST_F(UserNoteServiceTest, RemovePartialNoteInstance) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], manager);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], manager);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);

  // Create an in-progress note.
  EXPECT_EQ(CreationMapSize(), 0u);
  AddPartialNotesToService(1);
  EXPECT_EQ(CreationMapSize(), 1u);

  // Simulate the instance of the in-progress note being removed from the note
  // manager.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[2], manager);

  // Verify the model map hasn't been impacted and the partial note has been
  // removed from the creation map.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(CreationMapSize(), 0u);
  EXPECT_FALSE(note_service_->IsNoteInProgress(note_ids_[2]));
}

// Tests that the service requests the metadata snapshot for the right URLs when
// it gets notified that notes have changed on disk.
TEST_F(UserNoteServiceTest, OnNotesChanged) {
  // Initial setup.
  AddNewNotesToService(2);
  UserNoteManager* manager1 = ConfigureNewManager();
  UserNoteManager* manager2 = ConfigureNewManager();
  AddNewInstanceToManager(manager1, note_ids_[0]);
  AddNewInstanceToManager(manager1, note_ids_[1]);
  AddNewInstanceToManager(manager2, note_ids_[2]);
  AddNewInstanceToManager(manager2, note_ids_[3]);
  service_delegate_->SetFramesForUserNotes(GetAllFramesInUse());

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 4u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[3]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[1], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[2], manager2));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[3], manager2));

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes)
      .Times(1)
      .WillOnce(
          Invoke(service_delegate_.get(),
                 &MockUserNoteServiceDelegate::MockGetAllFramesForUserNotes));
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame).Times(0);
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab).Times(0);

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls)
      .Times(1)
      .WillOnce(Invoke(storage_.get(),
                       &MockUserNoteStorage::MockGetNoteMetadataForUrls));
  EXPECT_CALL(*storage_, GetNotesById).Times(0);

  // Configure service mock.
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched).Times(1);
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied).Times(0);

  // Simulate notes changing on disk.
  note_service_->OnNotesChanged();

  // Mocks ensure callbacks are invoked synchronously, so expectations can be
  // immediately verified.
  const UserNoteStorage::UrlSet& fetched_urls =
      storage_->requested_metadata_urls();
  EXPECT_EQ(fetched_urls.size(), web_contents_list_.size());
  for (size_t i = 0; i < fetched_urls.size(); ++i) {
    const auto& url_it = fetched_urls.find(
        web_contents_list_[i]->GetPrimaryMainFrame()->GetLastCommittedURL());
    EXPECT_NE(url_it, fetched_urls.end());
  }
}

// Tests that the service correctly fetches note metadata for the navigated
// frame when a navigation occurs.
TEST_F(UserNoteServiceTest, OnFrameNavigated) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  AddNewInstanceToManager(manager, note_ids_[0]);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager));

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes).Times(0);
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame(_)).Times(0);
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab(_)).Times(0);

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls)
      .Times(1)
      .WillOnce(Invoke(storage_.get(),
                       &MockUserNoteStorage::MockGetNoteMetadataForUrls));
  EXPECT_CALL(*storage_, GetNotesById).Times(0);

  // Configure service mock.
  std::vector<content::WeakDocumentPtr> all_frames_result;
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation)
      .Times(1)
      .WillOnce([&](const std::vector<content::WeakDocumentPtr>& all_frames,
                    UserNoteMetadataSnapshot metadata_snapshot) {
        all_frames_result.assign(all_frames.begin(), all_frames.end());
      });
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied).Times(0);

  // Pretend there was a navigation.
  content::RenderFrameHost* frame =
      web_contents_list_[0]->GetPrimaryMainFrame();
  note_service_->OnFrameNavigated(frame);

  // Mocks ensure callbacks are invoked synchronously, so expectations can be
  // immediately verified.
  ASSERT_EQ(all_frames_result.size(), 1u);
  content::RenderFrameHost* rfh =
      all_frames_result[0].AsRenderFrameHostIfValid();
  EXPECT_NE(rfh, nullptr);
  EXPECT_EQ(rfh, frame);

  const UserNoteStorage::UrlSet& requested_urls =
      storage_->requested_metadata_urls();
  ASSERT_EQ(requested_urls.size(), 1u);
  EXPECT_EQ(*(requested_urls.begin()), frame->GetLastCommittedURL());
}

// After a navigation to a document that has user notes in the foreground, the
// service should request the notes UI to show itself and fetch the notes
// metadata.
// TODO(crbug.com/40832588): This test will need to be changed when notes UI is
// no longer automatically shown on navigation.
TEST_F(UserNoteServiceTest, OnNoteMetadataFetchedForNavigationSomeNotes) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  AddNewInstanceToManager(manager, note_ids_[0]);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager));

  // Configure UI mock.
  auto mock_ui = std::make_unique<MockUserNotesUI>();
  EXPECT_CALL(*mock_ui, InvalidateIfVisible).Times(1);
  EXPECT_CALL(*mock_ui, FocusNote).Times(0);
  EXPECT_CALL(*mock_ui, StartNoteCreation).Times(0);
  EXPECT_CALL(*mock_ui, Show).Times(1);

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes).Times(0);
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame(_))
      .Times(1)
      .WillOnce(Invoke([&mock_ui](const content::RenderFrameHost* frame) {
        return mock_ui.get();
      }));
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab(_))
      .Times(1)
      .WillOnce(
          Invoke([](const content::RenderFrameHost* frame) { return true; }));

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls).Times(0);
  EXPECT_CALL(*storage_, GetNotesById).Times(0);

  // Configure service mock.
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation)
      .Times(1)
      .WillOnce(Invoke(mock_service_.get(),
                       &MockUserNoteService::
                           CallBaseClassOnNoteMetadataFetchedForNavigation));
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched).Times(1);
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied).Times(0);

  // Create a non-empty metadata snapshot.
  UserNoteMetadataSnapshot snapshot;
  GURL url =
      web_contents_list_[0]->GetPrimaryMainFrame()->GetLastCommittedURL();
  snapshot.AddEntry(
      url, note_ids_[0],
      std::make_unique<UserNoteMetadata>(base::Time::Now(), base::Time::Now(),
                                         /*min_note_version=*/1));

  // Simulate the service receiving the metadata snapshot after a navigation.
  note_service_->OnNoteMetadataFetchedForNavigation(
      GetAllFramesInUseAsWeakPtr(), std::move(snapshot));
}

// After a navigation to a document that has user notes, but isn't in the
// foreground, the service should not request the notes UI to show itself.
// TODO(crbug.com/40832588): This test will need to be changed when notes UI is
// no longer automatically shown on navigation.
TEST_F(UserNoteServiceTest,
       OnNoteMetadataFetchedForNavigationSomeNotesBackground) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  AddNewInstanceToManager(manager, note_ids_[0]);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager));

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes).Times(0);
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame(_)).Times(0);
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab(_))
      .Times(1)
      .WillOnce(
          Invoke([](const content::RenderFrameHost* frame) { return false; }));

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls).Times(0);
  EXPECT_CALL(*storage_, GetNotesById).Times(0);

  // Configure service mock.
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation)
      .Times(1)
      .WillOnce(Invoke(mock_service_.get(),
                       &MockUserNoteService::
                           CallBaseClassOnNoteMetadataFetchedForNavigation));
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched).Times(1);
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied).Times(0);

  // Create a non-empty metadata snapshot.
  UserNoteMetadataSnapshot snapshot;
  GURL url =
      web_contents_list_[0]->GetPrimaryMainFrame()->GetLastCommittedURL();
  snapshot.AddEntry(
      url, note_ids_[0],
      std::make_unique<UserNoteMetadata>(base::Time::Now(), base::Time::Now(),
                                         /*min_note_version=*/1));

  // Simulate the service receiving the metadata snapshot after a navigation.
  note_service_->OnNoteMetadataFetchedForNavigation(
      GetAllFramesInUseAsWeakPtr(), std::move(snapshot));
}

// After a navigation to a document that doesn't have user notes but is in the
// active tab, the service should not request the notes UI to show itself, but
// should Invalidate the notes displayed in the UI.
// TODO(crbug.com/40832588): This test will need to be changed when notes UI is
// no longer automatically shown on navigation.
TEST_F(UserNoteServiceTest, OnNoteMetadataFetchedForNavigationNoNotes) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  AddNewInstanceToManager(manager, note_ids_[0]);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager));

  // Configure UI mock.
  auto mock_ui = std::make_unique<MockUserNotesUI>();
  EXPECT_CALL(*mock_ui, InvalidateIfVisible).Times(1);
  EXPECT_CALL(*mock_ui, FocusNote).Times(0);
  EXPECT_CALL(*mock_ui, StartNoteCreation).Times(0);
  EXPECT_CALL(*mock_ui, Show).Times(0);

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes).Times(0);
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame(_))
      .Times(1)
      .WillOnce(Invoke([&mock_ui](const content::RenderFrameHost* frame) {
        return mock_ui.get();
      }));
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab(_))
      .Times(1)
      .WillOnce(
          Invoke([](const content::RenderFrameHost* frame) { return true; }));

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls).Times(0);
  EXPECT_CALL(*storage_, GetNotesById).Times(0);

  // Configure service mock.
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation)
      .Times(1)
      .WillOnce(Invoke(mock_service_.get(),
                       &MockUserNoteService::
                           CallBaseClassOnNoteMetadataFetchedForNavigation));
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied).Times(0);

  // Create a non-empty metadata snapshot.
  UserNoteMetadataSnapshot snapshot;
  GURL url =
      web_contents_list_[0]->GetPrimaryMainFrame()->GetLastCommittedURL();
  snapshot.AddEntry(
      url, note_ids_[0],
      std::make_unique<UserNoteMetadata>(base::Time::Now(), base::Time::Now(),
                                         /*min_note_version=*/1));

  // Simulate the service receiving the empty metadata snapshot after a
  // navigation.
  note_service_->OnNoteMetadataFetchedForNavigation(
      GetAllFramesInUseAsWeakPtr(), UserNoteMetadataSnapshot());
}

// After a navigation to a document that doesn't have user notes, but isn't in
// the foreground, the service should not request the notes UI to show itself
// nor to Invalidate the notes.
// TODO(crbug.com/40832588): This test will need to be changed when notes UI is
// no longer automatically shown on navigation.
TEST_F(UserNoteServiceTest,
       OnNoteMetadataFetchedForNavigationNoNotesBackground) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  AddNewInstanceToManager(manager, note_ids_[0]);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager));

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes).Times(0);
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame(_)).Times(0);
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab(_))
      .Times(1)
      .WillOnce(
          Invoke([](const content::RenderFrameHost* frame) { return false; }));

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls).Times(0);
  EXPECT_CALL(*storage_, GetNotesById).Times(0);

  // Configure service mock.
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation)
      .Times(1)
      .WillOnce(Invoke(mock_service_.get(),
                       &MockUserNoteService::
                           CallBaseClassOnNoteMetadataFetchedForNavigation));
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied).Times(0);

  // Create a non-empty metadata snapshot.
  UserNoteMetadataSnapshot snapshot;
  GURL url =
      web_contents_list_[0]->GetPrimaryMainFrame()->GetLastCommittedURL();
  snapshot.AddEntry(
      url, note_ids_[0],
      std::make_unique<UserNoteMetadata>(base::Time::Now(), base::Time::Now(),
                                         /*min_note_version=*/1));

  // Simulate the service receiving the empty metadata snapshot after a
  // navigation.
  note_service_->OnNoteMetadataFetchedForNavigation(
      GetAllFramesInUseAsWeakPtr(), UserNoteMetadataSnapshot());
}

// Tests that the service requests the right models from the storage after
// receiving the metadata snapshot.
TEST_F(UserNoteServiceTest, OnNoteMetadataFetched) {
  // Initial setup.
  AddNewNotesToService(2);
  AddPartialNotesToService(1);
  UserNoteManager* manager1 = ConfigureNewManager();
  UserNoteManager* manager2 = ConfigureNewManager();
  AddNewInstanceToManager(manager1, note_ids_[0]);
  AddNewInstanceToManager(manager1, note_ids_[1]);
  AddNewInstanceToManager(manager2, note_ids_[2]);
  AddNewInstanceToManager(manager2, note_ids_[3]);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 4u);
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[3]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[1], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[2], manager2));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[3], manager2));

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes).Times(0);
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame).Times(0);
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab).Times(0);

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls).Times(0);
  EXPECT_CALL(*storage_, GetNotesById)
      .Times(1)
      .WillOnce(Invoke(storage_.get(), &MockUserNoteStorage::MockGetNotesById));

  // Configure service mock.
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched)
      .Times(1)
      .WillOnce(
          Invoke(mock_service_.get(),
                 &MockUserNoteService::CallBaseClassOnNoteMetadataFetched));
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched)
      .Times(1)
      .WillOnce(Invoke(mock_service_.get(),
                       &MockUserNoteService::MockOnNoteModelsFetched));
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied).Times(0);

  // Create a fake metadata snapshot with some updated and new notes, including
  // one new note that wasn't in the creation map so simulate receiving it from
  // Sync.
  note_ids_.emplace_back(base::UnguessableToken::Create());
  UserNoteMetadataSnapshot snapshot;
  GURL url1 =
      web_contents_list_[0]->GetPrimaryMainFrame()->GetLastCommittedURL();
  GURL url2 =
      web_contents_list_[1]->GetPrimaryMainFrame()->GetLastCommittedURL();
  snapshot.AddEntry(
      url1, note_ids_[0],
      std::make_unique<UserNoteMetadata>(base::Time::Now(), base::Time::Now(),
                                         /*min_note_version=*/1));
  snapshot.AddEntry(
      url1, note_ids_[4],
      std::make_unique<UserNoteMetadata>(base::Time::Now(), base::Time::Now(),
                                         /*min_note_version=*/1));
  snapshot.AddEntry(
      url2, note_ids_[2],
      std::make_unique<UserNoteMetadata>(base::Time::Now(), base::Time::Now(),
                                         /*min_note_version=*/1));
  snapshot.AddEntry(
      url2, note_ids_[5],
      std::make_unique<UserNoteMetadata>(base::Time::Now(), base::Time::Now(),
                                         /*min_note_version=*/1));

  // Simulate the storage returning the metadata snapshot to the service
  // callback.
  note_service_->OnNoteMetadataFetched(GetAllFramesInUseAsWeakPtr(),
                                       std::move(snapshot));

  // Mocks ensure callbacks are invoked synchronously, so expectations can be
  // immediately verified.
  const UserNoteStorage::IdSet& fetched_ids = storage_->requested_model_ids();
  EXPECT_EQ(fetched_ids.size(), 4u);
  EXPECT_TRUE(base::Contains(fetched_ids, note_ids_[0]));
  EXPECT_TRUE(base::Contains(fetched_ids, note_ids_[2]));
  EXPECT_TRUE(base::Contains(fetched_ids, note_ids_[4]));
  EXPECT_TRUE(base::Contains(fetched_ids, note_ids_[5]));

  const UserNoteService::IdSet& computed_new_notes =
      mock_service_->computed_new_notes();
  EXPECT_EQ(computed_new_notes.size(), 2u);
  EXPECT_NE(computed_new_notes.find(note_ids_[4]), computed_new_notes.end());
  EXPECT_NE(computed_new_notes.find(note_ids_[5]), computed_new_notes.end());
}

// Tests that the service correctly updates the models in the model map and
// applies the necessary note changes.
TEST_F(UserNoteServiceTest, OnNoteModelsFetched) {
  // Initial setup.
  AddNewNotesToService(2);
  AddPartialNotesToService(1);
  UserNoteManager* manager1 = ConfigureNewManager();
  UserNoteManager* manager2 = ConfigureNewManager();
  AddNewInstanceToManager(manager1, note_ids_[0]);
  AddNewInstanceToManager(manager1, note_ids_[1]);
  AddNewInstanceToManager(manager2, note_ids_[2]);
  AddNewInstanceToManager(manager2, note_ids_[3]);

  // For note 1 to be correctly deleted later in the test, its target URL must
  // be changed to match the parent frame.
  content::RenderFrameHost* frame1 =
      web_contents_list_[0]->GetPrimaryMainFrame();
  const auto& note_1_entry_it = note_service_->model_map_.find(note_ids_[1]);
  note_1_entry_it->second.model->Update(std::make_unique<UserNote>(
      note_ids_[1], GetTestUserNoteMetadata(), GetTestUserNoteBody(),
      GetTestUserNotePageTarget(frame1->GetLastCommittedURL().spec())));

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 4u);
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[3]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[1], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[2], manager2));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[3], manager2));

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes).Times(0);
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame).Times(0);
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab).Times(0);

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls).Times(0);
  EXPECT_CALL(*storage_, GetNotesById).Times(0);

  // Configure service mock.
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched)
      .Times(1)
      .WillOnce(Invoke(mock_service_.get(),
                       &MockUserNoteService::CallBaseClassOnNoteModelsFetched));
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied)
      .Times(2)
      .WillRepeatedly(Invoke(mock_service_.get(),
                             &MockUserNoteService::MockOnFrameChangesApplied));

  // Prepare the fake input for passing to the method under test. 2 notes are
  // simulated as having been updated, 2 notes as created, and one as having
  // been removed. One of the created notes is not present in the creation map
  // to simulate receiving it from Sync.
  note_ids_.emplace_back(base::UnguessableToken::Create());
  UserNoteService::IdSet new_notes;
  new_notes.emplace(note_ids_[4]);
  new_notes.emplace(note_ids_[5]);

  content::RenderFrameHost* frame2 =
      web_contents_list_[1]->GetPrimaryMainFrame();
  auto change1 = std::make_unique<MockFrameUserNoteChanges>(
      note_service_->GetSafeRef(), frame1->GetWeakDocumentPtr(),
      /*added=*/IdList{note_ids_[4]},
      /*modified=*/IdList{note_ids_[0]}, /*removed=*/IdList{note_ids_[1]});
  auto change2 = std::make_unique<MockFrameUserNoteChanges>(
      note_service_->GetSafeRef(), frame2->GetWeakDocumentPtr(),
      /*added=*/IdList{note_ids_[5]},
      /*modified=*/IdList{note_ids_[2]}, /*removed=*/IdList{});
  base::UnguessableToken change1_id = change1->id();
  base::UnguessableToken change2_id = change2->id();
  std::vector<std::unique_ptr<FrameUserNoteChanges>> note_changes;
  note_changes.emplace_back(std::move(change1));
  note_changes.emplace_back(std::move(change2));

  const std::u16string kText0 = u"updated note 0";
  const std::u16string kText2 = u"updated note 2";
  const std::u16string kText4 = u"new note 4";
  const std::u16string kText5 = u"new note 5";
  const std::string url1 = frame1->GetLastCommittedURL().spec();
  const std::string url2 = frame2->GetLastCommittedURL().spec();
  auto note0 = std::make_unique<UserNote>(
      note_ids_[0], GetTestUserNoteMetadata(),
      std::make_unique<UserNoteBody>(kText0), GetTestUserNotePageTarget(url1));
  auto note2 = std::make_unique<UserNote>(
      note_ids_[2], GetTestUserNoteMetadata(),
      std::make_unique<UserNoteBody>(kText2), GetTestUserNotePageTarget(url2));
  auto note4 = std::make_unique<UserNote>(
      note_ids_[4], GetTestUserNoteMetadata(),
      std::make_unique<UserNoteBody>(kText4), GetTestUserNotePageTarget(url1));
  auto note5 = std::make_unique<UserNote>(
      note_ids_[5], GetTestUserNoteMetadata(),
      std::make_unique<UserNoteBody>(kText5), GetTestUserNotePageTarget(url2));
  std::vector<std::unique_ptr<UserNote>> note_models;
  note_models.emplace_back(std::move(note0));
  note_models.emplace_back(std::move(note4));
  note_models.emplace_back(std::move(note2));
  note_models.emplace_back(std::move(note5));

  // Simulate the storage returning the updated note models to the service
  // callback.
  note_service_->OnNoteModelsFetched(new_notes, std::move(note_changes),
                                     std::move(note_models));

  // Mocks ensure callbacks are invoked synchronously, so expectations can be
  // immediately verified.
  const IdList& changes_applied = mock_service_->changes_applied();
  EXPECT_EQ(changes_applied.size(), 2u);
  EXPECT_TRUE(base::Contains(changes_applied, change1_id));
  EXPECT_TRUE(base::Contains(changes_applied, change2_id));

  EXPECT_EQ(ModelMapSize(), 5u);
  EXPECT_EQ(CreationMapSize(), 0u);
  EXPECT_FALSE(DoesModelExist(note_ids_[1]));
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[3]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[4]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[5]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[2], manager2));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[3], manager2));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[4], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[5], manager2));

  std::u16string actual_text_0 =
      note_service_->GetNoteModel(note_ids_[0])->body().plain_text_value();
  std::u16string actual_text_2 =
      note_service_->GetNoteModel(note_ids_[2])->body().plain_text_value();
  std::u16string actual_text_4 =
      note_service_->GetNoteModel(note_ids_[4])->body().plain_text_value();
  std::u16string actual_text_5 =
      note_service_->GetNoteModel(note_ids_[5])->body().plain_text_value();
  EXPECT_EQ(actual_text_0, kText0);
  EXPECT_EQ(actual_text_2, kText2);
  EXPECT_EQ(actual_text_4, kText4);
  EXPECT_EQ(actual_text_5, kText5);

  EXPECT_EQ(note_service_->note_changes_in_progress_.size(), 2u);
  EXPECT_NE(note_service_->note_changes_in_progress_.find(change1_id),
            note_service_->note_changes_in_progress_.end());
  EXPECT_NE(note_service_->note_changes_in_progress_.find(change2_id),
            note_service_->note_changes_in_progress_.end());
}

// Tests that the service correctly finalizes frame changes that have been
// applied and notifies the UI to update itself when needed.
TEST_F(UserNoteServiceTest, OnFrameChangesApplied) {
  // Initial setup.
  AddNewNotesToService(2);
  AddPartialNotesToService(1);
  UserNoteManager* manager1 = ConfigureNewManager();
  UserNoteManager* manager2 = ConfigureNewManager();
  AddNewInstanceToManager(manager1, note_ids_[0]);
  AddNewInstanceToManager(manager1, note_ids_[1]);
  AddNewInstanceToManager(manager2, note_ids_[2]);
  AddNewInstanceToManager(manager2, note_ids_[3]);

  content::RenderFrameHost* frame1 =
      web_contents_list_[0]->GetPrimaryMainFrame();
  content::RenderFrameHost* frame2 =
      web_contents_list_[1]->GetPrimaryMainFrame();
  auto change1 = std::make_unique<FrameUserNoteChanges>(
      note_service_->GetSafeRef(), frame1->GetWeakDocumentPtr(),
      /*added=*/IdList{},
      /*modified=*/IdList{note_ids_[0]}, /*removed=*/IdList{});
  auto change2 = std::make_unique<FrameUserNoteChanges>(
      note_service_->GetSafeRef(), frame2->GetWeakDocumentPtr(),
      /*added=*/IdList{},
      /*modified=*/IdList{note_ids_[2]}, /*removed=*/IdList{});
  base::UnguessableToken change1_id = change1->id();
  base::UnguessableToken change2_id = change2->id();
  note_service_->note_changes_in_progress_.emplace(change1_id,
                                                   std::move(change1));
  note_service_->note_changes_in_progress_.emplace(change2_id,
                                                   std::move(change2));

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 4u);
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_EQ(note_service_->note_changes_in_progress_.size(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[3]), 1u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[1], manager1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[2], manager2));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[3], manager2));

  // Configure UI mock.
  auto mock_ui = std::make_unique<MockUserNotesUI>();
  EXPECT_CALL(*mock_ui, InvalidateIfVisible).Times(1);
  EXPECT_CALL(*mock_ui, FocusNote).Times(0);
  EXPECT_CALL(*mock_ui, StartNoteCreation).Times(0);
  EXPECT_CALL(*mock_ui, Show).Times(0);

  // Configure service delegate mock.
  EXPECT_CALL(*service_delegate_, GetAllFramesForUserNotes).Times(0);
  EXPECT_CALL(*service_delegate_, GetUICoordinatorForFrame(_))
      .Times(1)
      .WillOnce(Invoke([&mock_ui](const content::RenderFrameHost* frame) {
        return mock_ui.get();
      }));
  EXPECT_CALL(*service_delegate_, IsFrameInActiveTab(_))
      .Times(2)
      .WillOnce(
          Invoke([](const content::RenderFrameHost* frame) { return true; }))
      .WillOnce(
          Invoke([](const content::RenderFrameHost* frame) { return false; }));

  // Configure storage mock.
  EXPECT_CALL(*storage_, GetNoteMetadataForUrls).Times(0);
  EXPECT_CALL(*storage_, GetNotesById).Times(0);

  // Configure service mock.
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetchedForNavigation).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteMetadataFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnNoteModelsFetched).Times(0);
  EXPECT_CALL(*mock_service_, OnFrameChangesApplied)
      .Times(2)
      .WillRepeatedly(
          Invoke(mock_service_.get(),
                 &MockUserNoteService::CallBaseClassOnFrameChangesApplied));

  // Simulate the first change being applied. It should invalidate the UI since
  // it is simulated as being in an active tab.
  note_service_->OnFrameChangesApplied(change1_id);

  EXPECT_EQ(ModelMapSize(), 4u);
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_EQ(note_service_->note_changes_in_progress_.size(), 1u);
  EXPECT_EQ(note_service_->note_changes_in_progress_.find(change1_id),
            note_service_->note_changes_in_progress_.end());
  EXPECT_NE(note_service_->note_changes_in_progress_.find(change2_id),
            note_service_->note_changes_in_progress_.end());

  Mock::VerifyAndClearExpectations(mock_ui.get());

  // Simulate the second change being applied.
  EXPECT_CALL(*mock_ui, InvalidateIfVisible).Times(0);
  EXPECT_CALL(*mock_ui, FocusNote).Times(0);
  EXPECT_CALL(*mock_ui, StartNoteCreation).Times(0);
  EXPECT_CALL(*mock_ui, Show).Times(0);

  note_service_->OnFrameChangesApplied(change2_id);

  EXPECT_EQ(ModelMapSize(), 4u);
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_EQ(note_service_->note_changes_in_progress_.size(), 0u);
}

// Verify the creation flow is started and a partial note inserted into the
// creation map when "Add Note" is requested (as it would be from the context
// menu). The creation should begin synchronously when there's no selection
// since the renderer doesn't need to create a highlight and selector.
TEST_F(UserNoteServiceTest, OnAddNoteRequestedWithoutSelection) {
  // Initial setup.
  ConfigureNewManager();

  // Verify initial setup.
  ASSERT_EQ(web_contents_list_.size(), 1ul);
  ASSERT_EQ(ModelMapSize(), 2u);
  ASSERT_EQ(CreationMapSize(), 0u);

  content::RenderFrameHost* rfh = web_contents_list_[0]->GetPrimaryMainFrame();

  // Simulate the "Add Note" context menu item being invoked while there's no
  // selection in the renderer.
  note_service_->OnAddNoteRequested(rfh, /*has_selected_text=*/false);

  // Since there's no selection, a new partial note should be added to the
  // creation map synchronously. The model map should be unchanged.
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_EQ(ModelMapSize(), 2u);
}

// Verify the creation flow is started when "Add Note" is requested with a
// selection in the renderer.
TEST_F(UserNoteServiceTest, OnAddNoteRequestedWithSelection) {
  MockAnnotationAgentContainer container;

  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager(&container);

  // Verify initial setup.
  ASSERT_TRUE(container.is_bound());
  ASSERT_EQ(web_contents_list_.size(), 1ul);
  ASSERT_EQ(ModelMapSize(), 2u);
  ASSERT_EQ(CreationMapSize(), 0u);

  content::RenderFrameHost* rfh = web_contents_list_[0]->GetPrimaryMainFrame();

  // Simulate the "Add Note" context menu item being invoked while there's no
  // selection in the renderer.
  note_service_->OnAddNoteRequested(rfh, /*has_selected_text=*/true);

  // Since there's a selection, a partial note won't be created until the
  // renderer replies with a selector.
  EXPECT_EQ(CreationMapSize(), 0u);
  EXPECT_EQ(ModelMapSize(), 2u);

  mojo::Remote<blink::mojom::AnnotationAgentHost> host;
  MockAnnotationAgent agent;

  blink::mojom::SelectorCreationResultPtr selector_creation_result =
      blink::mojom::SelectorCreationResult::New();
  selector_creation_result->host_receiver = host.BindNewPipeAndPassReceiver();
  selector_creation_result->agent_remote = agent.BindNewPipeAndPassRemote();
  selector_creation_result->serialized_selector = "FOO";
  selector_creation_result->selected_text = std::u16string(u"FOO");

  EXPECT_CALL(container,
              CreateAgentFromSelection(blink::mojom::AnnotationType::kUserNote,
                                       testing::_))
      .WillOnce(
          [&](blink::mojom::AnnotationType type,
              MockAnnotationAgentContainer::CreateAgentFromSelectionCallback
                  cb) {
            std::move(cb).Run(
                std::move(selector_creation_result),
                /*error=*/shared_highlighting::LinkGenerationError::kNone,
                /*ready_status=*/
                shared_highlighting::LinkGenerationReadyStatus::
                    kRequestedAfterReady);
          });
  manager->note_agent_container().FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&container);

  // Now that the renderer replied, a new partial note should be added to the
  // creation map.
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_EQ(ModelMapSize(), 2u);
}

}  // namespace user_notes
