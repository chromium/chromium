// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_test_archive_publisher.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/offline_pages/core/archive_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

OfflinePageTestArchivePublisher::OfflinePageTestArchivePublisher(
    ArchiveManager* archive_manager,
    int64_t download_id_to_use)
    : expect_publish_archive_called_(false),
      publish_archive_called_(false),
      archive_attempt_failure_(false),
      use_verbatim_archive_path_(false),
      download_id_(download_id_to_use),
      archive_manager_(archive_manager) {}

OfflinePageTestArchivePublisher::~OfflinePageTestArchivePublisher() {
  if (expect_publish_archive_called_)
    EXPECT_TRUE(publish_archive_called_);
}

void OfflinePageTestArchivePublisher::PublishArchive(
    const OfflinePageItem& offline_page,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    PublishArchiveDoneCallback publish_done_callback) const {
  publish_archive_called_ = true;
  PublishArchiveResult publish_archive_result;

  if (archive_attempt_failure_) {
    publish_archive_result.move_result = SavePageResult::FILE_MOVE_FAILED;
  } else {
    publish_archive_result.move_result = SavePageResult::SUCCESS;

    // TODO(iwells): use_verbatim_archive_path is meant to accommodate
    // offline_page_request_handler_unittest.cc whose tests fail when the
    // published archive path is built using publish_directory. The tests should
    // be fixed and this special case should be removed.
    if (use_verbatim_archive_path_) {
      publish_archive_result.id.new_file_path = offline_page.file_path;
    } else {
      publish_archive_result.id.new_file_path =
          archive_manager_->GetPublicArchivesDir().Append(
              offline_page.file_path.BaseName());
    }
    publish_archive_result.id.download_id = download_id_;
  }

  background_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(publish_done_callback), offline_page,
                                std::move(publish_archive_result)));
}

void OfflinePageTestArchivePublisher::UnpublishArchives(
    const std::vector<PublishedArchiveId>& archive_ids) const {
  if (!archive_ids.empty())
    last_removed_id_ = archive_ids.back();
}

base::WeakPtr<OfflinePageArchivePublisher>
OfflinePageTestArchivePublisher::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace offline_pages
