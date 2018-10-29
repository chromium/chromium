// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/sync_manager_impl.h"

#include <cstddef>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/fake_encryptor.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/mock_unrecoverable_error_handler.h"
#include "components/sync/base/model_type_test_util.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/http_post_provider_interface.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/test_engine_components_factory.h"
#include "components/sync/engine_impl/cycle/sync_cycle.h"
#include "components/sync/engine_impl/sync_scheduler.h"
#include "components/sync/engine_impl/test_entry_factory.h"
#include "components/sync/js/js_event_handler.h"
#include "components/sync/js/js_test_util.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/change_record.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/nigori_util.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/syncable_id.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/syncable/test_user_share.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"
#include "components/sync/test/callback_counter.h"
#include "components/sync/test/engine/fake_model_worker.h"
#include "components/sync/test/engine/fake_sync_scheduler.h"
#include "components/sync/test/engine/test_id_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "url/gurl.h"

using base::ExpectDictStringValue;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace syncer {

using syncable::GET_BY_HANDLE;
using syncable::IS_DEL;
using syncable::IS_UNSYNCED;
using syncable::NON_UNIQUE_NAME;
using syncable::SPECIFICS;
using syncable::kEncryptedString;

namespace {

// Makes a child node under the type root folder.  Returns the id of the
// newly-created node.
int64_t MakeNode(UserShare* share,
                 ModelType model_type,
                 const std::string& client_tag) {
  WriteTransaction trans(FROM_HERE, share);
  WriteNode node(&trans);
  WriteNode::InitUniqueByCreationResult result =
      node.InitUniqueByCreation(model_type, client_tag);
  EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
  node.SetIsFolder(false);
  return node.GetId();
}

// Makes a non-folder child of the root node.  Returns the id of the
// newly-created node.
int64_t MakeNodeWithRoot(UserShare* share,
                         ModelType model_type,
                         const std::string& client_tag) {
  WriteTransaction trans(FROM_HERE, share);
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  WriteNode node(&trans);
  WriteNode::InitUniqueByCreationResult result =
      node.InitUniqueByCreation(model_type, root_node, client_tag);
  EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
  node.SetIsFolder(false);
  return node.GetId();
}

// Makes a folder child of a non-root node. Returns the id of the
// newly-created node.
int64_t MakeFolderWithParent(UserShare* share,
                             ModelType model_type,
                             int64_t parent_id,
                             BaseNode* predecessor) {
  WriteTransaction trans(FROM_HERE, share);
  ReadNode parent_node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, parent_node.InitByIdLookup(parent_id));
  WriteNode node(&trans);
  EXPECT_TRUE(node.InitBookmarkByCreation(parent_node, predecessor));
  node.SetIsFolder(true);
  return node.GetId();
}

int64_t MakeBookmarkWithParent(UserShare* share,
                               int64_t parent_id,
                               BaseNode* predecessor) {
  WriteTransaction trans(FROM_HERE, share);
  ReadNode parent_node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, parent_node.InitByIdLookup(parent_id));
  WriteNode node(&trans);
  EXPECT_TRUE(node.InitBookmarkByCreation(parent_node, predecessor));
  return node.GetId();
}

// Creates the "synced" root node for a particular datatype. We use the syncable
// methods here so that the syncer treats these nodes as if they were already
// received from the server.
int64_t MakeTypeRoot(UserShare* share, ModelType model_type) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST,
                                   share->directory.get());
  // Attempt to lookup by nigori tag.
  std::string type_tag = ModelTypeToRootTag(model_type);
  syncable::Id node_id = syncable::Id::CreateFromServerId(type_tag);
  syncable::MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                               node_id);
  EXPECT_TRUE(entry.good());
  entry.PutBaseVersion(1);
  entry.PutServerVersion(1);
  entry.PutIsUnappliedUpdate(false);
  entry.PutParentId(syncable::Id::GetRoot());
  entry.PutServerParentId(syncable::Id::GetRoot());
  entry.PutServerIsDir(true);
  entry.PutIsDir(true);
  entry.PutServerSpecifics(specifics);
  entry.PutSpecifics(specifics);
  entry.PutUniqueServerTag(type_tag);
  entry.PutNonUniqueName(type_tag);
  entry.PutIsDel(false);
  return entry.GetMetahandle();
}

// Simulates creating a "synced" node as a child of the root datatype node.
int64_t MakeServerNode(UserShare* share,
                       ModelType model_type,
                       const std::string& client_tag,
                       const std::string& hashed_tag,
                       const sync_pb::EntitySpecifics& specifics) {
  syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST,
                                   share->directory.get());
  syncable::Entry root_entry(&trans, syncable::GET_TYPE_ROOT, model_type);
  EXPECT_TRUE(root_entry.good());
  syncable::Id root_id = root_entry.GetId();
  syncable::Id node_id = syncable::Id::CreateFromServerId(client_tag);
  syncable::MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                               node_id);
  EXPECT_TRUE(entry.good());
  entry.PutBaseVersion(1);
  entry.PutServerVersion(1);
  entry.PutIsUnappliedUpdate(false);
  entry.PutServerParentId(root_id);
  entry.PutParentId(root_id);
  entry.PutServerIsDir(false);
  entry.PutIsDir(false);
  entry.PutServerSpecifics(specifics);
  entry.PutSpecifics(specifics);
  entry.PutNonUniqueName(client_tag);
  entry.PutUniqueClientTag(hashed_tag);
  entry.PutIsDel(false);
  return entry.GetMetahandle();
}

int GetTotalNodeCount(UserShare* share, int64_t root) {
  ReadTransaction trans(FROM_HERE, share);
  ReadNode node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(root));
  return node.GetTotalNodeCount();
}

const char kUrl[] = "example.com";
const char kPasswordValue[] = "secret";
const char kClientTag[] = "tag";

}  // namespace

// Unit tests for the SyncApi. Note that a lot of the underlying
// functionality is provided by the Syncable layer, which has its own
// unit tests. We'll test SyncApi specific things in this harness.
class SyncApiTest : public testing::Test {
 public:
  void SetUp() override { test_user_share_.SetUp(); }

  void TearDown() override { test_user_share_.TearDown(); }

 protected:
  // Create an entry with the given |model_type| and |client_tag|.
  void CreateEntry(const ModelType& model_type, const std::string& client_tag);

  // Attempts to load the entry specified by |model_type| and |client_tag| and
  // returns the lookup result code.
  BaseNode::InitByLookupResult LookupEntryByClientTag(
      const ModelType& model_type,
      const std::string& client_tag);

  // Replace the entry specified by |model_type| and |client_tag| with a
  // tombstone.
  void ReplaceWithTombstone(const ModelType& model_type,
                            const std::string& client_tag);

  // Save changes to the Directory, destroy it then reload it.
  bool ReloadDir();

  UserShare* user_share();
  syncable::Directory* dir();
  SyncEncryptionHandler* encryption_handler();
  PassphraseType GetPassphraseType(BaseTransaction* trans);

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestUserShare test_user_share_;
};

UserShare* SyncApiTest::user_share() {
  return test_user_share_.user_share();
}

syncable::Directory* SyncApiTest::dir() {
  return test_user_share_.user_share()->directory.get();
}

SyncEncryptionHandler* SyncApiTest::encryption_handler() {
  return test_user_share_.encryption_handler();
}

PassphraseType SyncApiTest::GetPassphraseType(BaseTransaction* trans) {
  return dir()->GetNigoriHandler()->GetPassphraseType(trans->GetWrappedTrans());
}

bool SyncApiTest::ReloadDir() {
  return test_user_share_.Reload();
}

void SyncApiTest::CreateEntry(const ModelType& model_type,
                              const std::string& client_tag) {
  WriteTransaction trans(FROM_HERE, user_share());
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  WriteNode node(&trans);
  ASSERT_EQ(node.InitUniqueByCreation(model_type, root_node, client_tag),
            WriteNode::INIT_SUCCESS);
}

BaseNode::InitByLookupResult SyncApiTest::LookupEntryByClientTag(
    const ModelType& model_type,
    const std::string& client_tag) {
  ReadTransaction trans(FROM_HERE, user_share());
  ReadNode node(&trans);
  return node.InitByClientTagLookup(model_type, client_tag);
}

void SyncApiTest::ReplaceWithTombstone(const ModelType& model_type,
                                       const std::string& client_tag) {
  WriteTransaction trans(FROM_HERE, user_share());
  WriteNode node(&trans);
  ASSERT_EQ(node.InitByClientTagLookup(model_type, client_tag),
            WriteNode::INIT_OK);
  node.Tombstone();
}

TEST_F(SyncApiTest, SanityCheckTest) {
  {
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_TRUE(trans.GetWrappedTrans());
  }
  {
    WriteTransaction trans(FROM_HERE, user_share());
    EXPECT_TRUE(trans.GetWrappedTrans());
  }
  {
    // No entries but root should exist
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode node(&trans);
    // Metahandle 1 can be root, sanity check 2
    EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_NOT_GOOD, node.InitByIdLookup(2));
  }
}

TEST_F(SyncApiTest, BasicTagWrite) {
  {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(kInvalidId, root_node.GetFirstChildId());
  }

  ignore_result(MakeNodeWithRoot(user_share(), BOOKMARKS, "testtag"));

  {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, "testtag"));
    EXPECT_NE(0, node.GetId());

    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(node.GetId(), root_node.GetFirstChildId());
  }
}

TEST_F(SyncApiTest, BasicTagWriteWithImplicitParent) {
  int64_t type_root = MakeTypeRoot(user_share(), PREFERENCES);

  {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode type_root_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, type_root_node.InitByIdLookup(type_root));
    EXPECT_EQ(kInvalidId, type_root_node.GetFirstChildId());
  }

  ignore_result(MakeNode(user_share(), PREFERENCES, "testtag"));

  {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, "testtag"));
    EXPECT_EQ(kInvalidId, node.GetParentId());

    ReadNode type_root_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, type_root_node.InitByIdLookup(type_root));
    EXPECT_EQ(node.GetId(), type_root_node.GetFirstChildId());
  }
}

TEST_F(SyncApiTest, ModelTypesSiloed) {
  {
    WriteTransaction trans(FROM_HERE, user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(root_node.GetFirstChildId(), 0);
  }

  ignore_result(MakeNodeWithRoot(user_share(), BOOKMARKS, "collideme"));
  ignore_result(MakeNodeWithRoot(user_share(), PREFERENCES, "collideme"));
  ignore_result(MakeNodeWithRoot(user_share(), AUTOFILL, "collideme"));

  {
    ReadTransaction trans(FROM_HERE, user_share());

    ReadNode bookmarknode(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              bookmarknode.InitByClientTagLookup(BOOKMARKS, "collideme"));

    ReadNode prefnode(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              prefnode.InitByClientTagLookup(PREFERENCES, "collideme"));

    ReadNode autofillnode(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              autofillnode.InitByClientTagLookup(AUTOFILL, "collideme"));

    EXPECT_NE(bookmarknode.GetId(), prefnode.GetId());
    EXPECT_NE(autofillnode.GetId(), prefnode.GetId());
    EXPECT_NE(bookmarknode.GetId(), autofillnode.GetId());
  }
}

TEST_F(SyncApiTest, ReadMissingTagsFails) {
  {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_NOT_GOOD,
              node.InitByClientTagLookup(BOOKMARKS, "testtag"));
  }
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_NOT_GOOD,
              node.InitByClientTagLookup(BOOKMARKS, "testtag"));
  }
}

// TODO(chron): Hook this all up to the server and write full integration tests
//              for update->undelete behavior.
TEST_F(SyncApiTest, TestDeleteBehavior) {
  int64_t node_id;
  int64_t folder_id;
  std::string test_title("test1");

  {
    WriteTransaction trans(FROM_HERE, user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    // we'll use this spare folder later
    WriteNode folder_node(&trans);
    EXPECT_TRUE(folder_node.InitBookmarkByCreation(root_node, nullptr));
    folder_id = folder_node.GetId();

    WriteNode wnode(&trans);
    WriteNode::InitUniqueByCreationResult result =
        wnode.InitUniqueByCreation(BOOKMARKS, root_node, "testtag");
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    wnode.SetIsFolder(false);
    wnode.SetTitle(test_title);

    node_id = wnode.GetId();
  }

  // Ensure we can delete something with a tag.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode wnode(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              wnode.InitByClientTagLookup(BOOKMARKS, "testtag"));
    EXPECT_FALSE(wnode.GetIsFolder());
    EXPECT_EQ(wnode.GetTitle(), test_title);

    wnode.Tombstone();
  }

  // Lookup of a node which was deleted should return failure,
  // but have found some data about the node.
  {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_IS_DEL,
              node.InitByClientTagLookup(BOOKMARKS, "testtag"));
    // Note that for proper function of this API this doesn't need to be
    // filled, we're checking just to make sure the DB worked in this test.
    EXPECT_EQ(node.GetTitle(), test_title);
  }

  {
    WriteTransaction trans(FROM_HERE, user_share());
    ReadNode folder_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, folder_node.InitByIdLookup(folder_id));

    WriteNode wnode(&trans);
    // This will undelete the tag.
    WriteNode::InitUniqueByCreationResult result =
        wnode.InitUniqueByCreation(BOOKMARKS, folder_node, "testtag");
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    EXPECT_EQ(wnode.GetIsFolder(), false);
    EXPECT_EQ(wnode.GetParentId(), folder_node.GetId());
    EXPECT_EQ(wnode.GetId(), node_id);
    EXPECT_NE(wnode.GetTitle(), test_title);  // Title should be cleared
    wnode.SetTitle(test_title);
  }

  // Now look up should work.
  {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, "testtag"));
    EXPECT_EQ(node.GetTitle(), test_title);
    EXPECT_EQ(node.GetModelType(), BOOKMARKS);
  }
}

