// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_header_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/file_icon_util.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_constants.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/url_formatter/url_formatter.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

// Concatenates all the strings in |file_names| with a comma delineator.
const std::u16string ConcatenateFileNames(
    const std::vector<std::u16string>& file_names) {
  return base::JoinString(file_names, u", ");
}

gfx::ImageSkia CreateMimeTypeIcon(const gfx::ImageSkia& file_type_icon,
                                  const gfx::Size& image_size) {
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(
      ash::image_util::CreateEmptyImage(image_size), file_type_icon);
}

gfx::Size GetImagePreviewSize(size_t index, int grid_icon_count) {
  switch (grid_icon_count) {
    case 1:
      return ash::sharesheet::kImagePreviewFullSize;
    case 2:
      return ash::sharesheet::kImagePreviewHalfSize;
    case 3:
      if (index == 0) {
        return ash::sharesheet::kImagePreviewHalfSize;
      } else {
        return ash::sharesheet::kImagePreviewQuarterSize;
      }
    default:
      return ash::sharesheet::kImagePreviewQuarterSize;
  }
}

}  // namespace

namespace ash {
namespace sharesheet {

// SharesheetHeaderView::SharesheetImagePreview
// ------------------------------------------------------

class SharesheetHeaderView::SharesheetImagePreview : public views::View {
 public:
  METADATA_HEADER(SharesheetImagePreview);
  explicit SharesheetImagePreview(size_t file_count) {
    SetBackground(views::CreateRoundedRectBackground(
        kImagePreviewPlaceholderBackgroundColor,
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::Emphasis::kMedium)));
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        /* inside_border_insets */ gfx::Insets(),
        /* between_child_spacing */ kImagePreviewBetweenChildSpacing));
    SetPreferredSize(kImagePreviewFullSize);
    SetFocusBehavior(View::FocusBehavior::NEVER);

    size_t grid_icon_count =
        (file_count > 0) ? std::min(file_count, kImagePreviewMaxIcons) : 1;
    size_t enumeration = (file_count > kImagePreviewMaxIcons)
                             ? file_count - kImagePreviewMaxIcons + 1
                             : 0;

    if (grid_icon_count == 1) {
      AddImageViewTo(this, kImagePreviewFullSize);
      return;
    }

    // If we need to have more than 1 icon, add two rows so that we can
    // layout the icons in a grid.
    DCHECK_GT(grid_icon_count, 1);
    AddRowToImageContainerView();
    AddRowToImageContainerView();

    ScopedLightModeAsDefault scoped_light_mode_as_default;
    for (size_t index = 0; index < grid_icon_count; ++index) {
      // If we have |enumeration|, add it as a label at the bottom right of
      // SharesheetImagePreview.
      if (enumeration != 0 && index == kImagePreviewMaxIcons - 1) {
        auto* label =
            children()[1]->AddChildView(std::make_unique<views::Label>(
                base::StrCat({u"+", base::NumberToString16(enumeration)}),
                CONTEXT_SHARESHEET_BUBBLE_SMALL, STYLE_SHARESHEET));
        label->SetLineHeight(kImagePreviewFileEnumerationLineHeight);
        label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kButtonLabelColorBlue));
        label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
        label->SetBackground(views::CreateRoundedRectBackground(
            kImagePreviewPlaceholderBackgroundColor,
            kImagePreviewIconCornerRadius));
        label->SetPreferredSize(kImagePreviewQuarterSize);
        return;
      }
      AddImageViewAt(index, grid_icon_count,
                     GetImagePreviewSize(index, grid_icon_count));
    }
  }

  SharesheetImagePreview(const SharesheetImagePreview&) = delete;
  SharesheetImagePreview& operator=(const SharesheetImagePreview&) = delete;

  ~SharesheetImagePreview() override {
    ::sharesheet::SharesheetMetrics::RecordSharesheetImagePreviewPressed(
        was_pressed_);
  }

  views::ImageView* GetImageViewAt(size_t index) {
    if (index >= image_views_.size()) {
      return nullptr;
    }
    return image_views_[index];
  }

  const size_t GetImageViewCount() { return image_views_.size(); }

  void SetBackgroundColorForIndex(const int index, const SkColor& color) {
    auto alpha_color =
        SkColorSetA(color, kImagePreviewBackgroundAlphaComponent);
    image_views_[index]->SetBackground(views::CreateRoundedRectBackground(
        alpha_color, kImagePreviewIconCornerRadius));
  }

 private:
  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    was_pressed_ = true;
    return false;
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP)
      was_pressed_ = true;
  }

  void OnThemeChanged() override {
    View::OnThemeChanged();
    ScopedLightModeAsDefault scoped_light_mode_as_default;
    SetBorder(views::CreateRoundedRectBorder(
        /*thickness=*/1,
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::Emphasis::kMedium),
        GetColorProvider()->GetColor(ui::kColorFocusableBorderUnfocused)));
  }

  void AddRowToImageContainerView() {
    auto* row = AddChildView(std::make_unique<views::View>());
    row->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        /* inside_border_insets */ gfx::Insets(),
        /* between_child_spacing */ kImagePreviewBetweenChildSpacing));
  }

  void AddImageViewTo(views::View* parent_view, const gfx::Size& size) {
    auto* image_view =
        parent_view->AddChildView(std::make_unique<views::ImageView>());
    image_view->SetImageSize(size);
    image_views_.push_back(image_view);
  }

  void AddImageViewAt(size_t index,
                      size_t grid_icon_count,
                      const gfx::Size& size) {
    views::View* parent_view = this;
    if (grid_icon_count > 1) {
      int row_num = 0;
      // For 2 icons, add to the second row for the second icons.
      // For 3 icons, add to the second row for the second and third icons.
      // For 4+ icons, add to the second row for the third and fourth icons.
      if ((grid_icon_count == 2 && index == 1) ||
          (grid_icon_count == 3 && index != 0) ||
          (grid_icon_count >= 4 && index > 1)) {
        row_num = 1;
      }
      parent_view = children()[row_num];
    }
    AddImageViewTo(parent_view, size);
  }

  std::vector<views::ImageView*> image_views_;

  // Used for recording UMA to indicate whether or not a user tried to interact
  // with the image preview.
  bool was_pressed_ = false;
};

