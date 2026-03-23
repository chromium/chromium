// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"

AiOverlayDialogPageHandler::AiOverlayDialogPageHandler(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
    mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote)
    : receiver_(this, std::move(receiver)), page_(std::move(remote)) {}

AiOverlayDialogPageHandler::~AiOverlayDialogPageHandler() = default;

void AiOverlayDialogPageHandler::GetApiKey(GetApiKeyCallback callback) {
  std::move(callback).Run(features::kAiOverlayDialogApiKey.Get());
}

void AiOverlayDialogPageHandler::GetMockAudioData(
    GetMockAudioDataCallback callback) {
  std::string path_string = features::kAiOverlayDialogMockJsonPath.Get();
  std::replace(path_string.begin(), path_string.end(), '+', '/');
  if (path_string.empty()) {
    VLOG(1) << "MockAudioData path not specified";
    std::move(callback).Run(std::nullopt);
    return;
  }

  VLOG(1) << "Using MockAudioData from: " << path_string;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const std::string& path_string) -> std::optional<std::string> {
            std::string data;
            if (!base::ReadFileToString(
                    base::FilePath::FromUTF8Unsafe(path_string), &data)) {
              return std::nullopt;
            }

            VLOG(1) << "\tMockAudioData head: " << data.substr(0, 100);
            return data;
          },
          path_string),
      std::move(callback));
}

void AiOverlayDialogPageHandler::InvalidatePageContext() {
  VLOG(1) << "Invalidate Page Context";
  page_->InvalidatePageContext();
}

void AiOverlayDialogPageHandler::UpdateCurrentPageContext(
    const GURL& url,
    const std::u16string& title,
    const std::string& content) {
  VLOG(1) << "Update Current Page Context";
  VLOG(1) << "\tURL: " << url.spec();
  VLOG(1) << "\tTitle: " << base::UTF16ToUTF8(title);
  VLOG(1) << "\tContent: " << content.substr(0, 200) << "...";

  page_->UpdateCurrentPageContext(url.spec(), base::UTF16ToUTF8(title),
                                  content);
}
