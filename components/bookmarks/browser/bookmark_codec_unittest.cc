// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_codec.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace bookmarks {
namespace {

const char16_t kUrl1Title[] = u"url1";
const char kUrl1Url[] = "http://www.url1.com";
const char16_t kUrl2Title[] = u"url2";
const char kUrl2Url[] = "http://www.url2.com";
const char16_t kUrl3Title[] = u"url3";
const char kUrl3Url[] = "http://www.url3.com";
const char16_t kUrl4Title[] = u"url4";
const char kUrl4Url[] = "http://www.url4.com";
const char16_t kFolder1Title[] = u"folder1";
const char16_t kFolder2Title[] = u"folder2";

const base::FilePath& GetTestDataDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &dir);
    return dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data");
  }());
  return *dir;
}

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

// Helper to verify the two given bookmark nodes.
void AssertNodesEqual(const BookmarkNode* expected,
                      const BookmarkNode* actual) {
  ASSERT_TRUE(expected);
  ASSERT_TRUE(actual);
  EXPECT_EQ(expected->id(), actual->id());
  EXPECT_EQ(expected->guid(), actual->guid());
  EXPECT_EQ(expected->GetTitle(), actual->GetTitle());
  EXPECT_EQ(expected->type(), actual->type());
  EXPECT_TRUE(expected->date_added() == actual->date_added());
  if (expected->is_url()) {
    EXPECT_EQ(expected->url(), actual->url());
  } else {
    EXPECT_TRUE(expected->date_folder_modified() ==
                actual->date_folder_modified());
    ASSERT_EQ(expected->children().size(), actual->children().size());
    for (size_t i = 0; i < expected->children().size(); ++i) {
      AssertNodesEqual(expected->children()[i].get(),
                       actual->children()[i].get());
    }
  }
}

// Verifies that the two given bookmark models are the same.
void AssertModelsEqual(BookmarkModel* expected, BookmarkModel* actual) {
  ASSERT_NO_FATAL_FAILURE(AssertNodesEqual(expected->bookmark_bar_node(),
                                           actual->bookmark_bar_node()));
  ASSERT_NO_FATAL_FAILURE(
      AssertNodesEqual(expected->other_node(), actual->other_node()));
  ASSERT_NO_FATAL_FAILURE(
      AssertNodesEqual(expected->mobile_node(), actual->mobile_node()));
}

}  // namespace

class BookmarkCodecTest : public testing::Test {
 protected:
  // Helpers to create bookmark models with different data.
  BookmarkModel* CreateTestModel1() {
    std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
    const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    return model.release();
  }
  BookmarkModel* CreateTestModel2() {
    std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
    const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    model->AddURL(bookmark_bar, 1, kUrl2Title, GURL(kUrl2Url));
    return model.release();
  }
  BookmarkModel* CreateTestModel3() {
    std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
    const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    const BookmarkNode* folder1 =
        model->AddFolder(bookmark_bar, 1, kFolder1Title);
    model->AddURL(folder1, 0, kUrl2Title, GURL(kUrl2Url));
    return model.release();
  }

  void GetBookmarksBarChildValue(base::Value::Dict* value,
                                 size_t index,
                                 base::Value** result_value) {
    base::Value::Dict* roots = value->FindDict(BookmarkCodec::kRootsKey);
    ASSERT_TRUE(roots);

    base::Value::Dict* bb_dict =
        roots->FindDict(BookmarkCodec::kBookmarkBarFolderNameKey);
    ASSERT_TRUE(bb_dict);

    base::Value::List* bb_children_list =
        bb_dict->FindList(BookmarkCodec::kChildrenKey);
    ASSERT_TRUE(bb_children_list);
    ASSERT_LT(index, bb_children_list->size());

    base::Value& child_value = (*bb_children_list)[index];
    ASSERT_TRUE(child_value.is_dict());

    *result_value = &child_value;
  }

  base::Value::Dict EncodeHelper(BookmarkModel* model,
                                 const std::string& sync_metadata_str,
                                 std::string* checksum) {
    BookmarkCodec encoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", encoder.computed_checksum());
    EXPECT_EQ("", encoder.stored_checksum());

    base::Value::Dict value(encoder.Encode(model, sync_metadata_str));
    const std::string& computed_checksum = encoder.computed_checksum();
    const std::string& stored_checksum = encoder.stored_checksum();

    // Computed and stored checksums should not be empty and should be equal.
    EXPECT_FALSE(computed_checksum.empty());
    EXPECT_FALSE(stored_checksum.empty());
    EXPECT_EQ(computed_checksum, stored_checksum);

    *checksum = computed_checksum;
    return value;
  }

