// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {
namespace {

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

const BookmarkNode* FindNodeByUuid(const UuidIndex& index,
                                   const std::string& uuid_str) {
  const base::Uuid uuid = base::Uuid::ParseLowercase(uuid_str);
  CHECK(uuid.is_valid());
  const auto it = index.find(uuid);
  return it == index.end() ? nullptr : *it;
}

TEST(ModelLoaderTest, LoadNonEmptyModel) {
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  ASSERT_TRUE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      test_file,
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails>& details = details_future.Get();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());

  EXPECT_FALSE(details->required_recovery());
  EXPECT_FALSE(details->ids_reassigned());
  EXPECT_EQ(11, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());

  EXPECT_EQ("dummy-sync-metadata-1",
            details->local_or_syncable_sync_metadata_str());

  const UuidIndex uuid_index = details->owned_local_or_syncable_uuid_index();

  // Sanity-check the presence of one node.
  const BookmarkNode* folder_b1 =
      FindNodeByUuid(uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, folder_b1);
  EXPECT_EQ(u"Folder B1", folder_b1->GetTitle());
  EXPECT_EQ(4, folder_b1->id());
}

}  // namespace

}  // namespace bookmarks