TEST_F(SyncApiTest, WriteAndReadPassword) {
  KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), "passphrase"};
  {
    ReadTransaction trans(FROM_HERE, user_share());
    trans.GetCryptographer()->AddKey(params);
  }

  {
    WriteTransaction trans(FROM_HERE, user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS, root_node, kClientTag);
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    sync_pb::PasswordSpecificsData data;
    data.set_password_value(kPasswordValue);
    password_node.SetPasswordSpecifics(data);
  }
  {
    ReadTransaction trans(FROM_HERE, user_share());

    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              password_node.InitByClientTagLookup(PASSWORDS, kClientTag));
    const sync_pb::PasswordSpecificsData& data =
        password_node.GetPasswordSpecifics();
    EXPECT_EQ(kPasswordValue, data.password_value());
  }
}

TEST_F(SyncApiTest, WritePasswordAndCheckMetadata) {
  KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), "passphrase"};
  {
    ReadTransaction trans(FROM_HERE, user_share());
    trans.GetCryptographer()->AddKey(params);
  }

  {
    WriteTransaction trans(FROM_HERE, user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS, root_node, kClientTag);
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    sync_pb::PasswordSpecificsData data;
    data.set_password_value(kPasswordValue);
    data.set_signon_realm(kUrl);
    password_node.SetPasswordSpecifics(data);
  }
  {
    ReadTransaction trans(FROM_HERE, user_share());

    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              password_node.InitByClientTagLookup(PASSWORDS, kClientTag));
    const sync_pb::PasswordSpecificsData& data =
        password_node.GetPasswordSpecifics();
    EXPECT_EQ(kPasswordValue, data.password_value());
    EXPECT_EQ(kUrl, password_node.GetEntitySpecifics()
                        .password()
                        .unencrypted_metadata()
                        .url());
  }
}

TEST_F(SyncApiTest, WriteEncryptedTitle) {
  KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), "passphrase"};
  {
    ReadTransaction trans(FROM_HERE, user_share());
    trans.GetCryptographer()->AddKey(params);
  }
  encryption_handler()->EnableEncryptEverything();
  int bookmark_id;
  {
    WriteTransaction trans(FROM_HERE, user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode bookmark_node(&trans);
    ASSERT_TRUE(bookmark_node.InitBookmarkByCreation(root_node, nullptr));
    bookmark_id = bookmark_node.GetId();
    bookmark_node.SetTitle("foo");

    WriteNode pref_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        pref_node.InitUniqueByCreation(PREFERENCES, root_node, "bar");
    ASSERT_EQ(WriteNode::INIT_SUCCESS, result);
    pref_node.SetTitle("bar");
  }
  {
    ReadTransaction trans(FROM_HERE, user_share());

    ReadNode bookmark_node(&trans);
    ASSERT_EQ(BaseNode::INIT_OK, bookmark_node.InitByIdLookup(bookmark_id));
    EXPECT_EQ("foo", bookmark_node.GetTitle());
    EXPECT_EQ(kEncryptedString, bookmark_node.GetEntry()->GetNonUniqueName());

    ReadNode pref_node(&trans);
    ASSERT_EQ(BaseNode::INIT_OK,
              pref_node.InitByClientTagLookup(PREFERENCES, "bar"));
    EXPECT_EQ(kEncryptedString, pref_node.GetTitle());
  }
}

// Non-unique name should not be empty. For bookmarks non-unique name is copied
// from bookmark title. This test verifies that setting bookmark title to ""
// results in single space title and non-unique name in internal representation.
// GetTitle should still return empty string.
TEST_F(SyncApiTest, WriteEmptyBookmarkTitle) {
  int bookmark_id;
  {
    WriteTransaction trans(FROM_HERE, user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode bookmark_node(&trans);
    ASSERT_TRUE(bookmark_node.InitBookmarkByCreation(root_node, nullptr));
    bookmark_id = bookmark_node.GetId();
    bookmark_node.SetTitle("");
  }
  {
    ReadTransaction trans(FROM_HERE, user_share());

    ReadNode bookmark_node(&trans);
    ASSERT_EQ(BaseNode::INIT_OK, bookmark_node.InitByIdLookup(bookmark_id));
    EXPECT_EQ("", bookmark_node.GetTitle());
    EXPECT_EQ(" ", bookmark_node.GetEntitySpecifics().bookmark().title());
    EXPECT_EQ(" ", bookmark_node.GetEntry()->GetNonUniqueName());
  }
}

TEST_F(SyncApiTest, BaseNodeSetSpecifics) {
  int64_t child_id = MakeNodeWithRoot(user_share(), BOOKMARKS, "testtag");
  WriteTransaction trans(FROM_HERE, user_share());
  WriteNode node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(child_id));

  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("http://www.google.com");

  EXPECT_NE(entity_specifics.SerializeAsString(),
            node.GetEntitySpecifics().SerializeAsString());
  node.SetEntitySpecifics(entity_specifics);
  EXPECT_EQ(entity_specifics.SerializeAsString(),
            node.GetEntitySpecifics().SerializeAsString());
}

TEST_F(SyncApiTest, BaseNodeSetSpecificsPreservesUnknownFields) {
  int64_t child_id = MakeNodeWithRoot(user_share(), BOOKMARKS, "testtag");
  WriteTransaction trans(FROM_HERE, user_share());
  WriteNode node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(child_id));
  EXPECT_TRUE(node.GetEntitySpecifics().unknown_fields().empty());

  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("http://www.google.com");
  std::string unknown_fields;
  {
    ::google::protobuf::io::StringOutputStream unknown_fields_stream(
        &unknown_fields);
    ::google::protobuf::io::CodedOutputStream output(&unknown_fields_stream);
    const int tag = 5;
    const int value = 100;
    output.WriteTag(tag);
    output.WriteLittleEndian32(value);
  }
  *entity_specifics.mutable_unknown_fields() = unknown_fields;
  node.SetEntitySpecifics(entity_specifics);
  EXPECT_FALSE(node.GetEntitySpecifics().unknown_fields().empty());
  EXPECT_EQ(unknown_fields, node.GetEntitySpecifics().unknown_fields());

  entity_specifics.mutable_unknown_fields()->clear();
  node.SetEntitySpecifics(entity_specifics);
  EXPECT_FALSE(node.GetEntitySpecifics().unknown_fields().empty());
  EXPECT_EQ(unknown_fields, node.GetEntitySpecifics().unknown_fields());
}

TEST_F(SyncApiTest, EmptyTags) {
  WriteTransaction trans(FROM_HERE, user_share());
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  WriteNode node(&trans);
  std::string empty_tag;
  WriteNode::InitUniqueByCreationResult result =
      node.InitUniqueByCreation(TYPED_URLS, root_node, empty_tag);
  EXPECT_NE(WriteNode::INIT_SUCCESS, result);
  EXPECT_EQ(BaseNode::INIT_FAILED_PRECONDITION,
            node.InitByClientTagLookup(TYPED_URLS, empty_tag));
}

// Test counting nodes when the type's root node has no children.
TEST_F(SyncApiTest, GetTotalNodeCountEmpty) {
  int64_t type_root = MakeTypeRoot(user_share(), BOOKMARKS);
  EXPECT_EQ(1, GetTotalNodeCount(user_share(), type_root));
}

// Test counting nodes when there is one child beneath the type's root.
TEST_F(SyncApiTest, GetTotalNodeCountOneChild) {
  int64_t type_root = MakeTypeRoot(user_share(), BOOKMARKS);
  int64_t parent =
      MakeFolderWithParent(user_share(), BOOKMARKS, type_root, nullptr);
  EXPECT_EQ(2, GetTotalNodeCount(user_share(), type_root));
  EXPECT_EQ(1, GetTotalNodeCount(user_share(), parent));
}

// Test counting nodes when there are multiple children beneath the type root,
// and one of those children has children of its own.
TEST_F(SyncApiTest, GetTotalNodeCountMultipleChildren) {
  int64_t type_root = MakeTypeRoot(user_share(), BOOKMARKS);
  int64_t parent =
      MakeFolderWithParent(user_share(), BOOKMARKS, type_root, nullptr);
  ignore_result(
      MakeFolderWithParent(user_share(), BOOKMARKS, type_root, nullptr));
  int64_t child1 =
      MakeFolderWithParent(user_share(), BOOKMARKS, parent, nullptr);
  ignore_result(MakeBookmarkWithParent(user_share(), parent, nullptr));
  ignore_result(MakeBookmarkWithParent(user_share(), child1, nullptr));
  EXPECT_EQ(6, GetTotalNodeCount(user_share(), type_root));
  EXPECT_EQ(4, GetTotalNodeCount(user_share(), parent));
}

// This tests directory integrity in the case of creating a new unique node
// with client tag matching that of an existing unapplied node with server only
// data. See crbug.com/505761.
TEST_F(SyncApiTest, WriteNode_UniqueByCreation_UndeleteCase) {
  int64_t preferences_root = MakeTypeRoot(user_share(), PREFERENCES);

  // Create a node with server only data.
  int64_t item1 = 0;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST,
                                     user_share()->directory.get());
    syncable::MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                                 syncable::Id::CreateFromServerId("foo1"));
    DCHECK(entry.good());
    entry.PutServerVersion(10);
    entry.PutIsUnappliedUpdate(true);
    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(PREFERENCES, &specifics);
    entry.PutServerSpecifics(specifics);
    const std::string hash = GenerateSyncableHash(PREFERENCES, "foo");
    entry.PutUniqueClientTag(hash);
    item1 = entry.GetMetahandle();
  }

  // Verify that the server-only item is invisible as a child of
  // of |preferences_root| because at this point it should have the
  // "deleted" flag set.
  EXPECT_EQ(1, GetTotalNodeCount(user_share(), preferences_root));

  // Create a client node with the same tag as the node above.
  int64_t item2 = MakeNode(user_share(), PREFERENCES, "foo");
  // Expect this to be the same directory entry as |item1|.
  EXPECT_EQ(item1, item2);
  // Expect it to be visible as a child of |preferences_root|.
  EXPECT_EQ(2, GetTotalNodeCount(user_share(), preferences_root));

  // Tombstone the new item
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(item1));
    node.Tombstone();
  }

  // Verify that it is gone from the index.
  EXPECT_EQ(1, GetTotalNodeCount(user_share(), preferences_root));
}

// Tests that InitUniqueByCreation called for existing encrypted entry properly
// decrypts specifics and pust them in BaseNode::unencrypted_data_.
TEST_F(SyncApiTest, WriteNode_UniqueByCreation_EncryptedExistingEntry) {
  KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), "passphrase"};
  {
    ReadTransaction trans(FROM_HERE, user_share());
    trans.GetCryptographer()->AddKey(params);
  }
  encryption_handler()->EnableEncryptEverything();
  WriteTransaction trans(FROM_HERE, user_share());
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();

  {
    WriteNode pref_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        pref_node.InitUniqueByCreation(PREFERENCES, root_node, "bar");
    ASSERT_EQ(WriteNode::INIT_SUCCESS, result);
    pref_node.SetTitle("bar");
    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.mutable_preference();
    pref_node.SetEntitySpecifics(entity_specifics);
  }
  {
    WriteNode pref_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        pref_node.InitUniqueByCreation(PREFERENCES, root_node, "bar");
    ASSERT_EQ(WriteNode::INIT_SUCCESS, result);
    // Call GetEntitySpecifics, ensure it doesn't DCHECK.
    pref_node.GetEntitySpecifics();
  }
}

