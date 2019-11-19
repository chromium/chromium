// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/update_publish_id_task.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class UpdatePublishIdTaskTest : public ModelTaskTestBase {};

TEST_F(UpdatePublishIdTaskTest, UpdatePublishId) {
  OfflinePageItem page = generator()->CreateItem();
  store_test_util()->InsertItem(page);
  base::FilePath new_file_path(FILE_PATH_LITERAL("/new/path/to/file"));
  int64_t new_system_download_id = 42LL;

  base::MockCallback<base::OnceCallback<void(bool)>> callback;

  EXPECT_CALL(callback, Run(true));

  // Build and run a task to change the file path in the offline page model.
  auto task = std::make_unique<UpdatePublishIdTask>(
      store(), page.offline_id,
      PublishedArchiveId(new_system_download_id, new_file_path),
      callback.Get());
  RunTask(std::move(task));

  auto offline_page = store_test_util()->GetPageByOfflineId(page.offline_id);

  EXPECT_EQ(new_file_path, offline_page->file_path);
  EXPECT_EQ(new_system_download_id, offline_page->system_download_id);
}

}  // namespace offline_pages
