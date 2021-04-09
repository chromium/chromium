// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_content_previews.h"

#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "base/files/file_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr SkColor kTitlePreviewColor = gfx::kGoogleGrey700;

// This is the left inset used for the distance between the Share text and the
// image preview.
constexpr int kSmallSpacing = 10;
}  // namespace

SharesheetContentPreviews::SharesheetContentPreviews(
    apps::mojom::IntentPtr intent,
    Profile* profile,
    std::unique_ptr<views::Label> share_title)
    : profile_(profile),
      intent_(std::move(intent)),
      thumbnail_loader_(profile) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      /* inside_border_insets */ gfx::Insets(),
      /* between_child_spacing */ 0, /* collapse_margins_spacing */ true));

  // The image view is initialised first to ensure its left most placement.
  InitaliseImageView();

  // A separate view is created for the share title and preview string views.
  content_view_ = AddChildView(std::make_unique<views::View>());
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /* inside_border_insets */ gfx::Insets(),
      /* between_child_spacing */ 0, /* collapse_margins_spacing */ true));

  auto* share_title_view = content_view_->AddChildView(std::move(share_title));
  share_title_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets(SharesheetBubbleView::kSpacing, kSmallSpacing, 0,
                  SharesheetBubbleView::kSpacing));
  ShowTextPreview();

  if (intent_->file_urls.has_value() && !intent_->file_urls.value().empty()) {
    LoadImage();
  } else {
    // TODO(crbug.com/2650014): Update to text icon.
    image_preview_->SetImage(gfx::CreateVectorIcon(kAddIcon));
  }
}

SharesheetContentPreviews::~SharesheetContentPreviews() = default;

int SharesheetContentPreviews::GetTitleViewHeight() {
  return content_view_->GetPreferredSize().height();
}

void SharesheetContentPreviews::InitaliseImageView() {
  image_preview_ = AddChildView(std::make_unique<views::ImageView>());
  image_preview_->SetProperty(views::kMarginsKey,
                              gfx::Insets(SharesheetBubbleView::kSpacing,
                                          SharesheetBubbleView::kSpacing,
                                          SharesheetBubbleView::kSpacing, 0));
  image_preview_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  image_preview_->SetImageSize(
      gfx::Size(sharesheet::kIconSize, sharesheet::kIconSize));
}

void SharesheetContentPreviews::ShowTextPreview() {
  // TODO(crbug.com/2650014): call a function that will dynamically resize the
  // image preview thumbnail relative to how many lines of text preview are
  // present.

  // TODO(crbug.com/2650014): Handle case for sharing multiple files. Add an
  // enumeration string to reflect how many files are being sent.

  // TODO(crbug.com/2650014): Handle drive_share_url and share_title fields.

  std::vector<std::string> share_fields;
  if (intent_->share_text.has_value() &&
      !(intent_->share_text.value().empty())) {
    share_fields = ExtractShareText();
  }

  // Files are added last to share_fields so they appear on the 2nd line of text
  // preview if a text/url is also shared.
  if (intent_->file_urls.has_value() && !intent_->file_urls.value().empty() &&
      share_fields.size() < 2) {
    for (const auto& file_url : intent_->file_urls.value()) {
      share_fields.push_back(file_url.ExtractFileName());
    }
  }

  // If there are two items to be shared, only the second line of text preview
  // will have a bottom inset of |kSpacing| to create whitespace from the
  // targets.
  if (share_fields.size() == 1) {
    AddTextLine(share_fields[0], SharesheetBubbleView::kSpacing);
  } else if (share_fields.size() > 1) {
    AddTextLine(share_fields[0], 0);
    AddTextLine(share_fields[1], SharesheetBubbleView::kSpacing);
  }
}

void SharesheetContentPreviews::AddTextLine(std::string text,
                                            int bottom_spacing) {
  auto* new_line = content_view_->AddChildView(std::make_unique<views::Label>(
      (base::ASCIIToUTF16(text)),
      ash::CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY));
  new_line->SetLineHeight(SharesheetBubbleView::kTitleLineHeight);
  new_line->SetEnabledColor(kTitlePreviewColor);
  new_line->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  new_line->SetProperty(views::kMarginsKey,
                        gfx::Insets(0, kSmallSpacing, bottom_spacing,
                                    SharesheetBubbleView::kSpacing));
}

std::vector<std::string> SharesheetContentPreviews::ExtractShareText() {
  std::vector<std::string> result;
  std::string extracted_text = intent_->share_text.value();
  GURL extracted_url;
  size_t last_space = extracted_text.find_last_of(' ');

  if (!intent_->share_text.has_value())
    return result;

  if (last_space == std::string::npos) {
    extracted_url = GURL(extracted_text);
    if (extracted_url.is_valid())
      extracted_text.clear();
  } else {
    extracted_url = GURL(extracted_text.substr(last_space + 1));
    if (extracted_url.is_valid())
      extracted_text.erase(last_space);
  }

  if (!extracted_text.empty())
    result.push_back(extracted_text);

  if (extracted_url.is_valid())
    result.push_back(extracted_url.spec());

  return result;
}

// TODO(crbug.com/2650014) Optimise to load several images.
void SharesheetContentPreviews::LoadImage() {
  base::FilePath file_path;
  storage::FileSystemContext* fs_context =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile_, file_manager::kFileManagerAppId);
  storage::FileSystemURL fs_url =
      fs_context->CrackURL(intent_->file_urls.value().front());
  file_path = fs_url.path();

  // This works for all shares right now because currently when we share data
  // that is not from the Files app (web share and ARC),
  // those files are being temporarily saved to disk before being shared.
  // If those implementations change, this will need to be updated.
  thumbnail_loader_.Load(
      {file_path, gfx::Size(sharesheet::kIconSize, sharesheet::kIconSize)},
      base::BindOnce(&SharesheetContentPreviews::OnImageLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharesheetContentPreviews::OnImageLoaded(const SkBitmap* bitmap,
                                              base::File::Error error) {
  if (error != base::File::FILE_OK) {
    // TODO(crbug.com/2650014): Handle error case:
    // Add placeholder icons for each mimetype.
    image_preview_->SetImage(gfx::CreateVectorIcon(kAddIcon));
    return;
  }

  // TODO(crbug.com/1189945): Update to use custom ImageSkiaSource so that
  // image will scale with device scale factor.
  image_preview_->SetImage(
      gfx::Image::CreateFrom1xBitmap(*bitmap).AsImageSkia());
}