// Tests that undeleting deleted password doesn't trigger any issues.
// See crbug/440430.
TEST_F(SyncApiTest, WriteNode_PasswordUniqueByCreationAfterDelete) {
  KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), "passphrase"};
  {
    ReadTransaction trans(FROM_HERE, user_share());
    trans.GetCryptographer()->AddKey(params);
  }

  WriteTransaction trans(FROM_HERE, user_share());
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  // Create new password.
  {
    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS, root_node, "foo");
    ASSERT_EQ(WriteNode::INIT_SUCCESS, result);
    sync_pb::PasswordSpecificsData password_specifics;
    password_specifics.set_password_value("secret");
    password_node.SetPasswordSpecifics(password_specifics);
  }
  // Delete password.
  {
    WriteNode password_node(&trans);
    BaseNode::InitByLookupResult result =
        password_node.InitByClientTagLookup(PASSWORDS, "foo");
    ASSERT_EQ(BaseNode::INIT_OK, result);
    password_node.Tombstone();
  }
  // Create password again triggering undeletion.
  {
    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS, root_node, "foo");
    ASSERT_EQ(WriteNode::INIT_SUCCESS, result);
  }
}

namespace {

class TestHttpPostProviderInterface : public HttpPostProviderInterface {
 public:
  ~TestHttpPostProviderInterface() override {}

  void SetExtraRequestHeaders(const char* headers) override {}
  void SetURL(const char* url, int port) override {}
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override {}
  bool MakeSynchronousPost(int* error_code, int* response_code) override {
    return false;
  }
  int GetResponseContentLength() const override { return 0; }
  const char* GetResponseContent() const override { return ""; }
  const std::string GetResponseHeaderValue(
      const std::string& name) const override {
    return std::string();
  }
  void Abort() override {}
};

class TestHttpPostProviderFactory : public HttpPostProviderFactory {
 public:
  ~TestHttpPostProviderFactory() override {}
  void Init(const std::string& user_agent,
            const BindToTrackerCallback& bind_to_tracker_callback) override {}
  HttpPostProviderInterface* Create() override {
    return new TestHttpPostProviderInterface();
  }
  void Destroy(HttpPostProviderInterface* http) override {
    delete static_cast<TestHttpPostProviderInterface*>(http);
  }
};

class SyncManagerObserverMock : public SyncManager::Observer {
 public:
  MOCK_METHOD1(OnSyncCycleCompleted, void(const SyncCycleSnapshot&));  // NOLINT
  MOCK_METHOD4(OnInitializationComplete,
               void(const WeakHandle<JsBackend>&,
                    const WeakHandle<DataTypeDebugInfoListener>&,
                    bool,
                    ModelTypeSet));                                 // NOLINT
  MOCK_METHOD1(OnConnectionStatusChange, void(ConnectionStatus));   // NOLINT
  MOCK_METHOD1(OnUpdatedToken, void(const std::string&));           // NOLINT
  MOCK_METHOD1(OnActionableError, void(const SyncProtocolError&));  // NOLINT
  MOCK_METHOD1(OnMigrationRequested, void(ModelTypeSet));           // NOLINT
  MOCK_METHOD1(OnProtocolEvent, void(const ProtocolEvent&));        // NOLINT
};

class SyncEncryptionHandlerObserverMock
    : public SyncEncryptionHandler::Observer {
 public:
  MOCK_METHOD3(OnPassphraseRequired,
               void(PassphraseRequiredReason,
                    const KeyDerivationParams&,
                    const sync_pb::EncryptedData&));  // NOLINT
  MOCK_METHOD0(OnPassphraseAccepted, void());         // NOLINT
  MOCK_METHOD2(OnBootstrapTokenUpdated,
               void(const std::string&, BootstrapTokenType type));  // NOLINT
  MOCK_METHOD2(OnEncryptedTypesChanged, void(ModelTypeSet, bool));  // NOLINT
  MOCK_METHOD0(OnEncryptionComplete, void());                       // NOLINT
  MOCK_METHOD1(OnCryptographerStateChanged, void(Cryptographer*));  // NOLINT
  MOCK_METHOD2(OnPassphraseTypeChanged,
               void(PassphraseType,
                    base::Time));  // NOLINT
  MOCK_METHOD1(OnLocalSetPassphraseEncryption,
               void(const SyncEncryptionHandler::NigoriState&));  // NOLINT
};

}  // namespace

class SyncManagerTest : public testing::Test,
                        public SyncManager::ChangeDelegate {
 protected:
  enum NigoriStatus { DONT_WRITE_NIGORI, WRITE_TO_NIGORI };

  enum EncryptionStatus { UNINITIALIZED, DEFAULT_ENCRYPTION, FULL_ENCRYPTION };

  SyncManagerTest()
      : sync_manager_("Test sync manager",
                      network::TestNetworkConnectionTracker::GetInstance()) {
    switches_.encryption_method = EngineComponentsFactory::ENCRYPTION_KEYSTORE;
  }

  ~SyncManagerTest() override {}

  virtual void DoSetUp(bool enable_local_sync_backend) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    extensions_activity_ = new ExtensionsActivity();

    SyncCredentials credentials;
    credentials.account_id = "foo@bar.com";
    credentials.email = "foo@bar.com";
    credentials.sync_token = "sometoken";

    sync_manager_.AddObserver(&manager_observer_);
    EXPECT_CALL(manager_observer_, OnInitializationComplete(_, _, _, _))
        .WillOnce(DoAll(SaveArg<0>(&js_backend_),
                        SaveArg<2>(&initialization_succeeded_)));

    EXPECT_FALSE(js_backend_.IsInitialized());

    std::vector<scoped_refptr<ModelSafeWorker>> workers;

    // This works only because all routing info types are GROUP_PASSIVE.
    // If we had types in other groups, we would need additional workers
    // to support them.
    scoped_refptr<ModelSafeWorker> worker = new FakeModelWorker(GROUP_PASSIVE);
    workers.push_back(worker);

    SyncManager::InitArgs args;
    args.database_location = temp_dir_.GetPath();
    args.service_url = GURL("https://example.com/");
    args.post_factory = std::unique_ptr<HttpPostProviderFactory>(
        new TestHttpPostProviderFactory());
    args.workers = workers;
    args.extensions_activity = extensions_activity_.get(),
    args.change_delegate = this;
    if (!enable_local_sync_backend)
      args.credentials = credentials;
    args.invalidator_client_id = "fake_invalidator_client_id";
    args.enable_local_sync_backend = enable_local_sync_backend;
    args.local_sync_backend_folder = temp_dir_.GetPath();
    args.engine_components_factory.reset(GetFactory());
    args.encryptor = &encryptor_;
    args.unrecoverable_error_handler =
        MakeWeakHandle(mock_unrecoverable_error_handler_.GetWeakPtr());
    args.cancelation_signal = &cancelation_signal_;
    args.short_poll_interval = base::TimeDelta::FromMinutes(60);
    args.long_poll_interval = base::TimeDelta::FromMinutes(120);
    sync_manager_.Init(&args);

    sync_manager_.GetEncryptionHandler()->AddObserver(&encryption_observer_);

    EXPECT_TRUE(js_backend_.IsInitialized());
    EXPECT_EQ(EngineComponentsFactory::STORAGE_ON_DISK, storage_used_);

    if (initialization_succeeded_) {
      ModelTypeSet enabled_types = GetEnabledTypes();
      for (ModelType type : enabled_types) {
        type_roots_[type] = MakeTypeRoot(sync_manager_.GetUserShare(), type);
      }
    }

    PumpLoop();
  }

  // Test implementation.
  void SetUp() override { DoSetUp(false); }

  void TearDown() override {
    sync_manager_.RemoveObserver(&manager_observer_);
    sync_manager_.ShutdownOnSyncThread();
    PumpLoop();
  }

  ModelTypeSet GetEnabledTypes() {
    ModelTypeSet enabled_types;
    enabled_types.Put(NIGORI);
    enabled_types.Put(DEVICE_INFO);
    enabled_types.Put(EXPERIMENTS);
    enabled_types.Put(BOOKMARKS);
    enabled_types.Put(THEMES);
    enabled_types.Put(SESSIONS);
    enabled_types.Put(PASSWORDS);
    enabled_types.Put(PREFERENCES);
    enabled_types.Put(PRIORITY_PREFERENCES);

    return enabled_types;
  }

  void OnChangesApplied(ModelType model_type,
                        int64_t model_version,
                        const BaseTransaction* trans,
                        const ImmutableChangeRecordList& changes) override {}

  void OnChangesComplete(ModelType model_type) override {}

  // Helper methods.
  bool SetUpEncryption(NigoriStatus nigori_status,
                       EncryptionStatus encryption_status) {
    UserShare* share = sync_manager_.GetUserShare();

    // We need to create the nigori node as if it were an applied server update.
    int64_t nigori_id = GetIdForDataType(NIGORI);
    if (nigori_id == kInvalidId)
      return false;

    // Set the nigori cryptographer information.
    if (encryption_status == FULL_ENCRYPTION)
      sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();

    WriteTransaction trans(FROM_HERE, share);
    Cryptographer* cryptographer = trans.GetCryptographer();
    if (!cryptographer)
      return false;
    if (encryption_status != UNINITIALIZED) {
      KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), "foobar"};
      cryptographer->AddKey(params);
    } else {
      DCHECK_NE(nigori_status, WRITE_TO_NIGORI);
    }
    if (nigori_status == WRITE_TO_NIGORI) {
      sync_pb::NigoriSpecifics nigori;
      cryptographer->GetKeys(nigori.mutable_encryption_keybag());
      share->directory->GetNigoriHandler()->UpdateNigoriFromEncryptedTypes(
          &nigori, trans.GetWrappedTrans());
      WriteNode node(&trans);
      EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(nigori_id));
      node.SetNigoriSpecifics(nigori);
    }
    return cryptographer->is_ready();
  }

  int64_t GetIdForDataType(ModelType type) {
    if (type_roots_.count(type) == 0)
      return 0;
    return type_roots_[type];
  }

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler) {
    js_backend_.Call(FROM_HERE, &JsBackend::SetJsEventHandler, event_handler);
    PumpLoop();
  }

  // Looks up an entry by client tag and resets IS_UNSYNCED value to false.
  // Returns true if entry was previously unsynced, false if IS_UNSYNCED was
  // already false.
  bool ResetUnsyncedEntry(ModelType type, const std::string& client_tag) {
    UserShare* share = sync_manager_.GetUserShare();
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST,
                                     share->directory.get());
    const std::string hash = GenerateSyncableHash(type, client_tag);
    syncable::MutableEntry entry(&trans, syncable::GET_BY_CLIENT_TAG, hash);
    EXPECT_TRUE(entry.good());
    if (!entry.GetIsUnsynced())
      return false;
    entry.PutIsUnsynced(false);
    return true;
  }

  virtual EngineComponentsFactory* GetFactory() {
    return new TestEngineComponentsFactory(
        GetSwitches(), EngineComponentsFactory::STORAGE_IN_MEMORY,
        &storage_used_);
  }

  // Returns true if we are currently encrypting all sync data.  May
  // be called on any thread.
  bool IsEncryptEverythingEnabledForTest() {
    return sync_manager_.GetEncryptionHandler()->IsEncryptEverythingEnabled();
  }

  // Gets the set of encrypted types from the cryptographer
  // Note: opens a transaction.  May be called from any thread.
  ModelTypeSet GetEncryptedTypes() {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    return GetEncryptedTypesWithTrans(&trans);
  }

  ModelTypeSet GetEncryptedTypesWithTrans(BaseTransaction* trans) {
    return trans->GetDirectory()->GetNigoriHandler()->GetEncryptedTypes(
        trans->GetWrappedTrans());
  }

  PassphraseType GetPassphraseType() {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    return GetPassphraseTypeWithTrans(&trans);
  }

  PassphraseType GetPassphraseTypeWithTrans(BaseTransaction* trans) {
    return trans->GetDirectory()->GetNigoriHandler()->GetPassphraseType(
        trans->GetWrappedTrans());
  }

  void SimulateInvalidatorEnabledForTest(bool is_enabled) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sync_manager_.sequence_checker_);
    sync_manager_.SetInvalidatorEnabled(is_enabled);
  }

  void SetProgressMarkerForType(ModelType type, bool set) {
    if (set) {
      sync_pb::DataTypeProgressMarker marker;
      marker.set_token("token");
      marker.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
      sync_manager_.directory()->SetDownloadProgress(type, marker);
    } else {
      sync_pb::DataTypeProgressMarker marker;
      sync_manager_.directory()->SetDownloadProgress(type, marker);
    }
  }

  EngineComponentsFactory::Switches GetSwitches() const { return switches_; }

  void ExpectPassphraseAcceptance() {
    EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
    EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
    EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  }

  void SetCustomPassphraseAndCheck(const std::string& passphrase) {
    EXPECT_CALL(encryption_observer_,
                OnPassphraseTypeChanged(PassphraseType::CUSTOM_PASSPHRASE, _));
    sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(passphrase);
    EXPECT_EQ(PassphraseType::CUSTOM_PASSPHRASE, GetPassphraseType());
  }

  bool HasUnrecoverableError() {
    return mock_unrecoverable_error_handler_.invocation_count() > 0;
  }

 private:
  // Needed by |sync_manager_|.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  // Needed by |sync_manager_|.
  base::ScopedTempDir temp_dir_;
  // Sync Id's for the roots of the enabled datatypes.
  std::map<ModelType, int64_t> type_roots_;
  scoped_refptr<ExtensionsActivity> extensions_activity_;

 protected:
  FakeEncryptor encryptor_;
  SyncManagerImpl sync_manager_;
  CancelationSignal cancelation_signal_;
  WeakHandle<JsBackend> js_backend_;
  bool initialization_succeeded_;
  StrictMock<SyncManagerObserverMock> manager_observer_;
  StrictMock<SyncEncryptionHandlerObserverMock> encryption_observer_;
  EngineComponentsFactory::Switches switches_;
  EngineComponentsFactory::StorageOption storage_used_;
  MockUnrecoverableErrorHandler mock_unrecoverable_error_handler_;
};