  bool Decode(BookmarkCodec* codec,
              const base::Value::Dict& value,
              BookmarkModel* model,
              std::string* sync_metadata_str) {
    int64_t max_id;
    bool result = codec->Decode(value, AsMutable(model->bookmark_bar_node()),
                                AsMutable(model->other_node()),
                                AsMutable(model->mobile_node()), &max_id,
                                sync_metadata_str);
    model->set_next_node_id(max_id);
    AsMutable(model->root_node())->SetMetaInfoMap(codec->model_meta_info_map());
    AsMutable(model->root_node())
        ->SetUnsyncedMetaInfoMap(codec->model_unsynced_meta_info_map());

    return result;
  }

  std::unique_ptr<BookmarkModel> DecodeHelper(
      const base::Value::Dict& value,
      const std::string& expected_stored_checksum,
      std::string* computed_checksum,
      bool expected_changes,
      std::string* sync_metadata_str) {
    BookmarkCodec decoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", decoder.computed_checksum());
    EXPECT_EQ("", decoder.stored_checksum());

    std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
    EXPECT_TRUE(Decode(&decoder, value, model.get(),
                       /*sync_metadata_str=*/sync_metadata_str));

    *computed_checksum = decoder.computed_checksum();
    const std::string& stored_checksum = decoder.stored_checksum();

    // Computed and stored checksums should not be empty.
    EXPECT_FALSE(computed_checksum->empty());
    EXPECT_FALSE(stored_checksum.empty());

    // Stored checksum should be as expected.
    EXPECT_EQ(expected_stored_checksum, stored_checksum);

    // The two checksums should be equal if expected_changes is true; otherwise
    // they should be different.
    if (expected_changes)
      EXPECT_NE(*computed_checksum, stored_checksum);
    else
      EXPECT_EQ(*computed_checksum, stored_checksum);

    return model;
  }

  void CheckIDs(const BookmarkNode* node, std::set<int64_t>* assigned_ids) {
    DCHECK(node);
    int64_t node_id = node->id();
    EXPECT_TRUE(assigned_ids->find(node_id) == assigned_ids->end());
    assigned_ids->insert(node_id);
    for (const auto& child : node->children())
      CheckIDs(child.get(), assigned_ids);
  }

  void ExpectIDsUnique(BookmarkModel* model) {
    std::set<int64_t> assigned_ids;
    CheckIDs(model->bookmark_bar_node(), &assigned_ids);
    CheckIDs(model->other_node(), &assigned_ids);
    CheckIDs(model->mobile_node(), &assigned_ids);
  }
};

TEST_F(BookmarkCodecTest, ChecksumEncodeDecodeTest) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  base::Value::Dict value =
      EncodeHelper(model_to_encode.get(), /*sync_metadata_str=*/std::string(),
                   &enc_checksum);

  std::string dec_checksum;
  std::unique_ptr<BookmarkModel> decoded_model =
      DecodeHelper(value, enc_checksum, &dec_checksum, false,
                   /*sync_metadata_str=*/nullptr);
}

TEST_F(BookmarkCodecTest, ChecksumEncodeIdenticalModelsTest) {
  // Encode two identical models and make sure the check-sums are same as long
  // as the data is the same.
  std::unique_ptr<BookmarkModel> model1(CreateTestModel1());
  std::string enc_checksum1;
  EncodeHelper(model1.get(), /*sync_metadata_str=*/std::string(),
               &enc_checksum1);

  std::unique_ptr<BookmarkModel> model2(CreateTestModel1());
  std::string enc_checksum2;
  EncodeHelper(model2.get(), /*sync_metadata_str=*/std::string(),
               &enc_checksum2);

  ASSERT_EQ(enc_checksum1, enc_checksum2);
}

