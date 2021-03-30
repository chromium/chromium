// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_

#include "chrome/browser/ui/ash/sharesheet/sharesheet_image_decoder.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/views/view.h"

class Profile;

namespace views {
class ImageView;
class Label;
}  // namespace views

// The SharesheetContentPreviews class is the view for the image
// previews feature.
class SharesheetContentPreviews : public views::View {
 public:
  explicit SharesheetContentPreviews(apps::mojom::IntentPtr intent,
                                     Profile* profile,
                                     std::unique_ptr<views::Label> share_title);
  ~SharesheetContentPreviews() override;
  SharesheetContentPreviews(const SharesheetContentPreviews&) = delete;
  SharesheetContentPreviews& operator=(const SharesheetContentPreviews&) =
      delete;

  // Returns the height of the text preview view.
  int GetTitleViewHeight();

 private:
  // Adds the view for image previews and sets the required properties.
  void InitaliseImageView();

  // Adds the view for text preview.
  void ShowTextPreview();

  // Creates a new Label view and adds styling.
  void AddTextLine(std::string text, int bottom_spacing);

  // Parses the share_text attribute for each individual url and text
  // from the intent struct and returns the result in a vector.
  //
  // TODO(crbug.com/2650014): Move the existing ExtractSharedFields function
  // from share_target_utils.h to a common place and reuse the function here.
  std::vector<std::string> ExtractShareText();

  // Invokes the image decoder to run tasks
  // which will decode the image preview.
  void ExecuteImageDecoder();

  // Adds the image preview to the view.
  void OnImageDecoded(gfx::ImageSkia image);

  // Contains the share title and text preview views.
  views::View* content_view_ = nullptr;
  views::ImageView* image_preview_ = nullptr;

  Profile* profile_;
  apps::mojom::IntentPtr intent_;
  SharesheetImageDecoder image_decoder_;

  base::WeakPtrFactory<SharesheetContentPreviews> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_
