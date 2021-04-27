// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_

#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Profile;

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {
namespace sharesheet {

// The SharesheetContentPreviews class is the view for the image
// previews feature.
class SharesheetContentPreviews : public views::View {
 public:
  METADATA_HEADER(SharesheetContentPreviews);

  explicit SharesheetContentPreviews(apps::mojom::IntentPtr intent,
                                     Profile* profile,
                                     std::unique_ptr<views::Label> share_title);
  ~SharesheetContentPreviews() override;
  SharesheetContentPreviews(const SharesheetContentPreviews&) = delete;
  SharesheetContentPreviews& operator=(const SharesheetContentPreviews&) =
      delete;

 private:
  // Adds the view for image previews and sets the required properties.
  void InitaliseImageView();

  // Adds the view for text preview.
  void ShowTextPreview();

  // Creates a new Label view and adds styling.
  void AddTextLine(const std::u16string& text,
                   const std::u16string& tooltip_text = u"");

  // Parses the share_text attribute for each individual url and text
  // from the intent struct and returns the result in a vector.
  //
  // TODO(crbug.com/2650014): Move the existing ExtractSharedFields function
  // from share_target_utils.h to a common place and reuse the function here.
  std::vector<std::u16string> ExtractShareText();

  void LoadImage();
  void OnImageLoaded(const SkBitmap* bitmap, base::File::Error error);

  // Contains the share title and text preview views.
  views::View* text_view_ = nullptr;
  views::ImageView* image_preview_ = nullptr;

  Profile* profile_;
  apps::mojom::IntentPtr intent_;
  ThumbnailLoader thumbnail_loader_;

  base::WeakPtrFactory<SharesheetContentPreviews> weak_ptr_factory_{this};
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONTENT_PREVIEWS_H_