TEST_F(BookmarkCodecTest, ChecksumManualEditTest) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  base::Value::Dict value =
      EncodeHelper(model_to_encode.get(), /*sync_metadata_str=*/std::string(),
                   &enc_checksum);

  // Change something in the encoded value before decoding it.
  base::Value* child1_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child1_value);
  std::string* title =
      child1_value->GetDict().FindString(BookmarkCodec::kNameKey);
  ASSERT_TRUE(title);
  std::string original_title = *title;
  child1_value->GetDict().Set(BookmarkCodec::kNameKey, original_title + "1");

  std::string dec_checksum;
  std::unique_ptr<BookmarkModel> decoded_model1 =
      DecodeHelper(value, enc_checksum, &dec_checksum, true,
                   /*sync_metadata_str=*/nullptr);

  // Undo the change and make sure the checksum is same as original.
  child1_value->GetDict().Set(BookmarkCodec::kNameKey, original_title);
  std::unique_ptr<BookmarkModel> decoded_model2 =
      DecodeHelper(value, enc_checksum, &dec_checksum, false,
                   /*sync_metadata_str=*/nullptr);
}

// Verifies no crash if a node does not have an id.
// This is a regression test for: https://crbug.com/1232410 .
TEST_F(BookmarkCodecTest, DecodeWithNoId) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  base::Value::Dict value =
      EncodeHelper(model_to_encode.get(), /*sync_metadata_str=*/std::string(),
                   &enc_checksum);

  // Remove an id.
  base::Value* child1_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child1_value);
  ASSERT_TRUE(child1_value->GetDict().Remove(BookmarkCodec::kIdKey));

  std::string dec_checksum;
  std::unique_ptr<BookmarkModel> decoded_model1 =
      DecodeHelper(value, enc_checksum, &dec_checksum, true,
                   /*sync_metadata_str=*/nullptr);
  // Test succeeds if no crash.
}

TEST_F(BookmarkCodecTest, ChecksumManualEditIDsTest) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel3());

  // The test depends on existence of multiple children under bookmark bar, so
  // make sure that's the case.
  size_t bb_child_count =
      model_to_encode->bookmark_bar_node()->children().size();
  ASSERT_GT(bb_child_count, 1u);

  std::string enc_checksum;
  base::Value::Dict value =
      EncodeHelper(model_to_encode.get(), /*sync_metadata_str=*/std::string(),
                   &enc_checksum);

  // Change IDs for all children of bookmark bar to be 1.
  base::Value* child_value = nullptr;
  for (size_t i = 0; i < bb_child_count; ++i) {
    GetBookmarksBarChildValue(&value, i, &child_value);
    std::string* id = child_value->GetDict().FindString(BookmarkCodec::kIdKey);
    ASSERT_TRUE(id);
    child_value->GetDict().Set(BookmarkCodec::kIdKey, "1");
  }

  std::string dec_checksum;
  std::unique_ptr<BookmarkModel> decoded_model =
      DecodeHelper(value, enc_checksum, &dec_checksum, true,
                   /*sync_metadata_str=*/nullptr);

  ExpectIDsUnique(decoded_model.get());

  // add a few extra nodes to bookmark model and make sure IDs are still uniuqe.
  const BookmarkNode* bb_node = decoded_model->bookmark_bar_node();
  decoded_model->AddURL(bb_node, 0, u"new url1", GURL("http://newurl1.com"));
  decoded_model->AddURL(bb_node, 0, u"new url2", GURL("http://newurl2.com"));

  ExpectIDsUnique(decoded_model.get());
}

TEST_F(BookmarkCodecTest, PersistIDsTest) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel3());
  BookmarkCodec encoder;
  base::Value::Dict model_value(
      encoder.Encode(model_to_encode.get(), std::string()));

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, model_value, decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));
  ASSERT_NO_FATAL_FAILURE(
      AssertModelsEqual(model_to_encode.get(), decoded_model.get()));

  // Add a couple of more items to the decoded bookmark model and make sure
  // ID persistence is working properly.
  const BookmarkNode* bookmark_bar = decoded_model->bookmark_bar_node();
  decoded_model->AddURL(bookmark_bar, bookmark_bar->children().size(),
                        kUrl3Title, GURL(kUrl3Url));
  const BookmarkNode* folder2_node = decoded_model->AddFolder(
      bookmark_bar, bookmark_bar->children().size(), kFolder2Title);
  decoded_model->AddURL(folder2_node, 0, kUrl4Title, GURL(kUrl4Url));

  BookmarkCodec encoder2;
  base::Value::Dict model_value2(
      encoder2.Encode(decoded_model.get(), std::string()));

  std::unique_ptr<BookmarkModel> decoded_model2(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, model_value2, decoded_model2.get(),
                     /*sync_metadata_str=*/nullptr));
  ASSERT_NO_FATAL_FAILURE(
      AssertModelsEqual(decoded_model.get(), decoded_model2.get()));
}