BEGIN_METADATA(SharesheetHeaderView, SharesheetImagePreview, views::View)
END_METADATA

// SharesheetHeaderView --------------------------------------------------------

SharesheetHeaderView::SharesheetHeaderView(apps::mojom::IntentPtr intent,
                                           Profile* profile,
                                           bool show_content_previews)
    : profile_(profile),
      intent_(std::move(intent)),
      thumbnail_loader_(profile) {
  SetID(HEADER_VIEW_ID);
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
  SetFocusBehavior(View::FocusBehavior::ALWAYS);

  const bool has_files =
      (intent_->files.has_value() && !intent_->files.value().empty());
  // The image view is initialised first to ensure its left most placement.
  if (show_content_previews) {
    auto file_count = (has_files) ? intent_->files.value().size() : 0;
    image_preview_ =
        AddChildView(std::make_unique<SharesheetImagePreview>(file_count));
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
  if (show_content_previews) {
    ShowTextPreview();
    if (has_files) {
      ResolveImages();
    } else {
      DCHECK_GT(image_preview_->GetImageViewCount(), 0);
      ScopedLightModeAsDefault scoped_light_mode_as_default;
      const auto icon_color = ColorProvider::Get()->GetContentLayerColor(
          ColorProvider::ContentLayerType::kIconColorProminent);
      gfx::ImageSkia file_type_icon = gfx::CreateVectorIcon(
          GetTextVectorIcon(),
          sharesheet::kImagePreviewPlaceholderIconContentSize, icon_color);
      image_preview_->GetImageViewAt(0)->SetImage(
          CreateMimeTypeIcon(file_type_icon, kImagePreviewFullSize));
      image_preview_->SetBackgroundColorForIndex(0, icon_color);
    }
  }
}

SharesheetHeaderView::~SharesheetHeaderView() = default;

void SharesheetHeaderView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGenericContainer;
  node_data->SetNameExplicitlyEmpty();
}

