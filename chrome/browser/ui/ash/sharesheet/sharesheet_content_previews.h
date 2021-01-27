// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_

#include "chrome/browser/ui/ash/sharesheet/sharesheet_image_decode.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/views/view.h"

class Profile;

// The SharesheetContentPreviews class is the view for the image
// previews feature.
class SharesheetContentPreviews : public views::View {
 public:
  explicit SharesheetContentPreviews(apps::mojom::IntentPtr intent,
                                     Profile* profile);
  ~SharesheetContentPreviews() override;
  SharesheetContentPreviews(const SharesheetContentPreviews&) = delete;
  SharesheetContentPreviews& operator=(const SharesheetContentPreviews&) =
      delete;

 private:
  // Adds the share title to the view.
  void ShowShareTitle();

  // Adds the title preview to the view.
  void ShowFileTitlePreview();

  // Invokes the image decoder to run tasks
  // which will decode the image preview.
  void ExecuteImageDecoder();

  // Adds the image preview to the view.
  void OnImageDecoded(gfx::ImageSkia image);

  // Reference to the sharesheet profile.
  Profile* profile_;
  apps::mojom::IntentPtr intent_;
  SharesheetImageDecode decoder_;

  base::WeakPtrFactory<SharesheetContentPreviews> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_