TEST_F(BookmarkCodecTest, CannotDecodeModelWithoutMobileBookmarks) {
  base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_without_sync.json");
  ASSERT_TRUE(base::PathExists(test_file));

  JSONFileValueDeserializer deserializer(test_file);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  EXPECT_FALSE(Decode(&decoder, *(root->GetIfDict()), decoded_model.get(),
                      /*sync_metadata_str=*/nullptr));
}

TEST_F(BookmarkCodecTest, EncodeAndDecodeMetaInfo) {
  // Add meta info and encode.
  std::unique_ptr<BookmarkModel> model(CreateTestModel1());
  model->SetNodeMetaInfo(model->root_node(), "model_info", "value1");
  model->SetNodeMetaInfo(model->bookmark_bar_node()->children().front().get(),
                         "node_info", "value2");
  std::string checksum;
  base::Value::Dict value =
      EncodeHelper(model.get(), /*sync_metadata_str=*/std::string(), &checksum);

  // Decode and check for meta info.
  model = DecodeHelper(value, checksum, &checksum, false,
                       /*sync_metadata_str=*/nullptr);
  std::string meta_value;
  EXPECT_TRUE(model->root_node()->GetMetaInfo("model_info", &meta_value));
  EXPECT_EQ("value1", meta_value);
  EXPECT_FALSE(model->root_node()->GetMetaInfo("other_key", &meta_value));
  const BookmarkNode* bbn = model->bookmark_bar_node();
  ASSERT_EQ(1u, bbn->children().size());
  const BookmarkNode* child = bbn->children().front().get();
  EXPECT_TRUE(child->GetMetaInfo("node_info", &meta_value));
  EXPECT_EQ("value2", meta_value);
  EXPECT_FALSE(child->GetMetaInfo("other_key", &meta_value));
}

TEST_F(BookmarkCodecTest, EncodeAndDecodeUnsyncedMetaInfo) {
  // Add meta info and encode.
  std::unique_ptr<BookmarkModel> model(CreateTestModel1());
  model->SetNodeUnsyncedMetaInfo(model->root_node(), "model_info", "value1");
  model->SetNodeUnsyncedMetaInfo(
      model->bookmark_bar_node()->children().front().get(), "node_info",
      "value2");
  std::string checksum;
  base::Value::Dict value =
      EncodeHelper(model.get(), /*sync_metadata_str=*/std::string(), &checksum);

  // Decode and check for meta info.
  model = DecodeHelper(value, checksum, &checksum, false,
                       /*sync_metadata_str=*/nullptr);
  std::string meta_value;
  EXPECT_TRUE(
      model->root_node()->GetUnsyncedMetaInfo("model_info", &meta_value));
  EXPECT_EQ("value1", meta_value);
  EXPECT_FALSE(
      model->root_node()->GetUnsyncedMetaInfo("other_key", &meta_value));
  const BookmarkNode* bbn = model->bookmark_bar_node();
  ASSERT_EQ(1u, bbn->children().size());
  const BookmarkNode* child = bbn->children().front().get();
  EXPECT_TRUE(child->GetUnsyncedMetaInfo("node_info", &meta_value));
  EXPECT_EQ("value2", meta_value);
  EXPECT_FALSE(child->GetUnsyncedMetaInfo("other_key", &meta_value));
}

// Verifies that we can still decode the old codec format after changing the
// way meta info is stored.
TEST_F(BookmarkCodecTest, CanDecodeMetaInfoAsString) {
  base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/meta_info_as_string.json");
  ASSERT_TRUE(base::PathExists(test_file));

  JSONFileValueDeserializer deserializer(test_file);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);

  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, *(root->GetIfDict()), model.get(),
                     /*sync_metadata_str=*/nullptr));

  const BookmarkNode* bbn = model->bookmark_bar_node();

  const char kNormalKey[] = "key";
  const char kNestedKey[] = "nested.key";
  std::string meta_value;
  EXPECT_TRUE(bbn->children()[0]->GetMetaInfo(kNormalKey, &meta_value));
  EXPECT_EQ("value", meta_value);
  EXPECT_TRUE(bbn->children()[1]->GetMetaInfo(kNormalKey, &meta_value));
  EXPECT_EQ("value2", meta_value);
  EXPECT_TRUE(bbn->children()[0]->GetMetaInfo(kNestedKey, &meta_value));
  EXPECT_EQ("value3", meta_value);
}

