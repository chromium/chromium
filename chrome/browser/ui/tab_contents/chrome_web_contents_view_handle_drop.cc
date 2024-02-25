// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/common/files_scan_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"
#include "ui/base/clipboard/file_info.h"

namespace {

void CompletionCallback(
    content::DropData drop_data,
    std::unique_ptr<enterprise_connectors::FilesScanData> files_scan_data,
    content::WebContentsViewDelegate::DropCompletionCallback callback,
    const enterprise_connectors::ContentAnalysisDelegate::Data& data,
    enterprise_connectors::ContentAnalysisDelegate::Result& result) {
  // If there are no negative results, proceed with just `drop_data`.
  bool all_text_results_allowed = !base::Contains(result.text_results, false);
  bool all_file_results_allowed = !base::Contains(result.paths_results, false);
  if (all_text_results_allowed && all_file_results_allowed) {
    std::move(callback).Run(std::move(drop_data));
    return;
  }

  // For text drag-drops, block the drop if any result is negative.
  if (!all_text_results_allowed) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // For file drag-drops, block file paths depending on the verdict obtained for
  // child paths.
  DCHECK(files_scan_data);
  std::set<size_t> file_indexes_to_block =
      files_scan_data->IndexesToBlock(result.paths_results);

  // If every file path should be blocked, the drop is aborted, otherwise it
  // continues by blocking sub-elements of the list. When everything is blocked,
  // it implies that no `result.paths_results` is allowed.
  if (file_indexes_to_block.size() == drop_data.filenames.size()) {
    for (size_t i = 0; i < data.paths.size(); ++i)
      result.paths_results[i] = false;

    std::move(callback).Run(std::nullopt);
    return;
  }

  // A specific index could be blocked due to its parent folder being
  // blocked and not because it got a bad verdict itself, so `result` needs
  // to be updated to reflect that.
  DCHECK_EQ(data.paths.size(),
            files_scan_data->expanded_paths_indexes().size());
  for (size_t i = 0; i < data.paths.size(); ++i) {
    int parent_index =
        files_scan_data->expanded_paths_indexes().at(data.paths[i]);
    if (file_indexes_to_block.count(parent_index))
      result.paths_results[i] = false;
  }

  std::vector<ui::FileInfo> final_filenames;
  for (size_t i = 0; i < drop_data.filenames.size(); ++i) {
    if (file_indexes_to_block.count(i))
      continue;
    final_filenames.push_back(std::move(drop_data.filenames[i]));
  }

  drop_data.filenames = std::move(final_filenames);
  std::move(callback).Run(std::move(drop_data));
}

// Helper class to handle WebContents being destroyed while files are opened in
// the threadpool. This class deletes itself either when it's no longer needed
// when ScanData is called, or when its corresponding web contents is destroyed
// so its weak ptrs are invalidated.
class HandleDropScanData : public content::WebContentsObserver {
 public:
  HandleDropScanData(
      content::WebContents* web_contents,
      content::DropData drop_data,
      enterprise_connectors::ContentAnalysisDelegate::Data analysis_data,
      content::WebContentsViewDelegate::DropCompletionCallback callback)
      : content::WebContentsObserver(web_contents),
        drop_data_(std::move(drop_data)),
        analysis_data_(std::move(analysis_data)),
        callback_(std::move(callback)) {}

  void ScanData(
      std::unique_ptr<enterprise_connectors::FilesScanData> files_scan_data) {
    DCHECK(web_contents());
    if (files_scan_data) {
      for (const auto& path : files_scan_data->expanded_paths()) {
        analysis_data_.paths.push_back(path);
      }
    }
    enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
        web_contents(), std::move(analysis_data_),
        base::BindOnce(&CompletionCallback, std::move(drop_data_),
                       std::move(files_scan_data), std::move(callback_)),
        safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP);

    delete this;
  }

  void WebContentsDestroyed() override { delete this; }

  base::WeakPtr<HandleDropScanData> GetWeakPtr() {
    return weakptr_factory_.GetWeakPtr();
  }

 private:
  content::DropData drop_data_;
  enterprise_connectors::ContentAnalysisDelegate::Data analysis_data_;
  content::WebContentsViewDelegate::DropCompletionCallback callback_;

  base::WeakPtrFactory<HandleDropScanData> weakptr_factory_{this};
};

}  // namespace

void HandleOnPerformingDrop(
    content::WebContents* web_contents,
    content::DropData drop_data,
    content::WebContentsViewDelegate::DropCompletionCallback callback) {
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto connector =
      drop_data.filenames.empty()
          ? enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY
          : enterprise_connectors::AnalysisConnector::FILE_ATTACHED;
  if (!enterprise_connectors::ContentAnalysisDelegate::IsEnabled(
          profile, web_contents->GetLastCommittedURL(), &data, connector)) {
    // If the enterprise policy is not enabled, make sure that the renderer
    // never forces a default action.
    drop_data.document_is_handling_drag = true;
    std::move(callback).Run(std::move(drop_data));
    return;
  }

  // If the page will not handle the drop, no need to perform content analysis.
  if (!drop_data.document_is_handling_drag) {
    std::move(callback).Run(std::move(drop_data));
    return;
  }

  data.reason = enterprise_connectors::ContentAnalysisRequest::DRAG_AND_DROP;

  // Collect the data that needs to be scanned.
  if (!drop_data.url_title.empty())
    data.text.push_back(base::UTF16ToUTF8(drop_data.url_title));
  if (drop_data.text)
    data.text.push_back(base::UTF16ToUTF8(*drop_data.text));
  if (drop_data.html)
    data.text.push_back(base::UTF16ToUTF8(*drop_data.html));

  // `callback` should only run asynchronously when scanning is blocking.
  content::WebContentsViewDelegate::DropCompletionCallback scan_callback =
      base::DoNothing();
  if (data.settings.block_until_verdict ==
      enterprise_connectors::BlockUntilVerdict::kBlock) {
    scan_callback = std::move(callback);
  }

  // `handle_drop_scan_data` is created on the heap to stay alive regardless of
  // how long the threadpool work takes or in case `web_contents` is destroyed.
  // It deletes itself when `HandleDropScanData::ScanData` is called or when
  // `web_contents` gets destroyed.
  auto* handle_drop_scan_data = new HandleDropScanData(
      web_contents, drop_data, std::move(data), std::move(scan_callback));
  if (drop_data.filenames.empty()) {
    handle_drop_scan_data->ScanData(/*files_scan_data=*/nullptr);
  } else {
    auto files_scan_data =
        std::make_unique<enterprise_connectors::FilesScanData>(
            drop_data.filenames);
    auto* files_scan_data_raw = files_scan_data.get();
    files_scan_data_raw->ExpandPaths(base::BindOnce(
        &HandleDropScanData::ScanData, handle_drop_scan_data->GetWeakPtr(),
        std::move(files_scan_data)));
  }

  if (!callback.is_null()) {
    std::move(callback).Run(std::move(drop_data));
  }
}
