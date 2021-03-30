// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"

namespace {

void CompletionCallback(
    content::WebContentsViewDelegate::DropCompletionCallback callback,
    const enterprise_connectors::ContentAnalysisDelegate::Data& data,
    const enterprise_connectors::ContentAnalysisDelegate::Result& result) {
  // If any result is negative, block the drop.
  const auto all_true_fn = [](const auto& vec) {
    return std::all_of(vec.cbegin(), vec.cend(), [](bool b) { return b; });
  };
  bool all_true =
      all_true_fn(result.text_results) && all_true_fn(result.paths_results);

  std::move(callback).Run(
      all_true
          ? content::WebContentsViewDelegate::DropCompletionResult::kContinue
          : content::WebContentsViewDelegate::DropCompletionResult::kAbort);
}

enterprise_connectors::ContentAnalysisDelegate::Data GetPathsToScan(
    content::WebContents* web_contents,
    const content::DropData& drop_data,
    enterprise_connectors::ContentAnalysisDelegate::Data data) {
  for (const auto& file : drop_data.filenames) {
    base::File::Info info;

    // Ignore the path if it's a symbolic link.
    if (!base::GetFileInfo(file.path, &info) || info.is_symbolic_link)
      continue;

    // If the file is a directory, recursively add the files it holds to |data|.
    if (info.is_directory) {
      base::FileEnumerator file_enumerator(file.path, /*recursive=*/true,
                                           base::FileEnumerator::FILES);
      for (base::FilePath sub_path = file_enumerator.Next(); !sub_path.empty();
           sub_path = file_enumerator.Next()) {
        data.paths.push_back(sub_path);
      }
    } else {
      data.paths.push_back(file.path);
    }
  }

  return data;
}

void ScanData(content::WebContents* web_contents,
              content::WebContentsViewDelegate::DropCompletionCallback callback,
              enterprise_connectors::ContentAnalysisDelegate::Data data) {
  enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
      web_contents, std::move(data),
      base::BindOnce(&CompletionCallback, std::move(callback)),
      safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP);
}

}  // namespace

void HandleOnPerformDrop(
    content::WebContents* web_contents,
    const content::DropData& drop_data,
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
    std::move(callback).Run(
        content::WebContentsViewDelegate::DropCompletionResult::kContinue);
    return;
  }

  // Collect the data that needs to be scanned.
  if (!drop_data.url_title.empty())
    data.text.push_back(drop_data.url_title);
  if (drop_data.text)
    data.text.push_back(*drop_data.text);
  if (drop_data.html)
    data.text.push_back(*drop_data.html);
  if (!drop_data.file_contents.empty())
    data.text.push_back(base::UTF8ToUTF16(drop_data.file_contents));

  if (drop_data.filenames.empty()) {
    ScanData(web_contents, std::move(callback), std::move(data));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&GetPathsToScan, web_contents, std::move(drop_data),
                       std::move(data)),
        base::BindOnce(&ScanData, web_contents, std::move(callback)));
  }
}