TEST_F(BookmarkCodecTest, EncodeAndDecodeSyncMetadata) {
  std::unique_ptr<BookmarkModel> model(CreateTestModel1());

  // Since metadata str serialized proto, it could contain no ASCII characters.
  std::string sync_metadata_str("a/2'\"");
  std::string checksum;
  base::Value::Dict value =
      EncodeHelper(model.get(), sync_metadata_str, &checksum);

  std::string decoded_sync_metadata_str;
  // Decode and verify.
  DecodeHelper(value, checksum, &checksum, false, &decoded_sync_metadata_str);
  EXPECT_EQ(sync_metadata_str, decoded_sync_metadata_str);
}

TEST_F(BookmarkCodecTest, EncodeAndDecodeGuid) {
  std::unique_ptr<BookmarkModel> model(CreateTestModel2());

  ASSERT_TRUE(model->bookmark_bar_node()->children()[0]->guid().is_valid());
  ASSERT_TRUE(model->bookmark_bar_node()->children()[1]->guid().is_valid());
  ASSERT_NE(model->bookmark_bar_node()->children()[0]->guid(),
            model->bookmark_bar_node()->children()[1]->guid());

  std::string checksum;
  base::Value::Dict model_value =
      EncodeHelper(model.get(), /*sync_metadata_str=*/std::string(), &checksum);

  // Decode and check for GUIDs.
  std::unique_ptr<BookmarkModel> decoded_model =
      DecodeHelper(model_value, checksum, &checksum, /*expected_changes=*/false,
                   /*sync_metadata_str=*/nullptr);

  ASSERT_NO_FATAL_FAILURE(AssertModelsEqual(model.get(), decoded_model.get()));

  EXPECT_EQ(model->bookmark_bar_node()->children()[0]->guid(),
            decoded_model->bookmark_bar_node()->children()[0]->guid());
  EXPECT_EQ(model->bookmark_bar_node()->children()[1]->guid(),
            decoded_model->bookmark_bar_node()->children()[1]->guid());
}

