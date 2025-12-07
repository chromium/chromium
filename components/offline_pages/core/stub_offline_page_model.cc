// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/stub_offline_page_model.h"

#include "base/files/file_path.h"

namespace offline_pages {

StubOfflinePageModel::StubOfflinePageModel()
    : archive_directory_(base::FilePath(FILE_PATH_LITERAL("/archive_dir/"))) {}
StubOfflinePageModel::~StubOfflinePageModel() = default;

void StubOfflinePageModel::SetArchiveDirectory(const base::FilePath& path) {
  archive_directory_ = path;
}

void StubOfflinePageModel::AddObserver(Observer* observer) {}
void StubOfflinePageModel::RemoveObserver(Observer* observer) {}
void StubOfflinePageModel::SavePage(
    const SavePageParams& save_page_params,
    std::unique_ptr<OfflinePageArchiver> archiver,
    content::WebContents* web_contents,
    SavePageCallback callback) {}
void StubOfflinePageModel::AddPage(const OfflinePageItem& page,
                                   AddPageCallback callback) {}
void StubOfflinePageModel::MarkPageAccessed(int64_t offline_id) {}
void StubOfflinePageModel::DeletePagesWithCriteria(
    const PageCriteria& criteria,
    DeletePageCallback callback) {}
void StubOfflinePageModel::DeleteCachedPagesByURLPredicate(
    const UrlPredicate& predicate,
    DeletePageCallback callback) {}
void StubOfflinePageModel::GetAllPages(
    MultipleOfflinePageItemCallback callback) {}
void StubOfflinePageModel::GetOfflineIdsForClientId(
    const ClientId& client_id,
    MultipleOfflineIdCallback callback) {}
void StubOfflinePageModel::GetPageByOfflineId(
    int64_t offline_id,
    SingleOfflinePageItemCallback callback) {}
void StubOfflinePageModel::GetPagesWithCriteria(
    const PageCriteria& criteria,
    MultipleOfflinePageItemCallback callback) {}
void StubOfflinePageModel::StoreThumbnail(int64_t offline_id,
                                          std::string thumbnail) {}
void StubOfflinePageModel::StoreFavicon(int64_t offline_id,
                                        std::string favicon) {}
void StubOfflinePageModel::GetVisualsByOfflineId(int64_t offline_id,
                                                 GetVisualsCallback callback) {}
void StubOfflinePageModel::GetVisualsAvailability(
    int64_t offline_id,
    base::OnceCallback<void(VisualsAvailability)> callback) {}
void StubOfflinePageModel::PublishInternalArchive(
    const OfflinePageItem& offline_page,
    PublishPageCallback publish_done_callback) {}
const base::FilePath& StubOfflinePageModel::GetArchiveDirectory(
    const std::string& name_space) const {
  return archive_directory_;
}
bool StubOfflinePageModel::IsArchiveInInternalDir(
    const base::FilePath& file_path) const {
  return archive_directory_.IsParent(file_path);
}

OfflineEventLogger* StubOfflinePageModel::GetLogger() {
  return nullptr;
}
}  // namespace offline_pages
