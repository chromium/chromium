// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_HEADER_VIEW_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_HEADER_VIEW_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/callback_list.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"

class Profile;

namespace ash {
namespace sharesheet {

// The SharesheetHeaderView class is the view for the image
// previews feature.
class SharesheetHeaderView : public views::View {
 public:
  METADATA_HEADER(SharesheetHeaderView);

  explicit SharesheetHeaderView(apps::mojom::IntentPtr intent,
                                Profile* profile,
                                bool show_content_previews);
  ~SharesheetHeaderView() override;
  SharesheetHeaderView(const SharesheetHeaderView&) = delete;
  SharesheetHeaderView& operator=(const SharesheetHeaderView&) = delete;

 private:
  class SharesheetImagePreview;

  enum class TextPlaceholderIcon {
    kGenericText = 0,
    kLink,
  };

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

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
  const gfx::VectorIcon& GetTextVectorIcon();

  // TODO(crbug.com/1233830): Move business logic out of UI code.
  void ResolveImages();
  void ResolveImage(size_t index);
  void LoadImage(const base::FilePath& file_path,
                 const gfx::Size& size,
                 HoldingSpaceImage::BitmapCallback callback);
  void OnImageLoaded(const gfx::Size& size, size_t index);

  const base::FilePath GetFilePathFromFileSystemUrl(
      const GURL& file_system_url);

  // Contains the share title and text preview views.
  views::View* text_view_ = nullptr;
  SharesheetImagePreview* image_preview_;
  // |text_icon_| is only used when we have no icons to show in the image
  // preview.
  TextPlaceholderIcon text_icon_ = TextPlaceholderIcon::kGenericText;

  Profile* profile_;
  apps::mojom::IntentPtr intent_;

  ThumbnailLoader thumbnail_loader_;
  std::vector<base::CallbackListSubscription> image_subscription_;
  // TODO(crbug.com/1156343): Clean up to use our own FileThumbnailImage class.
  std::vector<std::unique_ptr<HoldingSpaceImage>> images_;

  base::WeakPtrFactory<SharesheetHeaderView> weak_ptr_factory_{this};
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_HEADER_VIEW_H_
