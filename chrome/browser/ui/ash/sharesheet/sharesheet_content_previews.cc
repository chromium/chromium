// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_content_previews.h"

#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

constexpr SkColor kShareTitleColor = gfx::kGoogleGrey900;
constexpr SkColor kTitlePreviewColor = gfx::kGoogleGrey700;

SharesheetContentPreviews::SharesheetContentPreviews(
    apps::mojom::IntentPtr intent) {
  intent_ = std::move(intent);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /* inside_border_insets */ gfx::Insets(),
      /* between_child_spacing */ 0, /* collapse_margins_spacing */ true));

  ShowShareTitle();

  // SharesheetContentPreviews will display the Share title as
  // well as preview of the shared data when the
  // features::kSharesheetContentPreviews flag is enabled.
  if (base::FeatureList::IsEnabled(features::kSharesheetContentPreviews)) {
    ShowFileTitlePreview();
  }
}

SharesheetContentPreviews::~SharesheetContentPreviews() = default;

void SharesheetContentPreviews::ShowShareTitle() {
  auto share_title = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL),
      ash::CONTEXT_SHARESHEET_BUBBLE_TITLE, ash::STYLE_SHARESHEET);
  auto* share_title_ = AddChildView(std::move(share_title));
  share_title_->SetLineHeight(SharesheetBubbleView::kTitleLineHeight);
  share_title_->SetEnabledColor(kShareTitleColor);
  share_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  share_title_->SetProperty(views::kMarginsKey,
                            gfx::Insets(SharesheetBubbleView::kSpacing,
                                        SharesheetBubbleView::kSpacing, 0,
                                        SharesheetBubbleView::kSpacing));
}

void SharesheetContentPreviews::ShowFileTitlePreview() {
  if (intent_->file_urls.has_value()) {
    auto* file_title = AddChildView(std::make_unique<views::Label>(
        base::ASCIIToUTF16(
            (intent_->file_urls.value().front().ExtractFileName())),
        ash::CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY));
    file_title->SetLineHeight(SharesheetBubbleView::kTitleLineHeight);
    file_title->SetEnabledColor(kTitlePreviewColor);
    file_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    file_title->SetProperty(views::kMarginsKey,
                            gfx::Insets(3, SharesheetBubbleView::kSpacing,
                                        SharesheetBubbleView::kSpacing,
                                        SharesheetBubbleView::kSpacing));
  }
}

void SharesheetContentPreviews::ShowImagePreview() {
  // TODO(crbug.com/2631548): Add code to add a new
  // image view to show image preview to the
  // content previews view.
  NOTIMPLEMENTED();
}
