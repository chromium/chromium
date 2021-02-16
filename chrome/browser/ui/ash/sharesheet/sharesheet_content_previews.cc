// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_content_previews.h"

#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr char kImagePrefix[] = "image/";

constexpr SkColor kTitlePreviewColor = gfx::kGoogleGrey700;

// This is the left inset used for the distance between the Share text and the
// image preview.
constexpr int kSmallSpacing = 10;
constexpr int kImagePreviewSize = 50;
}  // namespace

SharesheetContentPreviews::SharesheetContentPreviews(
    apps::mojom::IntentPtr intent,
    Profile* profile,
    std::unique_ptr<views::Label> share_title)
    : profile_(profile), intent_(std::move(intent)) {
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

  // Decode image if the intent contains an image.
  if ((intent_->mime_type.has_value()) &&
      base::StartsWith(intent_->mime_type.value(), kImagePrefix,
                       base::CompareCase::SENSITIVE)) {
    ExecuteImageDecoder();
  } else {
    // TODO(crbug.com/2650014): call a function that determines what icon is
    // displayed based on the content type.
    // The code below provides a temporary placeholder icon.
    image_preview_->SetImage(gfx::CreateVectorIcon(kAddIcon));
  }
}

SharesheetContentPreviews::~SharesheetContentPreviews() = default;

void SharesheetContentPreviews::InitaliseImageView() {
  image_preview_ = AddChildView(std::make_unique<views::ImageView>());
  image_preview_->SetProperty(views::kMarginsKey,
                              gfx::Insets(SharesheetBubbleView::kSpacing,
                                          SharesheetBubbleView::kSpacing,
                                          SharesheetBubbleView::kSpacing, 0));
  image_preview_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  image_preview_->SetImageSize(gfx::Size(kImagePreviewSize, kImagePreviewSize));
}

void SharesheetContentPreviews::ShowTextPreview() {
  if (intent_->file_urls.has_value() && !(intent_->file_urls.value().empty())) {
    // TODO(crbug.com/2650014): add logic to handle other content types ie. url
    // and text.
    auto* file_title =
        content_view_->AddChildView(std::make_unique<views::Label>(
            base::ASCIIToUTF16(
                (intent_->file_urls.value().front().ExtractFileName())),
            ash::CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY));
    file_title->SetLineHeight(SharesheetBubbleView::kTitleLineHeight);
    file_title->SetEnabledColor(kTitlePreviewColor);
    file_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    file_title->SetProperty(
        views::kMarginsKey,
        gfx::Insets(0, kSmallSpacing, SharesheetBubbleView::kSpacing,
                    SharesheetBubbleView::kSpacing));
  }
}

void SharesheetContentPreviews::ExecuteImageDecoder() {
  // Invokes the image decoder and executes OnImageDecoded
  // upon completion.
  image_decoder_.DecodeImage(
      intent_->Clone(), profile_,
      base::BindOnce(&SharesheetContentPreviews::OnImageDecoded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharesheetContentPreviews::OnImageDecoded(gfx::ImageSkia image) {
  image_preview_->SetImage(image);
}
