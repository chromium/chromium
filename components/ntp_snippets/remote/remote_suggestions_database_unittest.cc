// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_database.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Mock;
using testing::_;

namespace ntp_snippets {

bool operator==(const RemoteSuggestion& lhs, const RemoteSuggestion& rhs) {
  return lhs.id() == rhs.id() && lhs.title() == rhs.title() &&
         lhs.url() == rhs.url() &&
         lhs.publisher_name() == rhs.publisher_name() &&
         lhs.amp_url() == rhs.amp_url() && lhs.snippet() == rhs.snippet() &&
         lhs.salient_image_url() == rhs.salient_image_url() &&
         lhs.publish_date() == rhs.publish_date() &&
         lhs.expiry_date() == rhs.expiry_date() && lhs.score() == rhs.score() &&
         lhs.is_dismissed() == rhs.is_dismissed();
}

namespace {

std::unique_ptr<RemoteSuggestion> CreateTestSuggestion() {
  SnippetProto proto;
  proto.add_ids("http://localhost");
  proto.set_remote_category_id(1);  // Articles
  auto* source = proto.mutable_source();
  source->set_url("http://localhost");
  source->set_publisher_name("Publisher");
  source->set_amp_url("http://amp");
  return RemoteSuggestion::CreateFromProto(proto);
}

// Eq matcher has to store the expected value, but RemoteSuggestion is movable-
// only.
MATCHER_P(PointeeEq, ptr_to_expected, "") {
  return *arg == *ptr_to_expected;
}

}  // namespace

class RemoteSuggestionsDatabaseTest : public testing::Test {
 public:
  RemoteSuggestionsDatabaseTest()
      : suggestion_db_(nullptr), image_db_(nullptr) {}

  void CreateDatabase() {
    // The FakeDBs are owned by |db_|, so clear our pointers before resetting
    // |db_| itself.
    suggestion_db_ = nullptr;
    image_db_ = nullptr;
    // Explicitly destroy any existing database before creating a new one.
    db_.reset();

    auto suggestion_db =
        std::make_unique<FakeDB<SnippetProto>>(&suggestion_db_storage_);
    auto image_db =
        std::make_unique<FakeDB<SnippetImageProto>>(&image_db_storage_);
    suggestion_db_ = suggestion_db.get();
    image_db_ = image_db.get();
    db_ = std::make_unique<RemoteSuggestionsDatabase>(std::move(suggestion_db),
                                                      std::move(image_db));
  }

  void ForceDBError() { db_->OnDatabaseError(); }

  FakeDB<SnippetProto>* suggestion_db() { return suggestion_db_; }
  std::map<std::string, SnippetProto>& suggestion_db_storage() {
    return suggestion_db_storage_;
  }
  FakeDB<SnippetImageProto>* image_db() { return image_db_; }
  std::map<std::string, SnippetImageProto>& image_db_storage() {
    return image_db_storage_;
  }

  RemoteSuggestionsDatabase* db() { return db_.get(); }

  // TODO(tschumann): MOCK_METHODS on non mock objects are an anti-pattern.
  // Clean up.
  void OnSnippetsLoaded(RemoteSuggestion::PtrVector snippets) {
    OnSnippetsLoadedImpl(snippets);
  }
  MOCK_METHOD1(OnSnippetsLoadedImpl,
               void(const RemoteSuggestion::PtrVector& snippets));

  MOCK_METHOD1(OnImageLoaded, void(std::string));

 private:
  std::map<std::string, SnippetProto> suggestion_db_storage_;
  std::map<std::string, SnippetImageProto> image_db_storage_;

  // Owned by |db_|.
  FakeDB<SnippetProto>* suggestion_db_;
  FakeDB<SnippetImageProto>* image_db_;

  std::unique_ptr<RemoteSuggestionsDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsDatabaseTest);
};