TEST_F(SyncManagerTest, RefreshEncryptionReady) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));

  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  const ModelTypeSet encrypted_types = GetEncryptedTypes();
  EXPECT_TRUE(encrypted_types.Has(PASSWORDS));
  EXPECT_FALSE(IsEncryptEverythingEnabledForTest());

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(GetIdForDataType(NIGORI)));
    sync_pb::NigoriSpecifics nigori = node.GetNigoriSpecifics();
    EXPECT_TRUE(nigori.has_encryption_keybag());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->CanDecrypt(nigori.encryption_keybag()));
  }
}

// Attempt to refresh encryption when nigori not downloaded.
TEST_F(SyncManagerTest, RefreshEncryptionNotReady) {
  // Don't set up encryption (no nigori node created).

  // Should fail. Triggers an OnPassphraseRequired because the cryptographer
  // is not ready.
  EXPECT_CALL(encryption_observer_, OnPassphraseRequired(_, _, _)).Times(1);
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  const ModelTypeSet encrypted_types = GetEncryptedTypes();
  EXPECT_TRUE(encrypted_types.Has(PASSWORDS));  // Hardcoded.
  EXPECT_FALSE(IsEncryptEverythingEnabledForTest());
}

// Attempt to refresh encryption when nigori is empty.
TEST_F(SyncManagerTest, RefreshEncryptionEmptyNigori) {
  EXPECT_TRUE(SetUpEncryption(DONT_WRITE_NIGORI, DEFAULT_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete()).Times(1);
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));

  // Should write to nigori.
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  const ModelTypeSet encrypted_types = GetEncryptedTypes();
  EXPECT_TRUE(encrypted_types.Has(PASSWORDS));  // Hardcoded.
  EXPECT_FALSE(IsEncryptEverythingEnabledForTest());

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(GetIdForDataType(NIGORI)));
    sync_pb::NigoriSpecifics nigori = node.GetNigoriSpecifics();
    EXPECT_TRUE(nigori.has_encryption_keybag());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->CanDecrypt(nigori.encryption_keybag()));
  }
}

TEST_F(SyncManagerTest, EncryptDataTypesWithNoData) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  EXPECT_CALL(
      encryption_observer_,
      OnEncryptedTypesChanged(HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();
  EXPECT_TRUE(IsEncryptEverythingEnabledForTest());
}

TEST_F(SyncManagerTest, EncryptDataTypesWithData) {
  size_t batch_size = 5;
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));

  // Create some unencrypted unsynced data.
  int64_t folder = MakeFolderWithParent(sync_manager_.GetUserShare(), BOOKMARKS,
                                        GetIdForDataType(BOOKMARKS), nullptr);
  // First batch_size nodes are children of folder.
  size_t i;
  for (i = 0; i < batch_size; ++i) {
    MakeBookmarkWithParent(sync_manager_.GetUserShare(), folder, nullptr);
  }
  // Next batch_size nodes are a different type and on their own.
  for (; i < 2 * batch_size; ++i) {
    MakeNodeWithRoot(sync_manager_.GetUserShare(), SESSIONS,
                     base::StringPrintf("%" PRIuS "", i));
  }
  // Last batch_size nodes are a third type that will not need encryption.
  for (; i < 3 * batch_size; ++i) {
    MakeNodeWithRoot(sync_manager_.GetUserShare(), THEMES,
                     base::StringPrintf("%" PRIuS "", i));
  }

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_EQ(SyncEncryptionHandler::SensitiveTypes(),
              GetEncryptedTypesWithTrans(&trans));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), BOOKMARKS, false /* not encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), SESSIONS, false /* not encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), THEMES, false /* not encrypted */));
  }

  EXPECT_CALL(
      encryption_observer_,
      OnEncryptedTypesChanged(HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();
  EXPECT_TRUE(IsEncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_EQ(EncryptableUserTypes(), GetEncryptedTypesWithTrans(&trans));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), BOOKMARKS, true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), SESSIONS, true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), THEMES, true /* is encrypted */));
  }

  // Trigger's a ReEncryptEverything with new passphrase.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  ExpectPassphraseAcceptance();
  SetCustomPassphraseAndCheck("new_passphrase");
  EXPECT_TRUE(IsEncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_EQ(EncryptableUserTypes(), GetEncryptedTypesWithTrans(&trans));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), BOOKMARKS, true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), SESSIONS, true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), THEMES, true /* is encrypted */));
  }
  // Calling EncryptDataTypes with an empty encrypted types should not trigger
  // a reencryption and should just notify immediately.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .Times(0);
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted()).Times(0);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete()).Times(0);
  sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();
}

// Sets a new explicit passphrase. This should update the bootstrap token
// and re-encrypt everything.
// (case 2 in SyncManager::SyncInternal::SetEncryptionPassphrase)
TEST_F(SyncManagerTest, SetPassphraseWithPassword) {
  Cryptographer verifier(&encryptor_);
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    // Store the default (soon to be old) key.
    Cryptographer* cryptographer = trans.GetCryptographer();
    std::string bootstrap_token;
    cryptographer->GetBootstrapToken(&bootstrap_token);
    verifier.Bootstrap(bootstrap_token);

    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS, root_node, "foo");
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    password_node.SetPasswordSpecifics(data);
  }
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  ExpectPassphraseAcceptance();
  SetCustomPassphraseAndCheck("new_passphrase");
  EXPECT_FALSE(IsEncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    // Verify the default key has changed.
    sync_pb::EncryptedData encrypted;
    cryptographer->GetKeys(&encrypted);
    EXPECT_FALSE(verifier.CanDecrypt(encrypted));

    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              password_node.InitByClientTagLookup(PASSWORDS, "foo"));
    const sync_pb::PasswordSpecificsData& data =
        password_node.GetPasswordSpecifics();
    EXPECT_EQ("secret", data.password_value());
  }
}

// Manually set the pending keys in the cryptographer/nigori to reflect the data
// being encrypted with a new (unprovided) GAIA password, then supply the
// password.
// (case 7 in SyncManager::SyncInternal::SetDecryptionPassphrase)
TEST_F(SyncManagerTest, SupplyPendingGAIAPass) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  Cryptographer other_cryptographer(&encryptor_);
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    std::string bootstrap_token;
    cryptographer->GetBootstrapToken(&bootstrap_token);
    other_cryptographer.Bootstrap(bootstrap_token);

    // Now update the nigori to reflect the new keys, and update the
    // cryptographer to have pending keys.
    KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), "passphrase2"};
    other_cryptographer.AddKey(params);
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitTypeRoot(NIGORI));
    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    cryptographer->SetPendingKeys(nigori.encryption_keybag());
    EXPECT_TRUE(cryptographer->has_pending_keys());
    node.SetNigoriSpecifics(nigori);
  }
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  ExpectPassphraseAcceptance();
  sync_manager_.GetEncryptionHandler()->SetDecryptionPassphrase("passphrase2");
  EXPECT_EQ(PassphraseType::IMPLICIT_PASSPHRASE, GetPassphraseType());
  EXPECT_FALSE(IsEncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    // Verify we're encrypting with the new key.
    sync_pb::EncryptedData encrypted;
    cryptographer->GetKeys(&encrypted);
    EXPECT_TRUE(other_cryptographer.CanDecrypt(encrypted));
  }
}

// Manually set the pending keys in the cryptographer/nigori to reflect the data
// being encrypted with an explicit (unprovided) passphrase, then supply the
// passphrase.
// (case 9 in SyncManager::SyncInternal::SetDecryptionPassphrase)
TEST_F(SyncManagerTest, SupplyPendingExplicitPass) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  Cryptographer other_cryptographer(&encryptor_);
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    std::string bootstrap_token;
    cryptographer->GetBootstrapToken(&bootstrap_token);
    other_cryptographer.Bootstrap(bootstrap_token);

    // Now update the nigori to reflect the new keys, and update the
    // cryptographer to have pending keys.
    KeyParams params = {KeyDerivationParams::CreateForPbkdf2(), "explicit"};
    other_cryptographer.AddKey(params);
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitTypeRoot(NIGORI));
    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    cryptographer->SetPendingKeys(nigori.encryption_keybag());
    EXPECT_TRUE(cryptographer->has_pending_keys());
    nigori.set_keybag_is_frozen(true);
    node.SetNigoriSpecifics(nigori);
  }
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_,
              OnPassphraseTypeChanged(PassphraseType::CUSTOM_PASSPHRASE, _));
  EXPECT_CALL(encryption_observer_, OnPassphraseRequired(_, _, _));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  sync_manager_.GetEncryptionHandler()->Init();
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  ExpectPassphraseAcceptance();
  sync_manager_.GetEncryptionHandler()->SetDecryptionPassphrase("explicit");
  EXPECT_EQ(PassphraseType::CUSTOM_PASSPHRASE, GetPassphraseType());
  EXPECT_FALSE(IsEncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    // Verify we're encrypting with the new key.
    sync_pb::EncryptedData encrypted;
    cryptographer->GetKeys(&encrypted);
    EXPECT_TRUE(other_cryptographer.CanDecrypt(encrypted));
  }
}

TEST_F(SyncManagerTest, SetPassphraseWithEmptyPasswordNode) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  int64_t node_id = 0;
  std::string tag = "foo";
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS, root_node, tag);
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    node_id = password_node.GetId();
  }
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  ExpectPassphraseAcceptance();
  SetCustomPassphraseAndCheck("new_passphrase");
  EXPECT_FALSE(IsEncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY,
              password_node.InitByClientTagLookup(PASSWORDS, tag));
  }
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY,
              password_node.InitByIdLookup(node_id));
  }
}

// Friended by WriteNode, so can't be in an anonymouse namespace.
TEST_F(SyncManagerTest, EncryptBookmarksWithLegacyData) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  std::string title;
  SyncAPINameToServerName("Google", &title);
  std::string url = "http://www.google.com";
  std::string raw_title2 = "..";  // An invalid cosmo title.
  std::string title2;
  SyncAPINameToServerName(raw_title2, &title2);
  std::string url2 = "http://www.bla.com";

  // Create a bookmark using the legacy format.
  int64_t node_id1 =
      MakeNodeWithRoot(sync_manager_.GetUserShare(), BOOKMARKS, "testtag");
  int64_t node_id2 =
      MakeNodeWithRoot(sync_manager_.GetUserShare(), BOOKMARKS, "testtag2");
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(node_id1));

    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.mutable_bookmark()->set_url(url);
    node.SetEntitySpecifics(entity_specifics);

    // Set the old style title.
    syncable::MutableEntry* node_entry = node.entry_;
    node_entry->PutNonUniqueName(title);

    WriteNode node2(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node2.InitByIdLookup(node_id2));

    sync_pb::EntitySpecifics entity_specifics2;
    entity_specifics2.mutable_bookmark()->set_url(url2);
    node2.SetEntitySpecifics(entity_specifics2);

    // Set the old style title.
    syncable::MutableEntry* node_entry2 = node2.entry_;
    node_entry2->PutNonUniqueName(title2);
  }

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(node_id1));
    EXPECT_EQ(BOOKMARKS, node.GetModelType());
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(title, node.GetBookmarkSpecifics().title());
    EXPECT_EQ(url, node.GetBookmarkSpecifics().url());

    ReadNode node2(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node2.InitByIdLookup(node_id2));
    EXPECT_EQ(BOOKMARKS, node2.GetModelType());
    // We should de-canonicalize the title in GetTitle(), but the title in the
    // specifics should be stored in the server legal form.
    EXPECT_EQ(raw_title2, node2.GetTitle());
    EXPECT_EQ(title2, node2.GetBookmarkSpecifics().title());
    EXPECT_EQ(url2, node2.GetBookmarkSpecifics().url());
  }

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), BOOKMARKS, false /* not encrypted */));
  }

  EXPECT_CALL(
      encryption_observer_,
      OnEncryptedTypesChanged(HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();
  EXPECT_TRUE(IsEncryptEverythingEnabledForTest());

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_EQ(EncryptableUserTypes(), GetEncryptedTypesWithTrans(&trans));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(), BOOKMARKS, true /* is encrypted */));

    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(node_id1));
    EXPECT_EQ(BOOKMARKS, node.GetModelType());
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(title, node.GetBookmarkSpecifics().title());
    EXPECT_EQ(url, node.GetBookmarkSpecifics().url());

    ReadNode node2(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node2.InitByIdLookup(node_id2));
    EXPECT_EQ(BOOKMARKS, node2.GetModelType());
    // We should de-canonicalize the title in GetTitle(), but the title in the
    // specifics should be stored in the server legal form.
    EXPECT_EQ(raw_title2, node2.GetTitle());
    EXPECT_EQ(title2, node2.GetBookmarkSpecifics().title());
    EXPECT_EQ(url2, node2.GetBookmarkSpecifics().url());
  }
}

