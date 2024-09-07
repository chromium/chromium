// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_header_view.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/typography.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
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
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
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
  METADATA_HEADER(SharesheetImagePreview, views::View)

 public:
  explicit SharesheetImagePreview(size_t file_count) {
    auto* color_provider = AshColorProvider::Get();
    const bool is_dark_mode_enabled =
        DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
    SetBackground(views::CreateRoundedRectBackground(
        cros_styles::ResolveColor(cros_styles::ColorName::kHighlightColor,
                                  is_dark_mode_enabled,
                                  /*use_debug_colors=*/false),
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::Emphasis::kMedium),
        1));
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
    DCHECK_GT(grid_icon_count, 1u);
    AddRowToImageContainerView();
    AddRowToImageContainerView();

    for (size_t index = 0; index < grid_icon_count; ++index) {
      // If we have |enumeration|, add it as a label at the bottom right of
      // SharesheetImagePreview.
      if (enumeration != 0 && index == kImagePreviewMaxIcons - 1) {
        auto* label =
            children()[1]->AddChildView(std::make_unique<views::Label>(
                base::StrCat({u"+", base::NumberToString16(enumeration)}),
                CONTEXT_SHARESHEET_BUBBLE_SMALL, STYLE_SHARESHEET));
        label->SetLineHeight(kImagePreviewFileEnumerationLineHeight);
        label->SetEnabledColor(cros_styles::ResolveColor(
            cros_styles::ColorName::kTextColorProminent, is_dark_mode_enabled,
            /*use_debug_colors=*/false));
        label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
        auto second_tone_icon_color_prominent =
            ColorUtil::GetSecondToneColor(color_provider->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorProminent));
        label->SetBackground(views::CreateRoundedRectBackground(
            second_tone_icon_color_prominent, kImagePreviewIconCornerRadius));
        label->SetPreferredSize(kImagePreviewQuarterSize);
        return;
      }
      AddImageViewAt(index, grid_icon_count,
                     GetImagePreviewSize(index, grid_icon_count));
    }
  }

  SharesheetImagePreview(const SharesheetImagePreview&) = delete;
  SharesheetImagePreview& operator=(const SharesheetImagePreview&) = delete;

  ~SharesheetImagePreview() override = default;

  RoundedImageView* GetImageViewAt(size_t index) {
    if (index >= image_views_.size()) {
      return nullptr;
    }
    return image_views_[index];
  }

  size_t GetImageViewCount() { return image_views_.size(); }

  void SetBackgroundColorForIndex(const int index, const SkColor& color) {
    auto alpha_color =
        SkColorSetA(color, kImagePreviewBackgroundAlphaComponent);
    image_views_[index]->SetBackground(views::CreateRoundedRectBackground(
        alpha_color, kImagePreviewIconCornerRadius));
  }

 private:
  // views::View:
  void OnThemeChanged() override {
    View::OnThemeChanged();
    SetBorder(views::CreateRoundedRectBorder(
        /*thickness=*/1,
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::Emphasis::kMedium),
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOutline)));
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
        parent_view->AddChildView(std::make_unique<RoundedImageView>(
            kImagePreviewIconCornerRadius,
            RoundedImageView::Alignment::kCenter));
    image_view->SetPreferredSize(size);
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

  std::vector<raw_ptr<RoundedImageView, VectorExperimental>> image_views_;
};

BEGIN_METADATA(SharesheetHeaderView, SharesheetImagePreview)
END_METADATA

// SharesheetHeaderView --------------------------------------------------------