TEST_F(BookmarkCodecTest, ReassignEmptyGUID) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(encoder.Encode(model_to_encode.get(), std::string()));

  std::unique_ptr<BookmarkModel> decoded_model1(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder1;
  ASSERT_TRUE(Decode(&decoder1, value, decoded_model1.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_FALSE(decoder1.guids_reassigned());

  // Change GUID of child to be empty.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  std::string* guid_str =
      child_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(guid_str);
  std::string original_guid_str = *guid_str;
  child_value->GetDict().Set(BookmarkCodec::kGuidKey, "");

  std::unique_ptr<BookmarkModel> decoded_model2(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, value, decoded_model2.get(),
                     /*sync_metadata_str=*/nullptr));

  const base::GUID guid = base::GUID::ParseCaseInsensitive(original_guid_str);
  ASSERT_TRUE(guid.is_valid());
  EXPECT_NE(guid, decoded_model2->bookmark_bar_node()->children()[0]->guid());
  EXPECT_TRUE(
      decoded_model2->bookmark_bar_node()->children()[0]->guid().is_valid());
  EXPECT_TRUE(decoder2.guids_reassigned());
}

TEST_F(BookmarkCodecTest, ReassignMissingGUID) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(encoder.Encode(model_to_encode.get(), std::string()));

  std::unique_ptr<BookmarkModel> decoded_model1(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder1;
  ASSERT_TRUE(Decode(&decoder1, value, decoded_model1.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_FALSE(decoder1.guids_reassigned());

  // Change GUID of child to be missing.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  std::string* guid_str =
      child_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(guid_str);
  std::string original_guid_str = *guid_str;
  child_value->GetDict().Remove(BookmarkCodec::kGuidKey);

  std::unique_ptr<BookmarkModel> decoded_model2(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, value, decoded_model2.get(),
                     /*sync_metadata_str=*/nullptr));

  const base::GUID guid = base::GUID::ParseCaseInsensitive(original_guid_str);
  ASSERT_TRUE(guid.is_valid());
  EXPECT_NE(guid, decoded_model2->bookmark_bar_node()->children()[0]->guid());
  EXPECT_TRUE(
      decoded_model2->bookmark_bar_node()->children()[0]->guid().is_valid());
  EXPECT_TRUE(decoder2.guids_reassigned());
}

TEST_F(BookmarkCodecTest, ReassignInvalidGUID) {
  const std::string kInvalidGuid = "0000";
  ASSERT_FALSE(base::GUID::ParseCaseInsensitive(kInvalidGuid).is_valid());

  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(encoder.Encode(model_to_encode.get(), std::string()));

  // Change GUID of child to be invalid.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  child_value->GetDict().Set(BookmarkCodec::kGuidKey, kInvalidGuid);

  std::string* guid =
      child_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(guid);
  ASSERT_EQ(*guid, kInvalidGuid);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, value, decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.guids_reassigned());
  EXPECT_TRUE(
      decoded_model->bookmark_bar_node()->children()[0]->guid().is_valid());
}

TEST_F(BookmarkCodecTest, ReassignDuplicateGUID) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel2());

  BookmarkCodec encoder;
  base::Value::Dict value(encoder.Encode(model_to_encode.get(), std::string()));

  base::Value* child1_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child1_value);

  std::string* child1_guid =
      child1_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(child1_guid);

  base::Value* child2_value = nullptr;
  GetBookmarksBarChildValue(&value, 1, &child2_value);

  // Change GUID of child to be duplicate.
  child2_value->GetDict().Set(BookmarkCodec::kGuidKey, *child1_guid);

  std::string* child2_guid =
      child2_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(child2_guid);
  ASSERT_EQ(*child1_guid, *child2_guid);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, value, decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.guids_reassigned());
  EXPECT_NE(decoded_model->bookmark_bar_node()->children()[0]->guid(),
            decoded_model->bookmark_bar_node()->children()[1]->guid());
}

TEST_F(BookmarkCodecTest, ReassignBannedGUID) {
  const base::GUID kBannedGuid =
      base::GUID::ParseLowercase(BookmarkNode::kBannedGuidDueToPastSyncBug);
  ASSERT_TRUE(kBannedGuid.is_valid());

  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(encoder.Encode(model_to_encode.get(), std::string()));

  // Change GUID of child to be invalid.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  child_value->GetDict().Set(BookmarkCodec::kGuidKey,
                             kBannedGuid.AsLowercaseString());

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, value, decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.guids_reassigned());
  EXPECT_TRUE(
      decoded_model->bookmark_bar_node()->children()[0]->guid().is_valid());
  EXPECT_NE(decoded_model->bookmark_bar_node()->children()[0]->guid(),
            kBannedGuid);
}

TEST_F(BookmarkCodecTest, ReassignPermanentNodeDuplicateGUID) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(encoder.Encode(model_to_encode.get(), std::string()));

  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);

  // Change GUID of child to be the root node GUID.
  child_value->GetDict().Set(BookmarkCodec::kGuidKey,
                             BookmarkNode::kRootNodeGuid);

  std::string* child_guid =
      child_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(child_guid);
  ASSERT_EQ(BookmarkNode::kRootNodeGuid, *child_guid);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, value, decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.guids_reassigned());
  EXPECT_NE(base::GUID::ParseLowercase(BookmarkNode::kRootNodeGuid),
            decoded_model->bookmark_bar_node()->children()[0]->guid());
}

TEST_F(BookmarkCodecTest, CanonicalizeGUID) {
  const base::GUID kGuid = base::GUID::GenerateRandomV4();
  const std::string kUpperCaseGuid =
      base::ToUpperASCII(kGuid.AsLowercaseString());

  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  BookmarkCodec encoder;
  base::Value::Dict value(encoder.Encode(model_to_encode.get(), std::string()));

  // Change a GUID to a capitalized form, which could have been produced by an
  // older version of the browser, before canonicalization was enforced.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  child_value->GetDict().Set(BookmarkCodec::kGuidKey, kUpperCaseGuid);

  std::unique_ptr<BookmarkModel> decoded_model2(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, value, decoded_model2.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_EQ(kGuid, decoded_model2->bookmark_bar_node()->children()[0]->guid());
}

}  // namespace bookmarks