// Create a bookmark and set the title/url, then verify the data was properly
// set. This replicates the unique way bookmarks have of creating sync nodes.
// See BookmarkChangeProcessor::PlaceSyncNode(..).
TEST_F(SyncManagerTest, CreateLocalBookmark) {
  std::string title = "title";
  std::string url = "url";
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode bookmark_root(&trans);
    ASSERT_EQ(BaseNode::INIT_OK, bookmark_root.InitTypeRoot(BOOKMARKS));
    WriteNode node(&trans);
    ASSERT_TRUE(node.InitBookmarkByCreation(bookmark_root, nullptr));
    node.SetIsFolder(false);
    node.SetTitle(title);

    sync_pb::BookmarkSpecifics bookmark_specifics(node.GetBookmarkSpecifics());
    bookmark_specifics.set_url(url);
    node.SetBookmarkSpecifics(bookmark_specifics);
  }
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode bookmark_root(&trans);
    ASSERT_EQ(BaseNode::INIT_OK, bookmark_root.InitTypeRoot(BOOKMARKS));
    int64_t child_id = bookmark_root.GetFirstChildId();

    ReadNode node(&trans);
    ASSERT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(child_id));
    EXPECT_FALSE(node.GetIsFolder());
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(url, node.GetBookmarkSpecifics().url());
  }
}

// Verifies WriteNode::UpdateEntryWithEncryption does not make unnecessary
// changes.
TEST_F(SyncManagerTest, UpdateEntryWithEncryption) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("url");
  entity_specifics.mutable_bookmark()->set_title("title");
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 GenerateSyncableHash(BOOKMARKS, client_tag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));
  // Manually change to the same data. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetEntitySpecifics(entity_specifics);
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Encrypt the datatatype, should set is_unsynced.
  EXPECT_CALL(
      encryption_observer_,
      OnEncryptedTypesChanged(HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, FULL_ENCRYPTION));

  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(
        cryptographer->CanDecryptUsingDefaultKey(specifics.encrypted()));
  }
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Set a new passphrase. Should set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  ExpectPassphraseAcceptance();
  SetCustomPassphraseAndCheck("new_passphrase");
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(
        cryptographer->CanDecryptUsingDefaultKey(specifics.encrypted()));
  }
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Force a re-encrypt everything. Should not set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));

  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(
        cryptographer->CanDecryptUsingDefaultKey(specifics.encrypted()));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to the same data. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetEntitySpecifics(entity_specifics);
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_FALSE(node_entry->GetIsUnsynced());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(
        cryptographer->CanDecryptUsingDefaultKey(specifics.encrypted()));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to different data. Should set is_unsynced.
  {
    entity_specifics.mutable_bookmark()->set_url("url2");
    entity_specifics.mutable_bookmark()->set_title("title2");
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetEntitySpecifics(entity_specifics);
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_TRUE(node_entry->GetIsUnsynced());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(
        cryptographer->CanDecryptUsingDefaultKey(specifics.encrypted()));
  }
}

// Passwords have their own handling for encryption. Verify it does not result
// in unnecessary writes via SetEntitySpecifics.
TEST_F(SyncManagerTest, UpdatePasswordSetEntitySpecificsNoChange) {
  std::string client_tag = "title";
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    cryptographer->Encrypt(
        data, entity_specifics.mutable_password()->mutable_encrypted());
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, client_tag,
                 GenerateSyncableHash(PASSWORDS, client_tag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));

  // Manually change to the same data via SetEntitySpecifics. Should not set
  // is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PASSWORDS, client_tag));
    node.SetEntitySpecifics(entity_specifics);
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));
}

// Passwords have their own handling for encryption. Verify it does not result
// in unnecessary writes via SetPasswordSpecifics.
TEST_F(SyncManagerTest, UpdatePasswordSetPasswordSpecifics) {
  std::string client_tag = "title";
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    cryptographer->Encrypt(
        data, entity_specifics.mutable_password()->mutable_encrypted());
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, client_tag,
                 GenerateSyncableHash(PASSWORDS, client_tag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));

  // Manually change to the same data via SetPasswordSpecifics. Should not set
  // is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PASSWORDS, client_tag));
    node.SetPasswordSpecifics(node.GetPasswordSpecifics());
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));

  // Manually change to different data. Should set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PASSWORDS, client_tag));
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret2");
    cryptographer->Encrypt(
        data, entity_specifics.mutable_password()->mutable_encrypted());
    node.SetPasswordSpecifics(data);
    const syncable::Entry* node_entry = node.GetEntry();
    EXPECT_TRUE(node_entry->GetIsUnsynced());
  }
}

// Passwords have their own handling for encryption. Verify setting a new
// passphrase updates the data and clears the unencrypted metadta for passwords.
TEST_F(SyncManagerTest, UpdatePasswordNewPassphrase) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value(kPasswordValue);
    entity_specifics.mutable_password()
        ->mutable_unencrypted_metadata()
        ->set_url(kUrl);
    cryptographer->Encrypt(
        data, entity_specifics.mutable_password()->mutable_encrypted());
  }
  EXPECT_TRUE(entity_specifics.password().has_unencrypted_metadata());
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, kClientTag,
                 GenerateSyncableHash(PASSWORDS, kClientTag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, kClientTag));

  // Set a new passphrase. Should set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  ExpectPassphraseAcceptance();
  SetCustomPassphraseAndCheck("new_passphrase");
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              password_node.InitByClientTagLookup(PASSWORDS, kClientTag));
    const sync_pb::PasswordSpecificsData& data =
        password_node.GetPasswordSpecifics();
    EXPECT_EQ(kPasswordValue, data.password_value());
    EXPECT_FALSE(password_node.GetEntitySpecifics()
                     .password()
                     .has_unencrypted_metadata());
  }
  EXPECT_TRUE(ResetUnsyncedEntry(PASSWORDS, kClientTag));

  // Check that writing new password doesn't set the metadata.
  const std::string tag = "newpassentity";
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS, root_node, tag);
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    sync_pb::PasswordSpecificsData data;
    data.set_password_value(kPasswordValue);
    data.set_signon_realm(kUrl);
    password_node.SetPasswordSpecifics(data);
  }
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              password_node.InitByClientTagLookup(PASSWORDS, tag));
    const sync_pb::PasswordSpecificsData& data =
        password_node.GetPasswordSpecifics();
    EXPECT_EQ(kPasswordValue, data.password_value());
    EXPECT_FALSE(password_node.GetEntitySpecifics()
                     .password()
                     .has_unencrypted_metadata());
  }
}

// Passwords have their own handling for encryption. Verify it does not result
// in unnecessary writes via ReencryptEverything.
TEST_F(SyncManagerTest, UpdatePasswordReencryptEverything) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    cryptographer->Encrypt(
        data, entity_specifics.mutable_password()->mutable_encrypted());
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, kClientTag,
                 GenerateSyncableHash(PASSWORDS, kClientTag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, kClientTag));

  // Force a re-encrypt everything. Should not set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, kClientTag));
}

// Metadata filling can happen during ReencryptEverything, check that data is
// written when it's applicable, namely that password specifics entity is marked
// unsynced, when data was written to the unencrypted metadata field.
TEST_F(SyncManagerTest, UpdatePasswordReencryptEverythingFillMetadata) {
  base::FieldTrialList field_trial_list(nullptr);

  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    data.set_signon_realm(kUrl);
    cryptographer->Encrypt(
        data, entity_specifics.mutable_password()->mutable_encrypted());
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, kClientTag,
                 GenerateSyncableHash(PASSWORDS, kClientTag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, kClientTag));

  // Force a re-encrypt everything. Should set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  // Check that unencrypted metadata field was set.
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              password_node.InitByClientTagLookup(PASSWORDS, kClientTag));
    EXPECT_EQ(kUrl, password_node.GetEntitySpecifics()
                        .password()
                        .unencrypted_metadata()
                        .url());
  }

  EXPECT_TRUE(ResetUnsyncedEntry(PASSWORDS, kClientTag));
}

// Check that when the data in PasswordSpecifics hasn't changed during
// ReEncryption, entity is not marked as unsynced.
TEST_F(SyncManagerTest,
       UpdatePasswordReencryptEverythingDontMarkUnsyncWhenNotNeeded) {
  base::FieldTrialList field_trial_list(nullptr);

  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    data.set_signon_realm(kUrl);
    cryptographer->Encrypt(
        data, entity_specifics.mutable_password()->mutable_encrypted());
  }
  entity_specifics.mutable_password()->mutable_unencrypted_metadata()->set_url(
      kUrl);
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, kClientTag,
                 GenerateSyncableHash(PASSWORDS, kClientTag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, kClientTag));

  // Force a re-encrypt everything. Should not set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, kClientTag));
}

// Test that attempting to start up with corrupted password data triggers
// an unrecoverable error (rather than crashing).
TEST_F(SyncManagerTest, ReencryptEverythingWithUnrecoverableErrorPasswords) {
  const char kClientTag[] = "client_tag";

  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    // Create a synced bookmark with undecryptable data.
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());

    Cryptographer other_cryptographer(&encryptor_);
    KeyParams fake_params = {KeyDerivationParams::CreateForPbkdf2(),
                             "fake_key"};
    other_cryptographer.AddKey(fake_params);
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    other_cryptographer.Encrypt(
        data, entity_specifics.mutable_password()->mutable_encrypted());

    // Set up the real cryptographer with a different key.
    KeyParams real_params = {KeyDerivationParams::CreateForPbkdf2(),
                             "real_key"};
    trans.GetCryptographer()->AddKey(real_params);
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, kClientTag,
                 GenerateSyncableHash(PASSWORDS, kClientTag), entity_specifics);
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, kClientTag));

  // Force a re-encrypt everything. Should trigger an unrecoverable error due
  // to being unable to decrypt the data that was previously applied.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  EXPECT_FALSE(HasUnrecoverableError());
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_TRUE(HasUnrecoverableError());
}

// Test that attempting to start up with corrupted bookmark data triggers
// an unrecoverable error (rather than crashing).
TEST_F(SyncManagerTest, ReencryptEverythingWithUnrecoverableErrorBookmarks) {
  const char kClientTag[] = "client_tag";
  EXPECT_CALL(
      encryption_observer_,
      OnEncryptedTypesChanged(HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, FULL_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    // Create a synced bookmark with undecryptable data.
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());

    Cryptographer other_cryptographer(&encryptor_);
    KeyParams fake_params = {KeyDerivationParams::CreateForPbkdf2(),
                             "fake_key"};
    other_cryptographer.AddKey(fake_params);
    sync_pb::EntitySpecifics bm_specifics;
    bm_specifics.mutable_bookmark()->set_title("title");
    bm_specifics.mutable_bookmark()->set_url("url");
    sync_pb::EncryptedData encrypted;
    other_cryptographer.Encrypt(bm_specifics, &encrypted);
    entity_specifics.mutable_encrypted()->CopyFrom(encrypted);

    // Set up the real cryptographer with a different key.
    KeyParams real_params = {KeyDerivationParams::CreateForPbkdf2(),
                             "real_key"};
    trans.GetCryptographer()->AddKey(real_params);
  }
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, kClientTag,
                 GenerateSyncableHash(BOOKMARKS, kClientTag), entity_specifics);
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, kClientTag));

  // Force a re-encrypt everything. Should trigger an unrecoverable error due
  // to being unable to decrypt the data that was previously applied.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));
  EXPECT_FALSE(HasUnrecoverableError());
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_TRUE(HasUnrecoverableError());
}