TEST_F(RemoteSuggestionsDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(RemoteSuggestionsDatabaseTest, LoadBeforeInit) {
  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  // Start a snippet and image load before the DB is initialized.
  db()->LoadSnippets(
      base::BindOnce(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                     base::Unretained(this)));
  db()->LoadImage("id",
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));

  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_TRUE(db()->IsInitialized());

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_));
  EXPECT_CALL(*this, OnImageLoaded(_));
  // Note: "Load" means "load everything in the DB" (which we do for
  // suggestions), while "Get" means "load a single item from the DB" (which we
  // do for images).
  // Note 2: |suggestion_db| and |image_db| are fakes which pass the proper data
  // back to the callback themselves.
  suggestion_db()->LoadCallback(true);
  image_db()->GetCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, LoadAfterInit) {
  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_)).Times(0);

  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_TRUE(db()->IsInitialized());

  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_));
  db()->LoadSnippets(
      base::BindOnce(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                     base::Unretained(this)));
  suggestion_db()->LoadCallback(true);

  EXPECT_CALL(*this, OnImageLoaded(_));
  db()->LoadImage("id",
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, Save) {
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  // Store a snippet and an image.
  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);
  suggestion_db()->UpdateCallback(true);
  image_db()->UpdateCallback(true);

  // Make sure they're there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::BindOnce(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                     base::Unretained(this)));
  suggestion_db()->LoadCallback(true);

  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, SavePersist) {
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  // Store a snippet and an image.
  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);
  suggestion_db()->UpdateCallback(true);
  image_db()->UpdateCallback(true);

  // They should still exist after recreating the database.
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::BindOnce(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                     base::Unretained(this)));
  suggestion_db()->LoadCallback(true);

  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, Update) {
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();

  // Store a snippet.
  db()->SaveSnippet(*snippet);
  suggestion_db()->UpdateCallback(true);

  // Change it.
  snippet->set_dismissed(true);
  db()->SaveSnippet(*snippet);
  suggestion_db()->UpdateCallback(true);

  // Make sure we get the updated version.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::BindOnce(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                     base::Unretained(this)));
  suggestion_db()->LoadCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, Delete) {
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();

  // Store a snippet.
  db()->SaveSnippet(*snippet);
  suggestion_db()->UpdateCallback(true);

  // Make sure it's there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::BindOnce(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                     base::Unretained(this)));
  suggestion_db()->LoadCallback(true);

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteSnippet(snippet->id());
  suggestion_db()->UpdateCallback(true);

  // Make sure it's gone.
  EXPECT_CALL(*this, OnSnippetsLoadedImpl(IsEmpty()));
  db()->LoadSnippets(
      base::BindOnce(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                     base::Unretained(this)));
  suggestion_db()->LoadCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, DeleteSnippetDoesNotDeleteImage) {
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  // Store a snippet and image.
  db()->SaveSnippet(*snippet);
  suggestion_db()->UpdateCallback(true);
  db()->SaveImage(snippet->id(), image_data);
  image_db()->UpdateCallback(true);

  // Make sure they're there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::BindOnce(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                     base::Unretained(this)));
  suggestion_db()->LoadCallback(true);

  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteSnippet(snippet->id());
  suggestion_db()->UpdateCallback(true);

  // Make sure the image is still there.
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, DeleteImage) {
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  // Store the image.
  db()->SaveImage(snippet->id(), image_data);
  image_db()->UpdateCallback(true);

  // Make sure the image is there.
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteImage(snippet->id());
  image_db()->UpdateCallback(true);

  // Make sure the image is gone.
  EXPECT_CALL(*this, OnImageLoaded(std::string()));
  db()->LoadImage(snippet->id(),
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, ShouldGarbageCollectImages) {
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  // Store images.
  db()->SaveImage("snippet-id-1", "pretty-image-1");
  image_db()->UpdateCallback(true);
  db()->SaveImage("snippet-id-2", "pretty-image-2");
  image_db()->UpdateCallback(true);
  db()->SaveImage("snippet-id-3", "pretty-image-3");
  image_db()->UpdateCallback(true);

  // Make sure the to-be-garbage collected images are there.
  EXPECT_CALL(*this, OnImageLoaded("pretty-image-1"));
  db()->LoadImage("snippet-id-1",
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);

  EXPECT_CALL(*this, OnImageLoaded("pretty-image-3"));
  db()->LoadImage("snippet-id-3",
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);

  // Garbage collect all except the second.
  db()->GarbageCollectImages(std::make_unique<std::set<std::string>>(
      std::set<std::string>({"snippet-id-2"})));
  // This will first load all image IDs, then delete the not-referenced ones.
  image_db()->LoadKeysCallback(true);
  image_db()->UpdateCallback(true);

  // Make sure the images are gone.
  EXPECT_CALL(*this, OnImageLoaded(std::string()));
  db()->LoadImage("snippet-id-1",
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);

  EXPECT_CALL(*this, OnImageLoaded(std::string()));
  db()->LoadImage("snippet-id-3",
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);

  // Make sure the second still exists.
  EXPECT_CALL(*this, OnImageLoaded("pretty-image-2"));
  db()->LoadImage("snippet-id-2",
                  base::BindOnce(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                                 base::Unretained(this)));
  image_db()->GetCallback(true);
}

TEST_F(RemoteSuggestionsDatabaseTest, TryOperationsAfterError) {
  CreateDatabase();
  suggestion_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  image_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(db()->IsInitialized());

  ForceDBError();
  ASSERT_TRUE(db()->IsErrorState());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);
  ASSERT_TRUE(suggestion_db_storage().empty());
  ASSERT_TRUE(image_db_storage().empty());

  db()->DeleteImage("foo");
  db()->GarbageCollectImages(
      std::make_unique<std::set<std::string>>(std::set<std::string>({"foo"})));

  SUCCEED() << "This test passes if it doesn't crash";
}

}  // namespace ntp_snippets
