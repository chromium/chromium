// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_header_view.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/file_icon_util.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_constants.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace {

// Concatenates all the strings in |file_names| with a comma delineator.
const std::u16string ConcatenateFileNames(
    const std::vector<std::string>& file_names) {
  auto all_file_names = base::JoinString(file_names, ", ");
  return base::ASCIIToUTF16(all_file_names);
}

gfx::ImageSkia CreatePlaceholderIcon(const gfx::VectorIcon& icon) {
  gfx::ImageSkia file_type_icon = gfx::CreateVectorIcon(
      icon, ash::sharesheet::kImagePreviewPlaceholderIconContentSize,
      ash::sharesheet::kImagePreviewPlaceholderIconColor);
  return ash::HoldingSpaceImage::SuperimposeOverEmptyImage(
      file_type_icon, ash::sharesheet::kImagePreviewSize);
}

}  // namespace

namespace ash {
namespace sharesheet {

SharesheetHeaderView::SharesheetHeaderView(apps::mojom::IntentPtr intent,
                                           Profile* profile)
    : profile_(profile),
      intent_(std::move(intent)),
      thumbnail_loader_(profile) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      /* inside_border_insets */ gfx::Insets(kSpacing),
      /* between_child_spacing */ kHeaderViewBetweenChildSpacing,
      /* collapse_margins_spacing */ false));
  // Sets all views to be left-aligned.
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  // Sets all views to be vertically centre-aligned.
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // The image view is initialised first to ensure its left most placement.
  if (base::FeatureList::IsEnabled(features::kSharesheetContentPreviews)) {
    InitaliseImageView();
  }
  // A separate view is created for the share title and preview string views.
  text_view_ = AddChildView(std::make_unique<views::View>());
  text_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /* inside_border_insets */ gfx::Insets(),
      /* between_child_spacing */ 0, /* collapse_margins_spacing */ true));
  text_view_->AddChildView(
      CreateShareLabel(l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL),
                       CONTEXT_SHARESHEET_BUBBLE_TITLE, kTitleTextLineHeight,
                       kTitleTextColor, gfx::ALIGN_LEFT));
  if (base::FeatureList::IsEnabled(features::kSharesheetContentPreviews)) {
    ShowTextPreview();

    if (intent_->file_urls.has_value() && !intent_->file_urls.value().empty()) {
      ResolveImage();
    } else {
      // TODO(crbug.com/2650014): Update to text icon.
      image_preview_->SetImage(
          CreatePlaceholderIcon(chromeos::kFiletypeGenericIcon));
    }
  }
}

SharesheetHeaderView::~SharesheetHeaderView() = default;

void SharesheetHeaderView::InitaliseImageView() {
  image_preview_ = AddChildView(std::make_unique<views::ImageView>());
  image_preview_->SetImageSize(kImagePreviewSize);
  image_preview_->SetPaintToLayer();
  image_preview_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kImagePreviewCornerRadius));
  image_preview_->SetBackground(
      views::CreateSolidBackground(kImagePreviewPlaceholderBackgroundColor));
}

void SharesheetHeaderView::ShowTextPreview() {
  std::vector<std::u16string> text_fields = ExtractShareText();

  std::u16string filenames_tooltip_text = u"";
  if (intent_->file_urls.has_value() && !intent_->file_urls.value().empty()) {
    std::vector<std::string> file_names;
    for (const auto& file_url : intent_->file_urls.value()) {
      file_names.push_back(file_url.ExtractFileName());
    }
    std::u16string file_text;
    if (file_names.size() == 1) {
      file_text = base::ASCIIToUTF16(file_names[0]);
    } else {
      // If there is more than 1 file, show an enumeration of the number of
      // files.
      auto size = intent_->file_urls.value().size();
      DCHECK_NE(size, 0);
      file_text =
          l10n_util::GetPluralStringFUTF16(IDS_SHARESHEET_FILES_LABEL, size);
      filenames_tooltip_text = ConcatenateFileNames(file_names);
    }
    text_fields.push_back(file_text);
  }

  int index = 0;
  int max_lines = std::min(text_fields.size(), kTextPreviewMaximumLines);
  for (; index < max_lines - 1; ++index) {
    AddTextLine(text_fields[index]);
  }
  // File names must always be on the last line, so |filenames_tooltip_text| is
  // only passed in on the last line of text. If there are no files, it will be
  // empty and the tooltip will instead be set to what the text says.
  DCHECK_LT(index, text_fields.size());
  AddTextLine(text_fields[index], filenames_tooltip_text);

  // If we have 2 or more lines of text, shorten the vertical insets.
  if (index >= 1) {
    static_cast<views::BoxLayout*>(GetLayoutManager())
        ->set_inside_border_insets(
            gfx::Insets(/* vertical */ kHeaderViewNarrowInsideBorderInsets,
                        /* horizontal */ kSpacing));
  }
}

