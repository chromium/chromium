// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/file_info.h"

namespace {

// Helper to keep a mapping of files to the index of their corresponding parent
// entry in content::DropData::filenames. For instance, this means that for a
// DropData with filenames = [ "a.txt", "dir/"], `PathsAndIndexes` might be
// populated with { "a.txt": 0, "dir/sub_1.txt": 1, "dir/sub_2.txt": 1 }.
using PathsAndIndexes = std::map<base::FilePath, size_t>;

// Helper struct to hold all relevant data to a drag-drop content analysis scan.
struct ContentAnalysisDropData {
  enterprise_connectors::ContentAnalysisDelegate::Data analysis_data;
  PathsAndIndexes paths_and_indexes;
};

void CompletionCallback(
    content::DropData drop_data,
    PathsAndIndexes paths_and_indexes,
    content::WebContentsViewDelegate::DropCompletionCallback callback,
    const enterprise_connectors::ContentAnalysisDelegate::Data& data,
    const enterprise_connectors::ContentAnalysisDelegate::Result& result) {
  // If there are no negative results, proceed with just `drop_data`.
  bool all_text_results_allowed = !base::Contains(result.text_results, false);
  bool all_file_results_allowed = !base::Contains(result.paths_results, false);
  if (all_text_results_allowed && all_file_results_allowed) {
    std::move(callback).Run(std::move(drop_data));
    return;
  }

  // For text drag-drops, block the drop if any result is negative.
  if (!all_text_results_allowed) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // For file drag-drops, block file paths depending on the verdict obtained for
  // child paths.
  DCHECK_EQ(paths_and_indexes.size(), result.paths_results.size());
  std::set<size_t> file_indexes_to_filter;
  for (size_t i = 0; i < result.paths_results.size(); ++i) {
    if (result.paths_results[i])
      continue;
    file_indexes_to_filter.insert(paths_and_indexes.at(data.paths[i]));
  }

  // If every file path should be filtered, the drop is aborted, otherwise it
  // continues by filtering the list.
  if (file_indexes_to_filter.size() == drop_data.filenames.size()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::vector<ui::FileInfo> final_filenames;
  for (size_t i = 0; i < drop_data.filenames.size(); ++i) {
    if (file_indexes_to_filter.count(i))
      continue;
    final_filenames.push_back(std::move(drop_data.filenames[i]));
  }

  drop_data.filenames = std::move(final_filenames);
  std::move(callback).Run(std::move(drop_data));
}

ContentAnalysisDropData GetPathsToScan(
    const std::vector<ui::FileInfo>& filenames,
    enterprise_connectors::ContentAnalysisDelegate::Data data) {
  ContentAnalysisDropData content_analysis_drop_data;
  for (size_t i = 0; i < filenames.size(); ++i) {
    const ui::FileInfo& file = filenames.at(i);
    base::File::Info info;

    // Ignore the path if it's a symbolic link.
    if (!base::GetFileInfo(file.path, &info) || info.is_symbolic_link)
      continue;

    // If the file is a directory, recursively add the files it holds to `data`.
    if (info.is_directory) {
      base::FileEnumerator file_enumerator(file.path, /*recursive=*/true,
                                           base::FileEnumerator::FILES);
      for (base::FilePath sub_path = file_enumerator.Next(); !sub_path.empty();
           sub_path = file_enumerator.Next()) {
        data.paths.push_back(sub_path);
        content_analysis_drop_data.paths_and_indexes.insert({sub_path, i});
      }
    } else {
      data.paths.push_back(file.path);
      content_analysis_drop_data.paths_and_indexes.insert({file.path, i});
    }
  }

  content_analysis_drop_data.analysis_data = std::move(data);

  return content_analysis_drop_data;
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
      content::WebContentsViewDelegate::DropCompletionCallback callback)
      : content::WebContentsObserver(web_contents),
        drop_data_(std::move(drop_data)),
        callback_(std::move(callback)) {}

  void ScanData(ContentAnalysisDropData content_analysis_drop_data) {
    DCHECK(web_contents());

    enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
        web_contents(), std::move(content_analysis_drop_data.analysis_data),
        base::BindOnce(&CompletionCallback, std::move(drop_data_),
                       std::move(content_analysis_drop_data.paths_and_indexes),
                       std::move(callback_)),
        safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP);

    delete this;
  }

  void WebContentsDestroyed() override { delete this; }

  base::WeakPtr<HandleDropScanData> GetWeakPtr() {
    return weakptr_factory_.GetWeakPtr();
  }

 private:
  content::DropData drop_data_;
  content::WebContentsViewDelegate::DropCompletionCallback callback_;

  base::WeakPtrFactory<HandleDropScanData> weakptr_factory_{this};
};

}  // namespace

void HandleOnPerformDrop(
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
    std::move(callback).Run(std::move(drop_data));
    return;
  }

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

  auto* handle_drop_scan_data =
      new HandleDropScanData(web_contents, drop_data, std::move(scan_callback));
  if (drop_data.filenames.empty()) {
    handle_drop_scan_data->ScanData({
        .analysis_data = std::move(data),
        .paths_and_indexes = {},
    });
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&GetPathsToScan, drop_data.filenames, std::move(data)),
        base::BindOnce(&HandleDropScanData::ScanData,
                       handle_drop_scan_data->GetWeakPtr()));
  }

  if (!callback.is_null()) {
    std::move(callback).Run(std::move(drop_data));
  }
}
