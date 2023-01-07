// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_STUB_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_STUB_OFFLINE_PAGE_MODEL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/offline_pages/core/offline_page_model.h"

namespace offline_pages {

// Stub implementation of OfflinePageModel interface for testing. Besides using
// as a stub for tests, it may also be subclassed to mock specific methods
// needed for a set of tests.
class StubOfflinePageModel : public OfflinePageModel {
 public:
  StubOfflinePageModel();
  ~StubOfflinePageModel() override;

  void SetArchiveDirectory(const base::FilePath& path);

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SavePage(const SavePageParams& save_page_params,
                std::unique_ptr<OfflinePageArchiver> archiver,
                content::WebContents* web_contents,
                SavePageCallback callback) override;
  void AddPage(const OfflinePageItem& page, AddPageCallback callback) override;
  void MarkPageAccessed(int64_t offline_id) override;
  void DeletePagesWithCriteria(const PageCriteria& criteria,
                               DeletePageCallback callback) override;
  void DeleteCachedPagesByURLPredicate(const UrlPredicate& predicate,
                                       DeletePageCallback callback) override;
  void GetAllPages(MultipleOfflinePageItemCallback callback) override;
  void GetOfflineIdsForClientId(const ClientId& client_id,
                                MultipleOfflineIdCallback callback) override;
  void GetPageByOfflineId(int64_t offline_id,
                          SingleOfflinePageItemCallback callback) override;
  void GetPagesWithCriteria(const PageCriteria& criteria,
                            MultipleOfflinePageItemCallback callback) override;
  void StoreThumbnail(int64_t offline_id, std::string thumbnail) override;
  void StoreFavicon(int64_t offline_id, std::string favicon) override;
  void GetVisualsByOfflineId(int64_t offline_id,
                             GetVisualsCallback callback) override;
  void GetVisualsAvailability(
      int64_t offline_id,
      base::OnceCallback<void(VisualsAvailability)> callback) override;
  void PublishInternalArchive(
      const OfflinePageItem& offline_page,
      PublishPageCallback publish_done_callback) override;
  const base::FilePath& GetArchiveDirectory(
      const std::string& name_space) const override;
  bool IsArchiveInInternalDir(const base::FilePath& file_path) const override;
  OfflineEventLogger* GetLogger() override;

 private:
  std::vector<int64_t> offline_ids_;
  base::FilePath archive_directory_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_STUB_OFFLINE_PAGE_MODEL_H_