void SharesheetHeaderView::AddTextLine(const std::u16string& text,
                                       const std::u16string& tooltip_text) {
  auto* new_line = text_view_->AddChildView(
      std::make_unique<views::Label>(text, CONTEXT_SHARESHEET_BUBBLE_BODY));
  new_line->SetLineHeight(kPrimaryTextLineHeight);
  new_line->SetEnabledColor(kPrimaryTextColor);
  new_line->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  new_line->SetHandlesTooltips(true);
  if (tooltip_text.empty()) {
    return;
  }
  new_line->SetTooltipText(tooltip_text);
  // We only get to here if this line is showing the number of files.
  // By default the accessible name is set to the label text. We set it here
  // so that it is also gives the list of file names.
  new_line->SetAccessibleName(
      base::StrCat({new_line->GetText(), u" ", tooltip_text}));
}

std::vector<std::u16string> SharesheetHeaderView::ExtractShareText() {
  std::vector<std::u16string> text_fields;

  if (intent_->share_title.has_value() &&
      !(intent_->share_title.value().empty())) {
    std::string title_text = intent_->share_title.value();
    text_fields.push_back(base::ASCIIToUTF16(title_text));
  }

  if (intent_->drive_share_url.has_value()) {
    GURL drive_share_url = intent_->drive_share_url.value();
    if (drive_share_url.is_valid()) {
      text_fields.push_back(base::ASCIIToUTF16(drive_share_url.spec()));
    }
  }

  if (intent_->share_text.has_value() &&
      !(intent_->share_text.value().empty())) {
    std::string extracted_text = intent_->share_text.value();
    size_t last_space = extracted_text.find_last_of(' ');
    GURL extracted_url;

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
      text_fields.push_back(base::ASCIIToUTF16(extracted_text));

    if (extracted_url.is_valid())
      text_fields.push_back(base::ASCIIToUTF16(extracted_url.spec()));
  }

  // There will always be at least 1 text field.
  DCHECK_NE(text_fields.size(), 0);
  return text_fields;
}

// TODO(crbug.com/2650014) Optimise to load several images.
void SharesheetHeaderView::ResolveImage() {
  base::FilePath file_path;
  storage::FileSystemContext* fs_context =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile_, file_manager::kFileManagerAppId);
  storage::FileSystemURL fs_url =
      fs_context->CrackURL(intent_->file_urls.value().front());
  file_path = fs_url.path();

  image_ = std::make_unique<HoldingSpaceImage>(
      kImagePreviewSize, file_path,
      base::BindRepeating(&SharesheetHeaderView::LoadImage,
                          weak_ptr_factory_.GetWeakPtr()),
      base::Optional<gfx::ImageSkia>(
          CreatePlaceholderIcon(chromeos::kFiletypeImageIcon)));
  image_subscription_ = image_->AddImageSkiaChangedCallback(base::BindRepeating(
      &SharesheetHeaderView::OnImageLoaded, weak_ptr_factory_.GetWeakPtr()));
  image_preview_->SetImage(image_->GetImageSkia(kImagePreviewSize));
}

void SharesheetHeaderView::LoadImage(
    const base::FilePath& file_path,
    const gfx::Size& size,
    HoldingSpaceImage::BitmapCallback callback) {
  // This works for all shares right now because currently when we share data
  // that is not from the Files app (web share and ARC),
  // those files are being temporarily saved to disk before being shared.
  // If those implementations change, this will need to be updated.
  thumbnail_loader_.Load({file_path, size}, std::move(callback));
}

void SharesheetHeaderView::OnImageLoaded() {
  image_preview_->SetImage(image_->GetImageSkia(kImagePreviewSize));
}

BEGIN_METADATA(SharesheetHeaderView, views::View)
END_METADATA

}  // namespace sharesheet
}  // namespace ash
