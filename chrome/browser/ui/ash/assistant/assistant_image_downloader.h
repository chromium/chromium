// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_IMAGE_DOWNLOADER_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_IMAGE_DOWNLOADER_H_

#include "ash/public/cpp/assistant/assistant_image_downloader.h"
#include "base/macros.h"

class AccountId;

// AssistantImageDownloader is the class responsible for downloading images on
// behalf of Assistant UI in ash.
class AssistantImageDownloader : public ash::AssistantImageDownloader {
 public:
  AssistantImageDownloader();
  ~AssistantImageDownloader() override;

  // ash::AssistantImageDownloader:
  void Download(
      const AccountId& account_id,
      const GURL& url,
      ash::AssistantImageDownloader::DownloadCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantImageDownloader);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_IMAGE_DOWNLOADER_H_
