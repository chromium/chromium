// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"

namespace {

void DeepScanCompletionCallback(
    content::WebContentsViewDelegate::DropCompletionCallback callback,
    const safe_browsing::DeepScanningDialogDelegate::Data& data,
    const safe_browsing::DeepScanningDialogDelegate::Result& result) {
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

}  // namespace

void HandleOnPerformDrop(
    content::WebContents* web_contents,
    const content::DropData& drop_data,
    content::WebContentsViewDelegate::DropCompletionCallback callback) {
  safe_browsing::DeepScanningDialogDelegate::Data data;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!safe_browsing::DeepScanningDialogDelegate::IsEnabled(
          profile, web_contents->GetLastCommittedURL(), &data)) {
    std::move(callback).Run(
        content::WebContentsViewDelegate::DropCompletionResult::kContinue);
    return;
  }

  // Collect the data that needs to be scanned.
  if (!drop_data.url_title.empty())
    data.text.push_back(drop_data.url_title);
  if (!drop_data.text.is_null())
    data.text.push_back(drop_data.text.string());
  if (!drop_data.html.is_null())
    data.text.push_back(drop_data.html.string());
  if (!drop_data.file_contents.empty())
    data.text.push_back(base::UTF8ToUTF16(drop_data.file_contents));

  for (const auto& file : drop_data.filenames)
    data.paths.push_back(file.path);

  // TODO(crbug.com/1008040): how to handle drop_data.file_system_files?
  // These are URLs that use the filesystem: schema.  Support for this API
  // is unclear.

  safe_browsing::DeepScanningDialogDelegate::ShowForWebContents(
      web_contents, std::move(data),
      base::BindOnce(&DeepScanCompletionCallback, std::move(callback)));
}