SharesheetHeaderView::SharesheetHeaderView(apps::IntentPtr intent,
                                           Profile* profile)
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
  SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  GetViewAccessibility().SetName(
      std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  const bool has_files = !intent_->files.empty();
  // The image view is initialised first to ensure its left most placement.
  auto file_count = intent_->files.size();
  image_preview_ =
      AddChildView(std::make_unique<SharesheetImagePreview>(file_count));

  // A separate view is created for the share title and preview string views.
  text_view_ = AddChildView(std::make_unique<views::View>());
  text_view_->SetID(HEADER_VIEW_TEXT_PREVIEW_ID);
  text_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /* inside_border_insets */ gfx::Insets(),
      /* between_child_spacing */ 0, /* collapse_margins_spacing */ true));
  text_view_->AddChildView(
      CreateShareLabel(l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL),
                       TypographyToken::kCrosTitle1,
                       cros_tokens::kCrosSysOnSurface, gfx::ALIGN_LEFT));

  ShowTextPreview();
  if (has_files) {
    ResolveImages();
  } else {
    DCHECK_GT(image_preview_->GetImageViewCount(), 0u);
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

SharesheetHeaderView::~SharesheetHeaderView() = default;

void SharesheetHeaderView::ShowTextPreview() {
  std::vector<std::unique_ptr<views::Label>> preview_labels =
      ExtractShareText();

  if (!intent_->files.empty()) {
    std::vector<std::u16string> file_names;
    for (const auto& file : intent_->files) {
      auto file_path = apps::GetFileSystemURL(profile_, file->url).path();
      file_names.push_back(file_path.BaseName().LossyDisplayName());
    }
    std::u16string file_text;
    std::u16string filenames_tooltip_text;
    if (file_names.size() == 1) {
      file_text = file_names[0];
    } else {
      // If there is more than 1 file, show an enumeration of the number of
      // files.
      auto size = intent_->files.size();
      DCHECK_NE(size, 0u);
      file_text =
          l10n_util::GetPluralStringFUTF16(IDS_SHARESHEET_FILES_LABEL, size);
      filenames_tooltip_text = ConcatenateFileNames(file_names);
    }
    auto file_label = CreatePreviewLabel(file_text);
    if (!filenames_tooltip_text.empty()) {
      file_label->SetTooltipText(filenames_tooltip_text);
      file_label->GetViewAccessibility().SetName(
          base::StrCat({file_text, u" ", filenames_tooltip_text}));
    }
    preview_labels.push_back(std::move(file_label));
  }

  if (preview_labels.size() == 0)
    return;

  int index = 0;
  int max_lines = std::min(preview_labels.size(), kTextPreviewMaximumLines);
  for (; index < max_lines; ++index) {
    text_view_->AddChildView(std::move(preview_labels[index]));
  }

  // If we have 2 or more lines of text, shorten the vertical insets.
  if (index >= 1) {
    static_cast<views::BoxLayout*>(GetLayoutManager())
        ->set_inside_border_insets(
            gfx::Insets::VH(kHeaderViewNarrowInsideBorderInsets, kSpacing));
  }
}

std::vector<std::unique_ptr<views::Label>>
SharesheetHeaderView::ExtractShareText() {
  std::vector<std::unique_ptr<views::Label>> preview_labels;

  if (intent_->share_title.has_value() &&
      !(intent_->share_title.value().empty())) {
    std::string title_text = intent_->share_title.value();
    preview_labels.push_back(CreatePreviewLabel(base::UTF8ToUTF16(title_text)));
  }

  if (intent_->share_text.has_value()) {
    apps_util::SharedText extracted_text =
        apps_util::ExtractSharedText(intent_->share_text.value());

    if (!extracted_text.text.empty()) {
      preview_labels.push_back(
          CreatePreviewLabel(base::UTF8ToUTF16(extracted_text.text)));
    }

    if (!extracted_text.url.is_empty()) {
      // The remaining width available for the text_view is : Full bubble width
      // - 2x margins - between_child_spacing - width of image preview.
      float available_width = kDefaultBubbleWidth - 2 * kSpacing -
                              kHeaderViewBetweenChildSpacing -
                              kImagePreviewFullIconSize;
      // Format URL to be elided correctly to prevent origin spoofing.
      auto elided_url = url_formatter::ElideUrl(
          extracted_text.url,
          views::TypographyProvider::Get().GetFont(
              CONTEXT_SHARESHEET_BUBBLE_BODY, views::style::STYLE_PRIMARY),
          available_width);
      auto url_label = CreatePreviewLabel(elided_url);

      // This formats the URL the same as ElideUrl does, but without elision.
      //
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
          extracted_text.url, format_types, base::UnescapeRule::NORMAL,
          /*new_parsed=*/nullptr,
          /*prefix_end=*/nullptr, /*offset_for_adjustment=*/nullptr);
      url_label->SetTooltipText(formatted_text);
      url_label->GetViewAccessibility().SetName(formatted_text);
      preview_labels.push_back(std::move(url_label));
      text_icon_ = TextPlaceholderIcon::kLink;
    }
  }

  return preview_labels;
}

std::unique_ptr<views::Label> SharesheetHeaderView::CreatePreviewLabel(
    const std::u16string& text) {
  return CreateShareLabel(text, TypographyToken::kCrosBody2,
                          cros_tokens::kCrosSysOnSurfaceVariant,
                          gfx::ALIGN_LEFT);
}

const gfx::VectorIcon& SharesheetHeaderView::GetTextVectorIcon() {
  switch (text_icon_) {
    case (TextPlaceholderIcon::kGenericText):
      return chromeos::kTextIcon;
    case (TextPlaceholderIcon::kLink):
      return vector_icons::kLinkIcon;
  }
}

void SharesheetHeaderView::ResolveImages() {
  for (size_t i = 0; i < image_preview_->GetImageViewCount(); ++i) {
    ResolveImage(i);
  }
}

void SharesheetHeaderView::ResolveImage(size_t index) {
  auto file_path =
      apps::GetFileSystemURL(profile_, intent_->files[index]->url).path();

  auto size = GetImagePreviewSize(index, intent_->files.size());
  auto image = std::make_unique<HoldingSpaceImage>(
      size, file_path,
      base::BindRepeating(&SharesheetHeaderView::LoadImage,
                          weak_ptr_factory_.GetWeakPtr()),
      HoldingSpaceImage::CreateDefaultPlaceholderImageSkiaResolver(
          /*use_light_mode_as_default=*/true));
  DCHECK_GT(image_preview_->GetImageViewCount(), index);
  const bool is_dark_mode_enabled =
      DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
  image_preview_->GetImageViewAt(index)->SetImage(
      image->GetImageSkia(size, is_dark_mode_enabled));

  const auto icon_color =
      chromeos::GetIconColorForPath(file_path, is_dark_mode_enabled);
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
  image_preview_->GetImageViewAt(index)->SetImage(images_[index]->GetImageSkia(
      size, DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()));
  // TODO(crbug.com/40213603): Investigate why this SchedulePaint is needed.
  image_preview_->GetImageViewAt(index)->SchedulePaint();
}

BEGIN_METADATA(SharesheetHeaderView)
END_METADATA

}  // namespace sharesheet
}  // namespace ash
