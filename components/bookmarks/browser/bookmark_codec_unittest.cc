// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_codec.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {
namespace {

using base::ASCIIToUTF16;
using testing::ElementsAre;
using testing::Pair;

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
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
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
  EXPECT_EQ(expected->uuid(), actual->uuid());
  EXPECT_EQ(expected->GetTitle(), actual->GetTitle());
  EXPECT_EQ(expected->type(), actual->type());
  EXPECT_EQ(expected->date_added(), actual->date_added());
  if (expected->is_url()) {
    EXPECT_EQ(expected->url(), actual->url());
  } else {
    EXPECT_EQ(expected->date_folder_modified(), actual->date_folder_modified());
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
  std::unique_ptr<BookmarkModel> CreateTestModel1() {
    std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
    const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    return model;
  }
  std::unique_ptr<BookmarkModel> CreateTestModel2() {
    std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
    const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    model->AddURL(bookmark_bar, 1, kUrl2Title, GURL(kUrl2Url));
    return model;
  }
  std::unique_ptr<BookmarkModel> CreateTestModel3() {
    std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
    const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    const BookmarkNode* folder1 =
        model->AddFolder(bookmark_bar, 1, kFolder1Title);
    model->AddURL(folder1, 0, kUrl2Title, GURL(kUrl2Url));
    return model;
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

  base::Value::Dict EncodeModel(
      BookmarkModel* model,
      const std::string& sync_metadata_str = std::string(),
      std::string* checksum = nullptr) {
    BookmarkCodec encoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", encoder.ComputedChecksumForTest());
    EXPECT_EQ("", encoder.StoredChecksumForTest());

    base::Value::Dict value(
        encoder.Encode(model->bookmark_bar_node(), model->other_node(),
                       model->mobile_node(), sync_metadata_str));
    const std::string& computed_checksum = encoder.ComputedChecksumForTest();
    const std::string& stored_checksum = encoder.StoredChecksumForTest();

    // Computed and stored checksums should not be empty and should be equal.
    EXPECT_FALSE(computed_checksum.empty());
    EXPECT_FALSE(stored_checksum.empty());
    EXPECT_EQ(computed_checksum, stored_checksum);

    if (checksum) {
      *checksum = computed_checksum;
    }

    return value;
  }

  bool Decode(BookmarkCodec* codec,
              const base::Value::Dict& value,
              std::set<int64_t> already_assigned_ids,
              BookmarkModel* model,
              std::string* sync_metadata_str) {
    int64_t max_id;
    bool result = codec->Decode(
        value, already_assigned_ids, AsMutable(model->bookmark_bar_node()),
        AsMutable(model->other_node()), AsMutable(model->mobile_node()),
        &max_id, sync_metadata_str);
    model->set_next_node_id(max_id);

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
    EXPECT_EQ("", decoder.ComputedChecksumForTest());
    EXPECT_EQ("", decoder.StoredChecksumForTest());

    std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
    EXPECT_TRUE(Decode(&decoder, value, /*already_assigned_ids=*/{},
                       model.get(),
                       /*sync_metadata_str=*/sync_metadata_str));

    *computed_checksum = decoder.ComputedChecksumForTest();
    const std::string& stored_checksum = decoder.StoredChecksumForTest();

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
      EncodeModel(model_to_encode.get(), /*sync_metadata_str=*/std::string(),
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
  EncodeModel(model1.get(), /*sync_metadata_str=*/std::string(),
              &enc_checksum1);

  std::unique_ptr<BookmarkModel> model2(CreateTestModel1());
  std::string enc_checksum2;
  EncodeModel(model2.get(), /*sync_metadata_str=*/std::string(),
              &enc_checksum2);

  ASSERT_EQ(enc_checksum1, enc_checksum2);
}

TEST_F(BookmarkCodecTest, ChecksumManualEditTest) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  base::Value::Dict value =
      EncodeModel(model_to_encode.get(), /*sync_metadata_str=*/std::string(),
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
      EncodeModel(model_to_encode.get(), /*sync_metadata_str=*/std::string(),
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
      EncodeModel(model_to_encode.get(), /*sync_metadata_str=*/std::string(),
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
  base::Value::Dict model_value(EncodeModel(model_to_encode.get()));

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, model_value, /*already_assigned_ids=*/{},
                     decoded_model.get(),
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
  base::Value::Dict model_value2(EncodeModel(decoded_model.get()));

  std::unique_ptr<BookmarkModel> decoded_model2(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, model_value2, /*already_assigned_ids=*/{},
                     decoded_model2.get(),
                     /*sync_metadata_str=*/nullptr));
  ASSERT_NO_FATAL_FAILURE(
      AssertModelsEqual(decoded_model.get(), decoded_model2.get()));
}

TEST_F(BookmarkCodecTest, DecodeModel) {
  base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  ASSERT_TRUE(base::PathExists(test_file));

  JSONFileValueDeserializer deserializer(test_file);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  std::string sync_metadata_str;
  EXPECT_TRUE(Decode(&decoder, *(root->GetIfDict()),
                     /*already_assigned_ids=*/{}, decoded_model.get(),
                     &sync_metadata_str));

  EXPECT_EQ("dummy-sync-metadata-1", sync_metadata_str);
  EXPECT_FALSE(decoder.ids_reassigned());
  EXPECT_FALSE(decoder.required_recovery());
  EXPECT_EQ(decoder.release_assigned_ids(),
            std::set<int64_t>({1, 2, 3, 4, 5, 6, 7, 9, 10}));
  EXPECT_EQ(11, decoded_model->next_node_id());

  // Compare with the content of model_with_sync_metadata_1.json.
  ASSERT_EQ(1u, decoded_model->bookmark_bar_node()->children().size());
  ASSERT_EQ(1u, decoded_model->other_node()->children().size());
  ASSERT_EQ(1u, decoded_model->mobile_node()->children().size());

  {
    const BookmarkNode* actual_folder_a1 =
        decoded_model->bookmark_bar_node()->children()[0].get();
    ASSERT_EQ(1u, actual_folder_a1->children().size());

    EXPECT_EQ(3, actual_folder_a1->id());
    EXPECT_TRUE(actual_folder_a1->is_folder());
    EXPECT_EQ(u"Folder A1", actual_folder_a1->GetTitle());
    EXPECT_EQ(
        base::Uuid::ParseLowercase("cc30491d-e0bd-4112-9d09-52bf3b948ab2"),
        actual_folder_a1->uuid());

    EXPECT_EQ(5, actual_folder_a1->children()[0]->id());
    EXPECT_FALSE(actual_folder_a1->children()[0]->is_folder());
    EXPECT_EQ(
        GURL("chrome-extension://eemcgdkfndhakfknompkggombfjjjeno/main.html#3"),
        actual_folder_a1->children()[0]->url());
    EXPECT_EQ(u"Bookmark Manager", actual_folder_a1->children()[0]->GetTitle());
    EXPECT_EQ(
        base::Uuid::ParseLowercase("8976663c-4b6e-4abc-ae57-0d136b88c2f5"),
        actual_folder_a1->children()[0]->uuid());
  }

  {
    const BookmarkNode* actual_folder_b1 =
        decoded_model->other_node()->children()[0].get();
    ASSERT_EQ(1u, actual_folder_b1->children().size());

    EXPECT_EQ(4, actual_folder_b1->id());
    EXPECT_TRUE(actual_folder_b1->is_folder());
    EXPECT_EQ(u"Folder B1", actual_folder_b1->GetTitle());
    EXPECT_EQ(
        base::Uuid::ParseLowercase("da47f36f-050f-4ac9-aa35-ab0d93d39f95"),
        actual_folder_b1->uuid());

    EXPECT_EQ(6, actual_folder_b1->children()[0]->id());
    EXPECT_FALSE(actual_folder_b1->children()[0]->is_folder());
    EXPECT_EQ(GURL("http://tools.google.com/chrome/intl/en/welcome.html"),
              actual_folder_b1->children()[0]->url());
    EXPECT_EQ(u"Get started with Google Chrome",
              actual_folder_b1->children()[0]->GetTitle());
    EXPECT_EQ(
        base::Uuid::ParseLowercase("b180b384-16cf-4149-9c43-be70d2adb56e"),
        actual_folder_b1->children()[0]->uuid());
  }

  {
    const BookmarkNode* actual_folder_c1 =
        decoded_model->mobile_node()->children()[0].get();
    ASSERT_EQ(1u, actual_folder_c1->children().size());

    EXPECT_EQ(7, actual_folder_c1->id());
    EXPECT_TRUE(actual_folder_c1->is_folder());
    EXPECT_EQ(u"Folder C1", actual_folder_c1->GetTitle());
    EXPECT_EQ(
        base::Uuid::ParseLowercase("00ae74aa-1149-4abd-bac3-bad9f61d608e"),
        actual_folder_c1->uuid());

    EXPECT_EQ(9, actual_folder_c1->children()[0]->id());
    EXPECT_FALSE(actual_folder_c1->children()[0]->is_folder());
    EXPECT_EQ(GURL("chrome://settings/"),
              actual_folder_c1->children()[0]->url());
    EXPECT_EQ(u"Settings", actual_folder_c1->children()[0]->GetTitle());
    EXPECT_EQ(
        base::Uuid::ParseLowercase("acd44d5d-2f17-4c6f-b443-fa2721178e52"),
        actual_folder_c1->children()[0]->uuid());
  }
}

TEST_F(BookmarkCodecTest, CannotDecodeModelWithoutMobileBookmarks) {
  base::FilePath test_file = GetTestDataDir().AppendASCII(
      "bookmarks/model_without_mobile_bookmarks.json");
  ASSERT_TRUE(base::PathExists(test_file));

  JSONFileValueDeserializer deserializer(test_file);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  EXPECT_FALSE(Decode(&decoder, *(root->GetIfDict()),
                      /*already_assigned_ids=*/{}, decoded_model.get(),
                      /*sync_metadata_str=*/nullptr));
}

TEST_F(BookmarkCodecTest, DecodeWithDuplicateIds) {
  base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_duplicate_ids.json");
  ASSERT_TRUE(base::PathExists(test_file));

  JSONFileValueDeserializer deserializer(test_file);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  EXPECT_TRUE(Decode(&decoder, *(root->GetIfDict()),
                     /*already_assigned_ids=*/{}, decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.ids_reassigned());
  EXPECT_TRUE(decoder.required_recovery());

  EXPECT_EQ(decoder.release_assigned_ids(),
            std::set<int64_t>({1, 2, 3, 4, 5, 6, 7, 8, 9}));
  EXPECT_EQ(10, decoded_model->next_node_id());

  EXPECT_THAT(
      decoder.release_reassigned_ids_per_old_id(),
      ElementsAre(Pair(1, 1), Pair(3, 2), Pair(4, 4), Pair(4, 5), Pair(5, 3),
                  Pair(6, 6), Pair(7, 8), Pair(9, 9), Pair(10, 7)));
}

TEST_F(BookmarkCodecTest, DecodeWithAlreadyAssignedIds) {
  base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  ASSERT_TRUE(base::PathExists(test_file));

  JSONFileValueDeserializer deserializer(test_file);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  EXPECT_TRUE(Decode(&decoder, *(root->GetIfDict()),
                     /*already_assigned_ids=*/{1, 2, 3}, decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.ids_reassigned());
  EXPECT_TRUE(decoder.required_recovery());

  EXPECT_EQ(decoder.release_assigned_ids(),
            std::set<int64_t>({4, 5, 6, 7, 8, 9, 10, 11, 12}));
  EXPECT_EQ(13, decoded_model->next_node_id());
}

TEST_F(BookmarkCodecTest, DecodeWithDuplicateIdsAndAlreadyAssignedIds) {
  base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_duplicate_ids.json");
  ASSERT_TRUE(base::PathExists(test_file));

  JSONFileValueDeserializer deserializer(test_file);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  EXPECT_TRUE(Decode(&decoder, *(root->GetIfDict()),
                     /*already_assigned_ids=*/{1, 2, 3}, decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.ids_reassigned());
  EXPECT_TRUE(decoder.required_recovery());

  EXPECT_EQ(decoder.release_assigned_ids(),
            std::set<int64_t>({4, 5, 6, 7, 8, 9, 10, 11, 12}));
  EXPECT_EQ(13, decoded_model->next_node_id());
}

TEST_F(BookmarkCodecTest, EncodeAndDecodeMetaInfo) {
  // Add meta info and encode.
  std::unique_ptr<BookmarkModel> model(CreateTestModel1());
  model->SetNodeMetaInfo(model->bookmark_bar_node()->children().front().get(),
                         "node_info", "value1");
  std::string checksum;
  base::Value::Dict value =
      EncodeModel(model.get(), /*sync_metadata_str=*/std::string(), &checksum);

  // Decode and check for meta info.
  model = DecodeHelper(value, checksum, &checksum, false,
                       /*sync_metadata_str=*/nullptr);
  std::string meta_value;
  EXPECT_FALSE(model->root_node()->GetMetaInfo("other_key", &meta_value));
  const BookmarkNode* bbn = model->bookmark_bar_node();
  ASSERT_EQ(1u, bbn->children().size());
  const BookmarkNode* child = bbn->children().front().get();
  EXPECT_TRUE(child->GetMetaInfo("node_info", &meta_value));
  EXPECT_EQ("value1", meta_value);
  EXPECT_FALSE(child->GetMetaInfo("other_key", &meta_value));
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
  ASSERT_TRUE(Decode(&decoder, *(root->GetIfDict()),
                     /*already_assigned_ids=*/{}, model.get(),
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

  // Since metadata str serialized proto, it could contain non-ASCII characters.
  std::string sync_metadata_str("a/2'\"");
  std::string checksum;
  base::Value::Dict value =
      EncodeModel(model.get(), sync_metadata_str, &checksum);

  // Decode and verify.
  std::string decoded_sync_metadata_str;
  DecodeHelper(value, checksum, &checksum, false, &decoded_sync_metadata_str);
  EXPECT_EQ(sync_metadata_str, decoded_sync_metadata_str);
}

TEST_F(BookmarkCodecTest, EncodeAndDecodeSyncMetadataWithoutPermanentNodes) {
  // Since metadata str serialized proto, it could contain non-ASCII characters.
  std::string sync_metadata_str("a/2'\"");

  BookmarkCodec encoder;
  base::Value::Dict value(encoder.Encode(/*bookmark_bar_node=*/nullptr,
                                         /*other_folder_node=*/nullptr,
                                         /*mobile_folder_node=*/nullptr,
                                         sync_metadata_str));
  const std::string& computed_checksum = encoder.ComputedChecksumForTest();
  const std::string& stored_checksum = encoder.StoredChecksumForTest();

  // Computed and stored checksums should not be empty and should be equal.
  EXPECT_FALSE(computed_checksum.empty());
  EXPECT_FALSE(stored_checksum.empty());
  EXPECT_EQ(computed_checksum, stored_checksum);

  // Decode and verify.
  std::string decoded_sync_metadata_str;
  BookmarkCodec decoder;
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());

  // Note that the decoder returns this as a failure case, although
  // `decoded_sync_metadata_str` is still populated.
  EXPECT_FALSE(Decode(&decoder, value, /*already_assigned_ids=*/{}, model.get(),
                      &decoded_sync_metadata_str));

  EXPECT_EQ(sync_metadata_str, decoded_sync_metadata_str);
}

TEST_F(BookmarkCodecTest, EncodeAndDecodeGuid) {
  std::unique_ptr<BookmarkModel> model(CreateTestModel2());

  ASSERT_TRUE(model->bookmark_bar_node()->children()[0]->uuid().is_valid());
  ASSERT_TRUE(model->bookmark_bar_node()->children()[1]->uuid().is_valid());
  ASSERT_NE(model->bookmark_bar_node()->children()[0]->uuid(),
            model->bookmark_bar_node()->children()[1]->uuid());

  std::string checksum;
  base::Value::Dict model_value =
      EncodeModel(model.get(), /*sync_metadata_str=*/std::string(), &checksum);

  // Decode and check for UUIDs.
  std::unique_ptr<BookmarkModel> decoded_model =
      DecodeHelper(model_value, checksum, &checksum, /*expected_changes=*/false,
                   /*sync_metadata_str=*/nullptr);

  ASSERT_NO_FATAL_FAILURE(AssertModelsEqual(model.get(), decoded_model.get()));

  EXPECT_EQ(model->bookmark_bar_node()->children()[0]->uuid(),
            decoded_model->bookmark_bar_node()->children()[0]->uuid());
  EXPECT_EQ(model->bookmark_bar_node()->children()[1]->uuid(),
            decoded_model->bookmark_bar_node()->children()[1]->uuid());
}

TEST_F(BookmarkCodecTest, ReassignEmptyUuid) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(EncodeModel(model_to_encode.get()));

  std::unique_ptr<BookmarkModel> decoded_model1(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder1;
  ASSERT_TRUE(Decode(&decoder1, value, /*already_assigned_ids=*/{},
                     decoded_model1.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_FALSE(decoder1.required_recovery());

  // Change UUID of child to be empty.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  std::string* uuid_str =
      child_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(uuid_str);
  std::string original_uuid_str = *uuid_str;
  child_value->GetDict().Set(BookmarkCodec::kGuidKey, "");

  std::unique_ptr<BookmarkModel> decoded_model2(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, value, /*already_assigned_ids=*/{},
                     decoded_model2.get(),
                     /*sync_metadata_str=*/nullptr));

  const base::Uuid uuid = base::Uuid::ParseCaseInsensitive(original_uuid_str);
  ASSERT_TRUE(uuid.is_valid());
  EXPECT_NE(uuid, decoded_model2->bookmark_bar_node()->children()[0]->uuid());
  EXPECT_TRUE(
      decoded_model2->bookmark_bar_node()->children()[0]->uuid().is_valid());
  EXPECT_TRUE(decoder2.required_recovery());
}

TEST_F(BookmarkCodecTest, ReassignMissingUuid) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(EncodeModel(model_to_encode.get()));

  std::unique_ptr<BookmarkModel> decoded_model1(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder1;
  ASSERT_TRUE(Decode(&decoder1, value, /*already_assigned_ids=*/{},
                     decoded_model1.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_FALSE(decoder1.required_recovery());

  // Change UUID of child to be missing.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  std::string* uuid_str =
      child_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(uuid_str);
  std::string original_uuid_str = *uuid_str;
  child_value->GetDict().Remove(BookmarkCodec::kGuidKey);

  std::unique_ptr<BookmarkModel> decoded_model2(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, value, /*already_assigned_ids=*/{},
                     decoded_model2.get(),
                     /*sync_metadata_str=*/nullptr));

  const base::Uuid uuid = base::Uuid::ParseCaseInsensitive(original_uuid_str);
  ASSERT_TRUE(uuid.is_valid());
  EXPECT_NE(uuid, decoded_model2->bookmark_bar_node()->children()[0]->uuid());
  EXPECT_TRUE(
      decoded_model2->bookmark_bar_node()->children()[0]->uuid().is_valid());
  EXPECT_TRUE(decoder2.required_recovery());
}

TEST_F(BookmarkCodecTest, ReassignInvalidUuid) {
  const std::string kInvalidGuid = "0000";
  ASSERT_FALSE(base::Uuid::ParseCaseInsensitive(kInvalidGuid).is_valid());

  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(EncodeModel(model_to_encode.get()));

  // Change UUID of child to be invalid.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  child_value->GetDict().Set(BookmarkCodec::kGuidKey, kInvalidGuid);

  std::string* uuid =
      child_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(uuid);
  ASSERT_EQ(*uuid, kInvalidGuid);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, value, /*already_assigned_ids=*/{},
                     decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.required_recovery());
  EXPECT_TRUE(
      decoded_model->bookmark_bar_node()->children()[0]->uuid().is_valid());
}

TEST_F(BookmarkCodecTest, ReassignDuplicateUuid) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel2());

  BookmarkCodec encoder;
  base::Value::Dict value(EncodeModel(model_to_encode.get()));

  base::Value* child1_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child1_value);

  std::string* child1_uuid =
      child1_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(child1_uuid);

  base::Value* child2_value = nullptr;
  GetBookmarksBarChildValue(&value, 1, &child2_value);

  // Change UUID of child to be duplicate.
  child2_value->GetDict().Set(BookmarkCodec::kGuidKey, *child1_uuid);

  std::string* child2_uuid =
      child2_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(child2_uuid);
  ASSERT_EQ(*child1_uuid, *child2_uuid);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, value, /*already_assigned_ids=*/{},
                     decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.required_recovery());
  EXPECT_NE(decoded_model->bookmark_bar_node()->children()[0]->uuid(),
            decoded_model->bookmark_bar_node()->children()[1]->uuid());
}

TEST_F(BookmarkCodecTest, ReassignBannedUuid) {
  const base::Uuid kBannedGuid =
      base::Uuid::ParseLowercase(kBannedUuidDueToPastSyncBug);
  ASSERT_TRUE(kBannedGuid.is_valid());

  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(EncodeModel(model_to_encode.get()));

  // Change UUID of child to be invalid.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  child_value->GetDict().Set(BookmarkCodec::kGuidKey,
                             kBannedGuid.AsLowercaseString());

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, value, /*already_assigned_ids=*/{},
                     decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.required_recovery());
  EXPECT_TRUE(
      decoded_model->bookmark_bar_node()->children()[0]->uuid().is_valid());
  EXPECT_NE(decoded_model->bookmark_bar_node()->children()[0]->uuid(),
            kBannedGuid);
}

TEST_F(BookmarkCodecTest, ReassignPermanentNodeDuplicateUuid) {
  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());

  BookmarkCodec encoder;
  base::Value::Dict value(EncodeModel(model_to_encode.get()));

  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);

  // Change UUID of child to be the root node UUID.
  child_value->GetDict().Set(BookmarkCodec::kGuidKey, kRootNodeUuid);

  std::string* child_uuid =
      child_value->GetDict().FindString(BookmarkCodec::kGuidKey);
  ASSERT_TRUE(child_uuid);
  ASSERT_EQ(kRootNodeUuid, *child_uuid);

  std::unique_ptr<BookmarkModel> decoded_model(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder;
  ASSERT_TRUE(Decode(&decoder, value, /*already_assigned_ids=*/{},
                     decoded_model.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_TRUE(decoder.required_recovery());
  EXPECT_NE(base::Uuid::ParseLowercase(kRootNodeUuid),
            decoded_model->bookmark_bar_node()->children()[0]->uuid());
}

TEST_F(BookmarkCodecTest, CanonicalizeUuid) {
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kUpperCaseGuid =
      base::ToUpperASCII(kGuid.AsLowercaseString());

  std::unique_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  BookmarkCodec encoder;
  base::Value::Dict value(EncodeModel(model_to_encode.get()));

  // Change a UUID to a capitalized form, which could have been produced by an
  // older version of the browser, before canonicalization was enforced.
  base::Value* child_value = nullptr;
  GetBookmarksBarChildValue(&value, 0, &child_value);
  child_value->GetDict().Set(BookmarkCodec::kGuidKey, kUpperCaseGuid);

  std::unique_ptr<BookmarkModel> decoded_model2(
      TestBookmarkClient::CreateModel());
  BookmarkCodec decoder2;
  ASSERT_TRUE(Decode(&decoder2, value, /*already_assigned_ids=*/{},
                     decoded_model2.get(),
                     /*sync_metadata_str=*/nullptr));

  EXPECT_EQ(kGuid, decoded_model2->bookmark_bar_node()->children()[0]->uuid());
}

}  // namespace bookmarks