// Verify SetTitle(..) doesn't unnecessarily set IS_UNSYNCED for bookmarks
// when we write the same data, but does set it when we write new data.
TEST_F(SyncManagerTest, SetBookmarkTitle) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("url");
  entity_specifics.mutable_bookmark()->set_title("title");
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 GenerateSyncableHash(BOOKMARKS, client_tag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to the same title. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetTitle(client_tag);
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to new title. Should set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetTitle("title2");
  }
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));
}

// Verify SetTitle(..) doesn't unnecessarily set IS_UNSYNCED for encrypted
// bookmarks when we write the same data, but does set it when we write new
// data.
TEST_F(SyncManagerTest, SetBookmarkTitleWithEncryption) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("url");
  entity_specifics.mutable_bookmark()->set_title("title");
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 GenerateSyncableHash(BOOKMARKS, client_tag), entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Encrypt the datatatype, should set is_unsynced.
  EXPECT_CALL(
      encryption_observer_,
      OnEncryptedTypesChanged(HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, FULL_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to the same title. Should not set is_unsynced.
  // NON_UNIQUE_NAME should be kEncryptedString.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetTitle(client_tag);
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to new title. Should set is_unsynced. NON_UNIQUE_NAME
  // should still be kEncryptedString.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetTitle("title2");
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
  }
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));
}

// Verify SetTitle(..) doesn't unnecessarily set IS_UNSYNCED for non-bookmarks
// when we write the same data, but does set it when we write new data.
TEST_F(SyncManagerTest, SetNonBookmarkTitle) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_preference()->set_name("name");
  entity_specifics.mutable_preference()->set_value("value");
  MakeServerNode(sync_manager_.GetUserShare(), PREFERENCES, client_tag,
                 GenerateSyncableHash(PREFERENCES, client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Manually change to the same title. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, client_tag));
    node.SetTitle(client_tag);
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Manually change to new title. Should set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, client_tag));
    node.SetTitle("title2");
  }
  EXPECT_TRUE(ResetUnsyncedEntry(PREFERENCES, client_tag));
}

// Verify SetTitle(..) doesn't unnecessarily set IS_UNSYNCED for encrypted
// non-bookmarks when we write the same data or when we write new data
// data (should remained kEncryptedString).
TEST_F(SyncManagerTest, SetNonBookmarkTitleWithEncryption) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_preference()->set_name("name");
  entity_specifics.mutable_preference()->set_value("value");
  MakeServerNode(sync_manager_.GetUserShare(), PREFERENCES, client_tag,
                 GenerateSyncableHash(PREFERENCES, client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Encrypt the datatatype, should set is_unsynced.
  EXPECT_CALL(
      encryption_observer_,
      OnEncryptedTypesChanged(HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, FULL_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_TRUE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Manually change to the same title. Should not set is_unsynced.
  // NON_UNIQUE_NAME should be kEncryptedString.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, client_tag));
    node.SetTitle(client_tag);
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Manually change to new title. Should not set is_unsynced because the
  // NON_UNIQUE_NAME should still be kEncryptedString.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, client_tag));
    node.SetTitle("title2");
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
    EXPECT_FALSE(node_entry->GetIsUnsynced());
  }
}

// Ensure that titles are truncated to 255 bytes, and attempting to reset
// them to their longer version does not set IS_UNSYNCED.
TEST_F(SyncManagerTest, SetLongTitle) {
  const int kNumChars = 512;
  const std::string kClientTag = "tag";
  std::string title(kNumChars, '0');
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_preference()->set_name("name");
  entity_specifics.mutable_preference()->set_value("value");
  MakeServerNode(sync_manager_.GetUserShare(), PREFERENCES, "short_title",
                 GenerateSyncableHash(PREFERENCES, kClientTag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, kClientTag));

  // Manually change to the long title. Should set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, kClientTag));
    node.SetTitle(title);
    EXPECT_EQ(node.GetTitle(), title.substr(0, 255));
  }
  EXPECT_TRUE(ResetUnsyncedEntry(PREFERENCES, kClientTag));

  // Manually change to the same title. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, kClientTag));
    node.SetTitle(title);
    EXPECT_EQ(node.GetTitle(), title.substr(0, 255));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, kClientTag));

  // Manually change to new title. Should set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, kClientTag));
    node.SetTitle("title2");
  }
  EXPECT_TRUE(ResetUnsyncedEntry(PREFERENCES, kClientTag));
}

// Create an encrypted entry when the cryptographer doesn't think the type is
// marked for encryption. Ensure reads/writes don't break and don't unencrypt
// the data.
TEST_F(SyncManagerTest, SetPreviouslyEncryptedSpecifics) {
  std::string client_tag = "tag";
  std::string url = "url";
  std::string url2 = "new_url";
  std::string title = "title";
  sync_pb::EntitySpecifics entity_specifics;
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* crypto = trans.GetCryptographer();
    sync_pb::EntitySpecifics bm_specifics;
    bm_specifics.mutable_bookmark()->set_title("title");
    bm_specifics.mutable_bookmark()->set_url("url");
    sync_pb::EncryptedData encrypted;
    crypto->Encrypt(bm_specifics, &encrypted);
    entity_specifics.mutable_encrypted()->CopyFrom(encrypted);
    AddDefaultFieldValue(BOOKMARKS, &entity_specifics);
  }
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 GenerateSyncableHash(BOOKMARKS, client_tag), entity_specifics);

  {
    // Verify the data.
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(url, node.GetBookmarkSpecifics().url());
  }

  {
    // Overwrite the url (which overwrites the specifics).
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));

    sync_pb::BookmarkSpecifics bookmark_specifics(node.GetBookmarkSpecifics());
    bookmark_specifics.set_url(url2);
    node.SetBookmarkSpecifics(bookmark_specifics);
  }

  {
    // Verify it's still encrypted and it has the most recent url.
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(url2, node.GetBookmarkSpecifics().url());
    const syncable::Entry* node_entry = node.GetEntry();
    EXPECT_EQ(kEncryptedString, node_entry->GetNonUniqueName());
    const sync_pb::EntitySpecifics& specifics = node_entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_encrypted());
  }
}

// Verify transaction version of a model type is incremented when node of
// that type is updated.
TEST_F(SyncManagerTest, IncrementTransactionVersion) {
  {
    ReadTransaction read_trans(FROM_HERE, sync_manager_.GetUserShare());
    ModelTypeSet enabled_types = GetEnabledTypes();
    for (ModelType type : enabled_types) {
      // Transaction version is incremented when SyncManagerTest::SetUp()
      // creates a node of each type.
      EXPECT_EQ(
          1,
          sync_manager_.GetUserShare()->directory->GetTransactionVersion(type));
    }
  }

  // Create bookmark node to increment transaction version of bookmark model.
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("url");
  entity_specifics.mutable_bookmark()->set_title("title");
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 GenerateSyncableHash(BOOKMARKS, client_tag), entity_specifics);

  {
    ReadTransaction read_trans(FROM_HERE, sync_manager_.GetUserShare());
    ModelTypeSet enabled_types = GetEnabledTypes();
    for (ModelType type : enabled_types) {
      EXPECT_EQ(
          type == BOOKMARKS ? 2 : 1,
          sync_manager_.GetUserShare()->directory->GetTransactionVersion(type));
    }
  }
}

class SyncManagerWithLocalBackendTest : public SyncManagerTest {
 protected:
  void SetUp() override { DoSetUp(true); }
};

// This test checks that we can successfully initialize without credentials in
// the local backend case.
TEST_F(SyncManagerWithLocalBackendTest, StartSyncInLocalMode) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));

  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  const ModelTypeSet encrypted_types = GetEncryptedTypes();
  EXPECT_TRUE(encrypted_types.Has(PASSWORDS));
  EXPECT_FALSE(IsEncryptEverythingEnabledForTest());

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(GetIdForDataType(NIGORI)));
    sync_pb::NigoriSpecifics nigori = node.GetNigoriSpecifics();
    EXPECT_TRUE(nigori.has_encryption_keybag());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->CanDecrypt(nigori.encryption_keybag()));
  }
}

class MockSyncScheduler : public FakeSyncScheduler {
 public:
  MockSyncScheduler() : FakeSyncScheduler() {}
  ~MockSyncScheduler() override {}

  MOCK_METHOD2(Start, void(SyncScheduler::Mode, base::Time));
  MOCK_METHOD1(ScheduleConfiguration, void(const ConfigurationParams&));
};

class ComponentsFactory : public TestEngineComponentsFactory {
 public:
  ComponentsFactory(const Switches& switches,
                    SyncScheduler* scheduler_to_use,
                    SyncCycleContext** cycle_context,
                    EngineComponentsFactory::StorageOption* storage_used)
      : TestEngineComponentsFactory(switches,
                                    EngineComponentsFactory::STORAGE_IN_MEMORY,
                                    storage_used),
        scheduler_to_use_(scheduler_to_use),
        cycle_context_(cycle_context) {}
  ~ComponentsFactory() override {}

  std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* stop_handle,
      bool local_sync_backend_enabled) override {
    *cycle_context_ = context;
    return std::move(scheduler_to_use_);
  }

 private:
  std::unique_ptr<SyncScheduler> scheduler_to_use_;
  SyncCycleContext** cycle_context_;
};

class SyncManagerTestWithMockScheduler : public SyncManagerTest {
 public:
  SyncManagerTestWithMockScheduler() : scheduler_(nullptr) {}
  EngineComponentsFactory* GetFactory() override {
    scheduler_ = new MockSyncScheduler();
    return new ComponentsFactory(GetSwitches(), scheduler_, &cycle_context_,
                                 &storage_used_);
  }

  MockSyncScheduler* scheduler() { return scheduler_; }
  SyncCycleContext* cycle_context() { return cycle_context_; }

 private:
  MockSyncScheduler* scheduler_;
  SyncCycleContext* cycle_context_;
};

// Test that the configuration params are properly created and sent to
// ScheduleConfigure. No callback should be invoked.
TEST_F(SyncManagerTestWithMockScheduler, BasicConfiguration) {
  ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION;
  ModelTypeSet types_to_download(BOOKMARKS, PREFERENCES);

  ConfigurationParams params;
  EXPECT_CALL(*scheduler(), Start(SyncScheduler::CONFIGURATION_MODE, _));
  EXPECT_CALL(*scheduler(), ScheduleConfiguration(_))
      .WillOnce(SaveArg<0>(&params));

  CallbackCounter ready_task_counter, retry_task_counter;
  sync_manager_.ConfigureSyncer(
      reason, types_to_download, SyncManager::SyncFeatureState::ON,
      base::Bind(&CallbackCounter::Callback,
                 base::Unretained(&ready_task_counter)),
      base::Bind(&CallbackCounter::Callback,
                 base::Unretained(&retry_task_counter)));
  EXPECT_EQ(0, ready_task_counter.times_called());
  EXPECT_EQ(0, retry_task_counter.times_called());
  EXPECT_EQ(sync_pb::SyncEnums::RECONFIGURATION, params.origin);
  EXPECT_EQ(types_to_download, params.types_to_download);
}

// Test that PurgeDisabledTypes only purges recently disabled types leaving
// others intact.
TEST_F(SyncManagerTestWithMockScheduler, PurgeDisabledTypes) {
  ModelTypeSet enabled_types = GetEnabledTypes();
  ModelTypeSet disabled_types = Difference(ModelTypeSet::All(), enabled_types);
  // Set data for all types.
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    SetProgressMarkerForType(type, true);
  }

  sync_manager_.PurgeDisabledTypes(disabled_types, ModelTypeSet(),
                                   ModelTypeSet());
  // Verify all the disabled types were purged.
  EXPECT_EQ(enabled_types,
            sync_manager_.GetUserShare()->directory->InitialSyncEndedTypes());
  EXPECT_EQ(disabled_types, sync_manager_.GetTypesWithEmptyProgressMarkerToken(
                                ModelTypeSet::All()));
}

// Test that SyncManager::ClearServerData invokes the scheduler.
TEST_F(SyncManagerTestWithMockScheduler, ClearServerData) {
  EXPECT_CALL(*scheduler(), Start(SyncScheduler::CLEAR_SERVER_DATA_MODE, _));
  CallbackCounter callback_counter;
  sync_manager_.ClearServerData(base::Bind(
      &CallbackCounter::Callback, base::Unretained(&callback_counter)));
  PumpLoop();
  EXPECT_EQ(1, callback_counter.times_called());
}

