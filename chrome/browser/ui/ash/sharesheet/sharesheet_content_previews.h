// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_

#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/views/view.h"

// The SharesheetContentPreviews class is the view for the image
// previews feature.
class SharesheetContentPreviews : public views::View {
 public:
  explicit SharesheetContentPreviews(apps::mojom::IntentPtr intent);
  ~SharesheetContentPreviews() override;
  SharesheetContentPreviews(const SharesheetContentPreviews&) = delete;
  SharesheetContentPreviews& operator=(const SharesheetContentPreviews&) =
      delete;

 private:
  // Adds the title preview to the view.
  void SetTitlePreview();

  // Adds the image preview to the view.
  void SetImagePreview();

  apps::mojom::IntentPtr intent_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_