void SharesheetHeaderView::ShowTextPreview() {
  std::vector<std::u16string> text_fields = ExtractShareText();

  std::u16string filenames_tooltip_text = u"";
  if (intent_->files.has_value() && !intent_->files.value().empty()) {
    std::vector<std::u16string> file_names;
    for (const auto& file : intent_->files.value()) {
      auto file_path =
          apps::GetFileSystemURL(profile_, file->url).path();
      file_names.push_back(file_path.BaseName().LossyDisplayName());
    }
    std::u16string file_text;
    if (file_names.size() == 1) {
      file_text = file_names[0];
    } else {
      // If there is more than 1 file, show an enumeration of the number of
      // files.
      auto size = intent_->files.value().size();
      DCHECK_NE(size, 0);
      file_text =
          l10n_util::GetPluralStringFUTF16(IDS_SHARESHEET_FILES_LABEL, size);
      filenames_tooltip_text = ConcatenateFileNames(file_names);
    }
    text_fields.push_back(file_text);
  }

  if (text_fields.size() == 0)
    return;

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
  auto* new_line = text_view_->AddChildView(CreateShareLabel(
      text, CONTEXT_SHARESHEET_BUBBLE_BODY, kPrimaryTextLineHeight,
      kPrimaryTextColor, gfx::ALIGN_LEFT, views::style::STYLE_PRIMARY));
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
    text_fields.push_back(base::UTF8ToUTF16(title_text));
  }

  if (intent_->share_text.has_value()) {
    apps_util::SharedText extracted_text =
        apps_util::ExtractSharedText(intent_->share_text.value());

    if (!extracted_text.text.empty()) {
      text_fields.push_back(base::UTF8ToUTF16(extracted_text.text));
    }

    if (!extracted_text.url.is_empty()) {
      // We format the URL to match the location bar so the user is not
      // surprised by what is being shared. This means:
      // - International characters are unescaped (human readable) where safe.
      // - Characters representing emojis are unescaped.
      // - No elisions that change the meaning of the URL.
      // - Spaces are not unescaped. We cannot share a URL with unescaped spaces
      // as the receiving program may think the URL ends at the space. Hence we
      // align the preview with the content to be shared.
      const auto format_types = url_formatter::kFormatUrlOmitDefaults &
                                ~url_formatter::kFormatUrlOmitHTTP;
      const auto formatted_text = url_formatter::FormatUrl(
          extracted_text.url, format_types, net::UnescapeRule::NORMAL,
          /*new_parsed=*/nullptr,
          /*prefix_end=*/nullptr, /*offset_for_adjustment=*/nullptr);
      text_fields.push_back(formatted_text);
      text_icon_ = TextPlaceholderIcon::kLink;
    }
  }

  return text_fields;
}

const gfx::VectorIcon& SharesheetHeaderView::GetTextVectorIcon() {
  switch (text_icon_) {
    case (TextPlaceholderIcon::kGenericText):
      return kSharesheetTextIcon;
    case (TextPlaceholderIcon::kLink):
      return kSharesheetLinkIcon;
  }
}

void SharesheetHeaderView::ResolveImages() {
  for (int i = 0; i < image_preview_->GetImageViewCount(); ++i) {
    ResolveImage(i);
  }
}

void SharesheetHeaderView::ResolveImage(size_t index) {
  auto file_path =
      apps::GetFileSystemURL(profile_, intent_->files.value()[index]->url)
          .path();

  auto size = GetImagePreviewSize(index, intent_->files.value().size());
  auto image = std::make_unique<HoldingSpaceImage>(
      size, file_path,
      base::BindRepeating(&SharesheetHeaderView::LoadImage,
                          weak_ptr_factory_.GetWeakPtr()),
      HoldingSpaceImage::CreateDefaultPlaceholderImageSkiaResolver(
          /*use_light_mode_as_default=*/true));
  DCHECK_GT(image_preview_->GetImageViewCount(), index);
  image_preview_->GetImageViewAt(index)->SetImage(image->GetImageSkia(size));

  ScopedLightModeAsDefault scoped_light_mode_as_default;
  const auto icon_color = GetIconColorForPath(
      file_path, AshColorProvider::Get()->IsDarkModeEnabled());
  image_preview_->SetBackgroundColorForIndex(index, icon_color);
  image_subscription_.push_back(image->AddImageSkiaChangedCallback(
      base::BindRepeating(&SharesheetHeaderView::OnImageLoaded,
                          weak_ptr_factory_.GetWeakPtr(), size, index)));
  images_.push_back(std::move(image));
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

void SharesheetHeaderView::OnImageLoaded(const gfx::Size& size, size_t index) {
  DCHECK_GT(image_preview_->GetImageViewCount(), index);
  image_preview_->GetImageViewAt(index)->SetImage(
      images_[index]->GetImageSkia(size));
}

BEGIN_METADATA(SharesheetHeaderView, views::View)
END_METADATA

}  // namespace sharesheet
}  // namespace ash