// Test that PurgePartiallySyncedTypes purges only those types that have not
// fully completed their initial download and apply.
TEST_F(SyncManagerTest, PurgePartiallySyncedTypes) {
  ModelTypeSet enabled_types = GetEnabledTypes();

  UserShare* share = sync_manager_.GetUserShare();

  // The test harness automatically initializes all types in the routing info.
  // Check that autofill is not among them.
  ASSERT_FALSE(enabled_types.Has(AUTOFILL));

  // Further ensure that the test harness did not create its root node.
  {
    syncable::ReadTransaction trans(FROM_HERE, share->directory.get());
    syncable::Entry autofill_root_node(&trans, syncable::GET_TYPE_ROOT,
                                       AUTOFILL);
    ASSERT_FALSE(autofill_root_node.good());
  }

  // One more redundant check.
  ASSERT_FALSE(
      sync_manager_.GetUserShare()->directory->InitialSyncEndedTypes().Has(
          AUTOFILL));

  // Give autofill a progress marker.
  sync_pb::DataTypeProgressMarker autofill_marker;
  autofill_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(AUTOFILL));
  autofill_marker.set_token("token");
  share->directory->SetDownloadProgress(AUTOFILL, autofill_marker);

  // Also add a pending autofill root node update from the server.
  TestEntryFactory factory_(share->directory.get());
  int autofill_meta = factory_.CreateUnappliedRootNode(AUTOFILL);

  // Preferences is an enabled type.  Check that the harness initialized it.
  ASSERT_TRUE(enabled_types.Has(PREFERENCES));
  ASSERT_TRUE(
      sync_manager_.GetUserShare()->directory->InitialSyncEndedTypes().Has(
          PREFERENCES));

  // Give preferencse a progress marker.
  sync_pb::DataTypeProgressMarker prefs_marker;
  prefs_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(PREFERENCES));
  prefs_marker.set_token("token");
  share->directory->SetDownloadProgress(PREFERENCES, prefs_marker);

  // Add a fully synced preferences node under the root.
  std::string pref_client_tag = "prefABC";
  std::string pref_hashed_tag = "hashXYZ";
  sync_pb::EntitySpecifics pref_specifics;
  AddDefaultFieldValue(PREFERENCES, &pref_specifics);
  int pref_meta = MakeServerNode(share, PREFERENCES, pref_client_tag,
                                 pref_hashed_tag, pref_specifics);

  // And now, the purge.
  sync_manager_.PurgePartiallySyncedTypes();

  // Ensure that autofill lost its progress marker, but preferences did not.
  ModelTypeSet empty_tokens =
      sync_manager_.GetTypesWithEmptyProgressMarkerToken(ModelTypeSet::All());
  EXPECT_TRUE(empty_tokens.Has(AUTOFILL));
  EXPECT_FALSE(empty_tokens.Has(PREFERENCES));

  // Ensure that autofill lost its node, but preferences did not.
  {
    syncable::ReadTransaction trans(FROM_HERE, share->directory.get());
    syncable::Entry autofill_node(&trans, GET_BY_HANDLE, autofill_meta);
    syncable::Entry pref_node(&trans, GET_BY_HANDLE, pref_meta);
    EXPECT_FALSE(autofill_node.good());
    EXPECT_TRUE(pref_node.good());
  }
}

// Test CleanupDisabledTypes properly purges all disabled types as specified
// by the previous and current enabled params.
TEST_F(SyncManagerTest, PurgeDisabledTypes) {
  ModelTypeSet enabled_types = GetEnabledTypes();
  ModelTypeSet disabled_types = Difference(ModelTypeSet::All(), enabled_types);

  // The harness should have initialized the enabled_types for us.
  EXPECT_EQ(enabled_types,
            sync_manager_.GetUserShare()->directory->InitialSyncEndedTypes());

  // Set progress markers for all types.
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    SetProgressMarkerForType(type, true);
  }

  // Verify all the enabled types remain after cleanup, and all the disabled
  // types were purged.
  sync_manager_.PurgeDisabledTypes(disabled_types, ModelTypeSet(),
                                   ModelTypeSet());
  EXPECT_EQ(enabled_types,
            sync_manager_.GetUserShare()->directory->InitialSyncEndedTypes());
  EXPECT_EQ(disabled_types, sync_manager_.GetTypesWithEmptyProgressMarkerToken(
                                ModelTypeSet::All()));

  // Disable some more types.
  disabled_types.Put(BOOKMARKS);
  disabled_types.Put(PREFERENCES);
  ModelTypeSet new_enabled_types =
      Difference(ModelTypeSet::All(), disabled_types);

  // Verify only the non-disabled types remain after cleanup.
  sync_manager_.PurgeDisabledTypes(disabled_types, ModelTypeSet(),
                                   ModelTypeSet());
  EXPECT_EQ(new_enabled_types,
            sync_manager_.GetUserShare()->directory->InitialSyncEndedTypes());
  EXPECT_EQ(disabled_types, sync_manager_.GetTypesWithEmptyProgressMarkerToken(
                                ModelTypeSet::All()));
}

// Test PurgeDisabledTypes properly unapplies types by deleting their local data
// and preserving their server data and progress marker.
TEST_F(SyncManagerTest, PurgeUnappliedTypes) {
  ModelTypeSet unapplied_types = ModelTypeSet(BOOKMARKS, PREFERENCES);
  ModelTypeSet enabled_types = GetEnabledTypes();
  ModelTypeSet disabled_types = Difference(ModelTypeSet::All(), enabled_types);

  // The harness should have initialized the enabled_types for us.
  EXPECT_EQ(enabled_types,
            sync_manager_.GetUserShare()->directory->InitialSyncEndedTypes());

  // Set progress markers for all types.
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    SetProgressMarkerForType(type, true);
  }

  // Add the following kinds of items:
  // 1. Fully synced preference.
  // 2. Locally created preference, server unknown, unsynced
  // 3. Locally deleted preference, server known, unsynced
  // 4. Server deleted preference, locally known.
  // 5. Server created preference, locally unknown, unapplied.
  // 6. A fully synced bookmark (no unique_client_tag).
  UserShare* share = sync_manager_.GetUserShare();
  sync_pb::EntitySpecifics pref_specifics;
  AddDefaultFieldValue(PREFERENCES, &pref_specifics);
  sync_pb::EntitySpecifics bm_specifics;
  AddDefaultFieldValue(BOOKMARKS, &bm_specifics);
  int pref1_meta =
      MakeServerNode(share, PREFERENCES, "pref1", "hash1", pref_specifics);
  int64_t pref2_meta = MakeNodeWithRoot(share, PREFERENCES, "pref2");
  int pref3_meta =
      MakeServerNode(share, PREFERENCES, "pref3", "hash3", pref_specifics);
  int pref4_meta =
      MakeServerNode(share, PREFERENCES, "pref4", "hash4", pref_specifics);
  int pref5_meta =
      MakeServerNode(share, PREFERENCES, "pref5", "hash5", pref_specifics);
  int bookmark_meta =
      MakeServerNode(share, BOOKMARKS, "bookmark", "", bm_specifics);

  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share->directory.get());
    // Pref's 1 and 2 are already set up properly.
    // Locally delete pref 3.
    syncable::MutableEntry pref3(&trans, GET_BY_HANDLE, pref3_meta);
    pref3.PutIsDel(true);
    pref3.PutIsUnsynced(true);
    // Delete pref 4 at the server.
    syncable::MutableEntry pref4(&trans, GET_BY_HANDLE, pref4_meta);
    pref4.PutServerIsDel(true);
    pref4.PutIsUnappliedUpdate(true);
    pref4.PutServerVersion(2);
    // Pref 5 is an new unapplied update.
    syncable::MutableEntry pref5(&trans, GET_BY_HANDLE, pref5_meta);
    pref5.PutIsUnappliedUpdate(true);
    pref5.PutIsDel(true);
    pref5.PutBaseVersion(-1);
    // Bookmark is already set up properly
  }

  // Take a snapshot to clear all the dirty bits.
  share->directory.get()->SaveChanges();

  // Now request a purge for the unapplied types.
  disabled_types.PutAll(unapplied_types);
  sync_manager_.PurgeDisabledTypes(disabled_types, ModelTypeSet(),
                                   unapplied_types);

  // Verify the unapplied types still have progress markers and initial sync
  // ended after cleanup.
  EXPECT_TRUE(
      sync_manager_.GetUserShare()->directory->InitialSyncEndedTypes().HasAll(
          unapplied_types));
  EXPECT_TRUE(
      sync_manager_.GetTypesWithEmptyProgressMarkerToken(unapplied_types)
          .Empty());

  // Ensure the items were unapplied as necessary.
  {
    syncable::ReadTransaction trans(FROM_HERE, share->directory.get());
    syncable::Entry pref_node(&trans, GET_BY_HANDLE, pref1_meta);
    ASSERT_TRUE(pref_node.good());
    EXPECT_TRUE(pref_node.GetKernelCopy().is_dirty());
    EXPECT_FALSE(pref_node.GetIsUnsynced());
    EXPECT_TRUE(pref_node.GetIsUnappliedUpdate());
    EXPECT_TRUE(pref_node.GetIsDel());
    EXPECT_GT(pref_node.GetServerVersion(), 0);
    EXPECT_EQ(pref_node.GetBaseVersion(), -1);

    // Pref 2 should just be locally deleted.
    syncable::Entry pref2_node(&trans, GET_BY_HANDLE, pref2_meta);
    ASSERT_TRUE(pref2_node.good());
    EXPECT_TRUE(pref2_node.GetKernelCopy().is_dirty());
    EXPECT_FALSE(pref2_node.GetIsUnsynced());
    EXPECT_TRUE(pref2_node.GetIsDel());
    EXPECT_FALSE(pref2_node.GetIsUnappliedUpdate());
    EXPECT_TRUE(pref2_node.GetIsDel());
    EXPECT_EQ(pref2_node.GetServerVersion(), 0);
    EXPECT_EQ(pref2_node.GetBaseVersion(), -1);

    syncable::Entry pref3_node(&trans, GET_BY_HANDLE, pref3_meta);
    ASSERT_TRUE(pref3_node.good());
    EXPECT_TRUE(pref3_node.GetKernelCopy().is_dirty());
    EXPECT_FALSE(pref3_node.GetIsUnsynced());
    EXPECT_TRUE(pref3_node.GetIsUnappliedUpdate());
    EXPECT_TRUE(pref3_node.GetIsDel());
    EXPECT_GT(pref3_node.GetServerVersion(), 0);
    EXPECT_EQ(pref3_node.GetBaseVersion(), -1);

    syncable::Entry pref4_node(&trans, GET_BY_HANDLE, pref4_meta);
    ASSERT_TRUE(pref4_node.good());
    EXPECT_TRUE(pref4_node.GetKernelCopy().is_dirty());
    EXPECT_FALSE(pref4_node.GetIsUnsynced());
    EXPECT_TRUE(pref4_node.GetIsUnappliedUpdate());
    EXPECT_TRUE(pref4_node.GetIsDel());
    EXPECT_GT(pref4_node.GetServerVersion(), 0);
    EXPECT_EQ(pref4_node.GetBaseVersion(), -1);

    // Pref 5 should remain untouched.
    syncable::Entry pref5_node(&trans, GET_BY_HANDLE, pref5_meta);
    ASSERT_TRUE(pref5_node.good());
    EXPECT_FALSE(pref5_node.GetKernelCopy().is_dirty());
    EXPECT_FALSE(pref5_node.GetIsUnsynced());
    EXPECT_TRUE(pref5_node.GetIsUnappliedUpdate());
    EXPECT_TRUE(pref5_node.GetIsDel());
    EXPECT_GT(pref5_node.GetServerVersion(), 0);
    EXPECT_EQ(pref5_node.GetBaseVersion(), -1);

    syncable::Entry bookmark_node(&trans, GET_BY_HANDLE, bookmark_meta);
    ASSERT_TRUE(bookmark_node.good());
    EXPECT_TRUE(bookmark_node.GetKernelCopy().is_dirty());
    EXPECT_FALSE(bookmark_node.GetIsUnsynced());
    EXPECT_TRUE(bookmark_node.GetIsUnappliedUpdate());
    EXPECT_TRUE(bookmark_node.GetIsDel());
    EXPECT_GT(bookmark_node.GetServerVersion(), 0);
    EXPECT_EQ(bookmark_node.GetBaseVersion(), -1);
  }
}

// A test harness to exercise the code that processes and passes changes from
// the "SYNCER"-WriteTransaction destructor, through the SyncManager, to the
// ChangeProcessor.
class SyncManagerChangeProcessingTest : public SyncManagerTest {
 public:
  void OnChangesApplied(ModelType model_type,
                        int64_t model_version,
                        const BaseTransaction* trans,
                        const ImmutableChangeRecordList& changes) override {
    last_changes_ = changes;
  }

  void OnChangesComplete(ModelType model_type) override {}

  const ImmutableChangeRecordList& GetRecentChangeList() {
    return last_changes_;
  }

  UserShare* share() { return sync_manager_.GetUserShare(); }

  // Set some flags so our nodes reasonably approximate the real world scenario
  // and can get past CheckTreeInvariants.
  //
  // It's never going to be truly accurate, since we're squashing update
  // receipt, processing and application into a single transaction.
  void SetNodeProperties(syncable::MutableEntry* entry) {
    entry->PutId(id_factory_.NewServerId());
    entry->PutBaseVersion(10);
    entry->PutServerVersion(10);
  }

  // Looks for the given change in the list.  Returns the index at which it was
  // found.  Returns -1 on lookup failure.
  size_t FindChangeInList(int64_t id, ChangeRecord::Action action) {
    SCOPED_TRACE(id);
    for (size_t i = 0; i < last_changes_.Get().size(); ++i) {
      if (last_changes_.Get()[i].id == id &&
          last_changes_.Get()[i].action == action) {
        return i;
      }
    }
    ADD_FAILURE() << "Failed to find specified change";
    return static_cast<size_t>(-1);
  }

  // Returns the current size of the change list.
  //
  // Note that spurious changes do not necessarily indicate a problem.
  // Assertions on change list size can help detect problems, but it may be
  // necessary to reduce their strictness if the implementation changes.
  size_t GetChangeListSize() { return last_changes_.Get().size(); }

  void ClearChangeList() { last_changes_ = ImmutableChangeRecordList(); }

 protected:
  ImmutableChangeRecordList last_changes_;
  TestIdFactory id_factory_;
};

// Test creation of a folder and a bookmark.
TEST_F(SyncManagerChangeProcessingTest, AddBookmarks) {
  int64_t type_root = GetIdForDataType(BOOKMARKS);
  int64_t folder_id = kInvalidId;
  int64_t child_id = kInvalidId;

  // Create a folder and a bookmark under it.
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share()->directory.get());
    syncable::Entry root(&trans, syncable::GET_BY_HANDLE, type_root);
    ASSERT_TRUE(root.good());

    syncable::MutableEntry folder(&trans, syncable::CREATE, BOOKMARKS,
                                  root.GetId(), "folder");
    ASSERT_TRUE(folder.good());
    SetNodeProperties(&folder);
    folder.PutIsDir(true);
    folder_id = folder.GetMetahandle();

    syncable::MutableEntry child(&trans, syncable::CREATE, BOOKMARKS,
                                 folder.GetId(), "child");
    ASSERT_TRUE(child.good());
    SetNodeProperties(&child);
    child_id = child.GetMetahandle();
  }

  // The closing of the above scope will delete the transaction.  Its processed
  // changes should be waiting for us in a member of the test harness.
  EXPECT_EQ(2UL, GetChangeListSize());

  // We don't need to check these return values here.  The function will add a
  // non-fatal failure if these changes are not found.
  size_t folder_change_pos =
      FindChangeInList(folder_id, ChangeRecord::ACTION_ADD);
  size_t child_change_pos =
      FindChangeInList(child_id, ChangeRecord::ACTION_ADD);

  // Parents are delivered before children.
  EXPECT_LT(folder_change_pos, child_change_pos);
}

// Test creation of a preferences (with implicit parent Id)
TEST_F(SyncManagerChangeProcessingTest, AddPreferences) {
  int64_t item1_id = kInvalidId;
  int64_t item2_id = kInvalidId;

  // Create two preferences.
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share()->directory.get());

    syncable::MutableEntry item1(&trans, syncable::CREATE, PREFERENCES,
                                 "test_item_1");
    ASSERT_TRUE(item1.good());
    SetNodeProperties(&item1);
    item1_id = item1.GetMetahandle();

    // Need at least two items to ensure hitting all possible codepaths in
    // ChangeReorderBuffer::Traversal::ExpandToInclude.
    syncable::MutableEntry item2(&trans, syncable::CREATE, PREFERENCES,
                                 "test_item_2");
    ASSERT_TRUE(item2.good());
    SetNodeProperties(&item2);
    item2_id = item2.GetMetahandle();
  }

  // The closing of the above scope will delete the transaction.  Its processed
  // changes should be waiting for us in a member of the test harness.
  EXPECT_EQ(2UL, GetChangeListSize());

  FindChangeInList(item1_id, ChangeRecord::ACTION_ADD);
  FindChangeInList(item2_id, ChangeRecord::ACTION_ADD);
}

// Test moving a bookmark into an empty folder.
TEST_F(SyncManagerChangeProcessingTest, MoveBookmarkIntoEmptyFolder) {
  int64_t type_root = GetIdForDataType(BOOKMARKS);
  int64_t folder_b_id = kInvalidId;
  int64_t child_id = kInvalidId;

  // Create two folders.  Place a child under folder A.
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share()->directory.get());
    syncable::Entry root(&trans, syncable::GET_BY_HANDLE, type_root);
    ASSERT_TRUE(root.good());

    syncable::MutableEntry folder_a(&trans, syncable::CREATE, BOOKMARKS,
                                    root.GetId(), "folderA");
    ASSERT_TRUE(folder_a.good());
    SetNodeProperties(&folder_a);
    folder_a.PutIsDir(true);

    syncable::MutableEntry folder_b(&trans, syncable::CREATE, BOOKMARKS,
                                    root.GetId(), "folderB");
    ASSERT_TRUE(folder_b.good());
    SetNodeProperties(&folder_b);
    folder_b.PutIsDir(true);
    folder_b_id = folder_b.GetMetahandle();

    syncable::MutableEntry child(&trans, syncable::CREATE, BOOKMARKS,
                                 folder_a.GetId(), "child");
    ASSERT_TRUE(child.good());
    SetNodeProperties(&child);
    child_id = child.GetMetahandle();
  }

  // Close that transaction.  The above was to setup the initial scenario.  The
  // real test starts now.

  // Move the child from folder A to folder B.
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share()->directory.get());

    syncable::Entry folder_b(&trans, syncable::GET_BY_HANDLE, folder_b_id);
    syncable::MutableEntry child(&trans, syncable::GET_BY_HANDLE, child_id);

    child.PutParentId(folder_b.GetId());
  }

  EXPECT_EQ(1UL, GetChangeListSize());

  // Verify that this was detected as a real change.  An early version of the
  // UniquePosition code had a bug where moves from one folder to another were
  // ignored unless the moved node's UniquePosition value was also changed in
  // some way.
  FindChangeInList(child_id, ChangeRecord::ACTION_UPDATE);
}

// Test moving a bookmark into a non-empty folder.
TEST_F(SyncManagerChangeProcessingTest, MoveIntoPopulatedFolder) {
  int64_t type_root = GetIdForDataType(BOOKMARKS);
  int64_t child_a_id = kInvalidId;
  int64_t child_b_id = kInvalidId;

  // Create two folders.  Place one child each under folder A and folder B.
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share()->directory.get());
    syncable::Entry root(&trans, syncable::GET_BY_HANDLE, type_root);
    ASSERT_TRUE(root.good());

    syncable::MutableEntry folder_a(&trans, syncable::CREATE, BOOKMARKS,
                                    root.GetId(), "folderA");
    ASSERT_TRUE(folder_a.good());
    SetNodeProperties(&folder_a);
    folder_a.PutIsDir(true);

    syncable::MutableEntry folder_b(&trans, syncable::CREATE, BOOKMARKS,
                                    root.GetId(), "folderB");
    ASSERT_TRUE(folder_b.good());
    SetNodeProperties(&folder_b);
    folder_b.PutIsDir(true);

    syncable::MutableEntry child_a(&trans, syncable::CREATE, BOOKMARKS,
                                   folder_a.GetId(), "childA");
    ASSERT_TRUE(child_a.good());
    SetNodeProperties(&child_a);
    child_a_id = child_a.GetMetahandle();

    syncable::MutableEntry child_b(&trans, syncable::CREATE, BOOKMARKS,
                                   folder_b.GetId(), "childB");
    SetNodeProperties(&child_b);
    child_b_id = child_b.GetMetahandle();
  }

  // Close that transaction.  The above was to setup the initial scenario.  The
  // real test starts now.

  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share()->directory.get());

    syncable::MutableEntry child_a(&trans, syncable::GET_BY_HANDLE, child_a_id);
    syncable::MutableEntry child_b(&trans, syncable::GET_BY_HANDLE, child_b_id);

    // Move child A from folder A to folder B and update its position.
    child_a.PutParentId(child_b.GetParentId());
    child_a.PutPredecessor(child_b.GetId());
  }

  EXPECT_EQ(1UL, GetChangeListSize());

  // Verify that only child a is in the change list.
  // (This function will add a failure if the lookup fails.)
  FindChangeInList(child_a_id, ChangeRecord::ACTION_UPDATE);
}

// Tests the ordering of deletion changes.
TEST_F(SyncManagerChangeProcessingTest, DeletionsAndChanges) {
  int64_t type_root = GetIdForDataType(BOOKMARKS);
  int64_t folder_a_id = kInvalidId;
  int64_t folder_b_id = kInvalidId;
  int64_t child_id = kInvalidId;

  // Create two folders.  Place a child under folder A.
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share()->directory.get());
    syncable::Entry root(&trans, syncable::GET_BY_HANDLE, type_root);
    ASSERT_TRUE(root.good());

    syncable::MutableEntry folder_a(&trans, syncable::CREATE, BOOKMARKS,
                                    root.GetId(), "folderA");
    ASSERT_TRUE(folder_a.good());
    SetNodeProperties(&folder_a);
    folder_a.PutIsDir(true);
    folder_a_id = folder_a.GetMetahandle();

    syncable::MutableEntry folder_b(&trans, syncable::CREATE, BOOKMARKS,
                                    root.GetId(), "folderB");
    ASSERT_TRUE(folder_b.good());
    SetNodeProperties(&folder_b);
    folder_b.PutIsDir(true);
    folder_b_id = folder_b.GetMetahandle();

    syncable::MutableEntry child(&trans, syncable::CREATE, BOOKMARKS,
                                 folder_a.GetId(), "child");
    ASSERT_TRUE(child.good());
    SetNodeProperties(&child);
    child_id = child.GetMetahandle();
  }

  // Close that transaction.  The above was to setup the initial scenario.  The
  // real test starts now.

  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                     share()->directory.get());

    syncable::MutableEntry folder_a(&trans, syncable::GET_BY_HANDLE,
                                    folder_a_id);
    syncable::MutableEntry folder_b(&trans, syncable::GET_BY_HANDLE,
                                    folder_b_id);
    syncable::MutableEntry child(&trans, syncable::GET_BY_HANDLE, child_id);

    // Delete folder B and its child.
    child.PutIsDel(true);
    folder_b.PutIsDel(true);

    // Make an unrelated change to folder A.
    folder_a.PutNonUniqueName("NewNameA");
  }

  EXPECT_EQ(3UL, GetChangeListSize());

  size_t folder_a_pos =
      FindChangeInList(folder_a_id, ChangeRecord::ACTION_UPDATE);
  size_t folder_b_pos =
      FindChangeInList(folder_b_id, ChangeRecord::ACTION_DELETE);
  size_t child_pos = FindChangeInList(child_id, ChangeRecord::ACTION_DELETE);

  // Deletes should appear before updates.
  EXPECT_LT(child_pos, folder_a_pos);
  EXPECT_LT(folder_b_pos, folder_a_pos);
}

// During initialization SyncManagerImpl loads sqlite database. If it fails to
// do so it should fail initialization. This test verifies this behavior.
// Test reuses SyncManagerImpl initialization from SyncManagerTest but overrides
// EngineComponentsFactory to return DirectoryBackingStore that always fails
// to load.
class SyncManagerInitInvalidStorageTest : public SyncManagerTest {
 public:
  SyncManagerInitInvalidStorageTest() {}

  EngineComponentsFactory* GetFactory() override {
    return new TestEngineComponentsFactory(
        GetSwitches(), EngineComponentsFactory::STORAGE_INVALID,
        &storage_used_);
  }
};

// SyncManagerInitInvalidStorageTest::GetFactory will return
// DirectoryBackingStore that ensures that SyncManagerImpl::OpenDirectory fails.
// SyncManagerImpl initialization is done in SyncManagerTest::SetUp. This test's
// task is to ensure that SyncManagerImpl reported initialization failure in
// OnInitializationComplete callback.
TEST_F(SyncManagerInitInvalidStorageTest, FailToOpenDatabase) {
  EXPECT_FALSE(initialization_succeeded_);
}

}  // namespace syncer
