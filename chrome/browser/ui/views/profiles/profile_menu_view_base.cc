// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

// Helpers --------------------------------------------------------------------

constexpr int kMenuWidth = 290;
constexpr int kMaxImageSize = ProfileMenuViewBase::kIdentityImageSize;
constexpr int kDefaultMargin = 8;
constexpr int kBadgeSize = 16;
constexpr int kCircularImageButtonSize = 28;
constexpr int kCircularImageButtonRefreshSize = 32;
constexpr int kCircularImageButtonTransparentRefreshSize = 24;
constexpr float kShortcutIconToImageRatio = 9.0f / 16.0f;
constexpr float kShortcutIconToImageRefreshRatio = 20.0f / 32.0f;
constexpr float kShortcutIconToImageTransparentRefreshRatio = 16.0f / 24.0f;
// TODO(crbug.com/1128499): Remove this constant by extracting art height from
// |avatar_header_art|.
constexpr int kHeaderArtHeight = 80;
constexpr int kIdentityImageBorder = 2;
constexpr int kIdentityImageSizeInclBorder =
    ProfileMenuViewBase::kIdentityImageSize + 2 * kIdentityImageBorder;
constexpr int kHalfOfAvatarImageViewSize = kIdentityImageSizeInclBorder / 2;

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

// Spacing between the edge of the user menu and the top/bottom or left/right of
// the menu items.
constexpr int kMenuEdgeMargin = 16;

constexpr int kSyncInfoInsidePadding = 12;
constexpr int kSyncInfoRefreshInsidePadding = 16;

// The bottom background edge should match the center of the identity image.
constexpr auto kBackgroundInsets =
    gfx::Insets::TLBR(0, 0, kHalfOfAvatarImageViewSize, 0);

gfx::ImageSkia SizeImage(const gfx::ImageSkia& image, int size) {
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size(size, size));
}

gfx::ImageSkia ColorImage(const gfx::ImageSkia& image, SkColor color) {
  return gfx::ImageSkiaOperations::CreateColorMask(image, color);
}

class CircleImageSource : public gfx::CanvasImageSource {
 public:
  CircleImageSource(int size, SkColor color)
      : gfx::CanvasImageSource(gfx::Size(size, size)), color_(color) {}

  CircleImageSource(const CircleImageSource&) = delete;
  CircleImageSource& operator=(const CircleImageSource&) = delete;

  ~CircleImageSource() override = default;

  void Draw(gfx::Canvas* canvas) override;

 private:
  SkColor color_;
};

void CircleImageSource::Draw(gfx::Canvas* canvas) {
  float radius = size().width() / 2.0f;
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color_);
  canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
}

gfx::ImageSkia CreateCircle(int size, SkColor color) {
  return gfx::CanvasImageSource::MakeImageSkia<CircleImageSource>(size, color);
}

gfx::ImageSkia CropCircle(const gfx::ImageSkia& image) {
  DCHECK_EQ(image.width(), image.height());
  // The color here is irrelevant as long as it's opaque; only alpha matters.
  return gfx::ImageSkiaOperations::CreateMaskedImage(
      image, CreateCircle(image.width(), SK_ColorWHITE));
}

gfx::ImageSkia AddCircularBackground(const gfx::ImageSkia& image,
                                     SkColor bg_color,
                                     int size) {
  if (image.isNull())
    return gfx::ImageSkia();

  return gfx::ImageSkiaOperations::CreateSuperimposedImage(
      CreateCircle(size, bg_color), image);
}

std::unique_ptr<views::BoxLayout> CreateBoxLayout(
    views::BoxLayout::Orientation orientation,
    views::BoxLayout::CrossAxisAlignment cross_axis_alignment,
    gfx::Insets insets = gfx::Insets()) {
  auto layout = std::make_unique<views::BoxLayout>(orientation, insets);
  layout->set_cross_axis_alignment(cross_axis_alignment);
  return layout;
}

const gfx::ImageSkia ImageForMenu(const gfx::VectorIcon& icon,
                                  float icon_to_image_ratio,
                                  SkColor color) {
  const int padding =
      static_cast<int>(kMaxImageSize * (1.0f - icon_to_image_ratio) / 2.0f);

  gfx::ImageSkia sized_icon =
      gfx::CreateVectorIcon(icon, kMaxImageSize - 2 * padding, color);
  return gfx::CanvasImageSource::CreatePadded(sized_icon, gfx::Insets(padding));
}

ui::ImageModel SizeImageModel(const ui::ImageModel& image_model, int size) {
  DCHECK(!image_model.IsImageGenerator());  // Not prepared to handle these.
  if (image_model.IsImage()) {
    return ui::ImageModel::FromImageSkia(
        CropCircle(SizeImage(image_model.GetImage().AsImageSkia(), size)));
  }
  const ui::VectorIconModel& model = image_model.GetVectorIcon();
  if (model.has_color()) {
    return ui::ImageModel::FromVectorIcon(*model.vector_icon(), model.color(),
                                          size);
  }
  return ui::ImageModel::FromVectorIcon(*model.vector_icon(), model.color_id(),
                                        size);
}

const gfx::ImageSkia ProfileManagementImageFromIcon(
    const gfx::VectorIcon& icon,
    const ui::ColorProvider* color_provider) {
  constexpr float kIconToImageRatio = 0.75f;
  constexpr int kIconSize = 20;
  const SkColor icon_color = color_provider->GetColor(ui::kColorIcon);
  gfx::ImageSkia image = ImageForMenu(icon, kIconToImageRatio, icon_color);
  return SizeImage(image, kIconSize);
}

// TODO(crbug.com/1146998): Adjust button size to be 16x16.
class CircularImageButton : public views::ImageButton {
 public:
  METADATA_HEADER(CircularImageButton);

  CircularImageButton(PressedCallback callback,
                      const gfx::VectorIcon& icon,
                      const std::u16string& text,
                      int button_size = kCircularImageButtonSize,
                      bool has_background_color = false,
                      bool show_border = false,
                      SkColor themed_icon_color = SK_ColorTRANSPARENT)
      : ImageButton(std::move(callback)),
        icon_(icon),
        button_size_(button_size),
        has_background_color_(has_background_color),
        show_border_(show_border),
        themed_icon_color_(themed_icon_color) {
    SetTooltipText(text);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

    InstallCircleHighlightPathGenerator(this);

    const int kBorderThickness = show_border_ ? 1 : 0;
    const SkScalar kButtonRadius = (button_size_ + 2 * kBorderThickness) / 2.0f;
    if (features::IsChromeRefresh2023() && has_background_color_) {
      SetBackground(views::CreateThemedRoundedRectBackground(
          kColorProfileMenuIconButtonBackground, kButtonRadius));
      views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnHover(true);
      views::InkDrop::Get(this)->SetLayerRegion(views::LayerRegion::kAbove);
      views::InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
          [](CircularImageButton* host) {
            const auto* color_provider = host->GetColorProvider();
            const SkColor hover_color = color_provider->GetColor(
                kColorProfileMenuIconButtonBackgroundHovered);
            const float hover_alpha = SkColorGetA(hover_color);

            auto ink_drop_highlight = std::make_unique<views::InkDropHighlight>(
                host->size(), host->height() / 2,
                gfx::PointF(host->GetLocalBounds().CenterPoint()),
                SkColorSetA(hover_color, SK_AlphaOPAQUE));
            ink_drop_highlight->set_visible_opacity(hover_alpha /
                                                    SK_AlphaOPAQUE);
            return ink_drop_highlight;
          },
          this));
    }

    // TODO(crbug.com/1422119): Remove border for Chrome Refresh 2023.
    if (show_border_) {
      SetBorder(views::CreateThemedRoundedRectBorder(
          kBorderThickness, kButtonRadius, ui::kColorMenuSeparator));
    }
  }

  // views::ImageButton:
  void OnThemeChanged() override {
    views::ImageButton::OnThemeChanged();

    const auto* color_provider = GetColorProvider();
    SkColor icon_color = color_provider->GetColor(ui::kColorIcon);
    float shortcutIconToImageRatio = kShortcutIconToImageRatio;
    if (features::IsChromeRefresh2023()) {
      icon_color = color_provider->GetColor(kColorProfileMenuIconButton);
      shortcutIconToImageRatio =
          has_background_color_ ? kShortcutIconToImageRefreshRatio
                                : kShortcutIconToImageTransparentRefreshRatio;
    } else if (themed_icon_color_ != SK_ColorTRANSPARENT) {
      icon_color = themed_icon_color_;
    }
    gfx::ImageSkia image =
        ImageForMenu(*icon_, shortcutIconToImageRatio, icon_color);
    SetImage(views::Button::STATE_NORMAL, SizeImage(image, button_size_));
    views::InkDrop::Get(this)->SetBaseColor(icon_color);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
  // In Chrome Refresh 2023, this kind of button could have different sizes in
  // different sections of the Profile Menu, which is also different from the
  // size in the previous version of the menu.
  int button_size_;
  // In Chrome Refresh 2023, some buttons on the Profile Menu have a background
  // color that is based on the profile theme color and on light or dark mode,
  // while other buttons have a transparent background. In the previous version
  // of the menu, all backgrounds are transparent.
  bool has_background_color_;
  bool show_border_;
  // In the Profile Menu previous to Chrome Refresh 2023, icons that appears on
  // top of a background with the profile theme color (e.g. edit button) have a
  // different color than the default icon color. For the default icons, this is
  // set to transparent and not used.
  // TODO(crbug.com/1422119): Remove this parameter after Chrome Refresh 2023 is
  // launched.
  SkColor themed_icon_color_;
};

BEGIN_METADATA(CircularImageButton, views::ImageButton)
END_METADATA

class FeatureButtonIconView : public views::ImageView {
 public:
  FeatureButtonIconView(const gfx::VectorIcon& icon, float icon_to_image_ratio)
      : icon_(icon), icon_to_image_ratio_(icon_to_image_ratio) {}
  ~FeatureButtonIconView() override = default;

  // views::ImageView:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    constexpr int kIconSize = 16;
    const SkColor icon_color = GetColorProvider()->GetColor(ui::kColorIcon);
    gfx::ImageSkia image =
        ImageForMenu(*icon_, icon_to_image_ratio_, icon_color);
    SetImage(SizeImage(ColorImage(image, icon_color), kIconSize));
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
  const float icon_to_image_ratio_;
};

class ProfileManagementFeatureButton : public HoverButton {
 public:
  METADATA_HEADER(ProfileManagementFeatureButton);
  ProfileManagementFeatureButton(PressedCallback callback,
                                 const gfx::VectorIcon& icon,
                                 const std::u16string& clickable_text)
      : HoverButton(std::move(callback), clickable_text), icon_(icon) {}

  // HoverButton:
  void OnThemeChanged() override {
    HoverButton::OnThemeChanged();
    SetImage(STATE_NORMAL,
             ProfileManagementImageFromIcon(*icon_, GetColorProvider()));
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
};
BEGIN_METADATA(ProfileManagementFeatureButton, HoverButton)
END_METADATA

class ProfileManagementIconView : public views::ImageView {
 public:
  explicit ProfileManagementIconView(const gfx::VectorIcon& icon)
      : icon_(icon) {}
  ~ProfileManagementIconView() override = default;

  // views::ImageView:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    SetImage(ProfileManagementImageFromIcon(*icon_, GetColorProvider()));
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
};

// AvatarImageView is used to ensure avatar adornments are kept in sync with
// current theme colors.
class AvatarImageView : public views::ImageView {
 public:
  AvatarImageView(const ui::ImageModel& avatar_image,
                  const ProfileMenuViewBase* root_view)
      : avatar_image_(avatar_image), root_view_(root_view) {
    if (avatar_image_.IsEmpty()) {
      // This can happen if the account image hasn't been fetched yet, if there
      // is no image, or in tests.
      avatar_image_ = ui::ImageModel::FromVectorIcon(
          kUserAccountAvatarIcon, ui::kColorMenuIcon,
          ProfileMenuViewBase::kIdentityImageSize);
    }
  }

  // views::ImageView:
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
    constexpr int kBadgePadding = 1;
    DCHECK(!avatar_image_.IsEmpty());
    gfx::ImageSkia sized_avatar_image =
        SizeImageModel(avatar_image_, ProfileMenuViewBase::kIdentityImageSize)
            .Rasterize(GetColorProvider());
    sized_avatar_image = AddCircularBackground(
        sized_avatar_image, GetBackgroundColor(), kIdentityImageSizeInclBorder);

    if (features::IsChromeRefresh2023()) {
      SetImage(sized_avatar_image);
    } else {
      gfx::ImageSkia sized_badge = AddCircularBackground(
          SizeImage(root_view_->GetSyncIcon(), kBadgeSize),
          GetBackgroundColor(), kBadgeSize + 2 * kBadgePadding);
      gfx::ImageSkia sized_badge_with_shadow =
          gfx::ImageSkiaOperations::CreateImageWithDropShadow(
              sized_badge, gfx::ShadowValue::MakeMdShadowValues(/*elevation=*/1,
                                                                SK_ColorBLACK));

      gfx::ImageSkia badged_image =
          gfx::ImageSkiaOperations::CreateIconWithBadge(
              sized_avatar_image, sized_badge_with_shadow);
      SetImage(badged_image);
    }
  }

 private:
  SkColor GetBackgroundColor() const {
    return GetColorProvider()->GetColor(ui::kColorBubbleBackground);
  }

  ui::ImageModel avatar_image_;
  raw_ptr<const ProfileMenuViewBase> root_view_;
};

class SyncButton : public HoverButton {
 public:
  METADATA_HEADER(SyncButton);
  SyncButton(PressedCallback callback,
             ProfileMenuViewBase* root_view,
             const std::u16string& clickable_text)
      : HoverButton(std::move(callback), clickable_text),
        root_view_(root_view) {}

  // HoverButton:
  void OnThemeChanged() override {
    HoverButton::OnThemeChanged();
    SetImage(STATE_NORMAL, SizeImage(root_view_->GetSyncIcon(), kBadgeSize));
  }

 private:
  raw_ptr<const ProfileMenuViewBase> root_view_;
};

BEGIN_METADATA(SyncButton, HoverButton)
END_METADATA

class SyncImageView : public views::ImageView {
 public:
  explicit SyncImageView(const ProfileMenuViewBase* root_view)
      : root_view_(root_view) {}

  // views::ImageView:
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
    SetImage(SizeImage(root_view_->GetSyncIcon(), kBadgeSize));
  }

 private:
  raw_ptr<const ProfileMenuViewBase> root_view_;
};

void BuildProfileTitleAndSubtitle(views::View* parent,
                                  const std::u16string& title,
                                  const std::u16string& subtitle) {
  views::View* profile_titles_container =
      parent->AddChildView(std::make_unique<views::View>());
  // Separate the titles from the avatar image by the default margin.
  profile_titles_container->SetLayoutManager(
      CreateBoxLayout(views::BoxLayout::Orientation::kVertical,
                      views::BoxLayout::CrossAxisAlignment::kCenter,
                      gfx::Insets::TLBR(kDefaultMargin, 0, 0, 0)));

  if (!title.empty()) {
    profile_titles_container->AddChildView(
        features::IsChromeRefresh2023()
            ? std::make_unique<views::Label>(title,
                                             views::style::CONTEXT_DIALOG_TITLE,
                                             views::style::STYLE_HEADLINE_4)
            : std::make_unique<views::Label>(
                  title, views::style::CONTEXT_DIALOG_TITLE));
  }

  if (!subtitle.empty()) {
    profile_titles_container->AddChildView(std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_LABEL,
        features::IsChromeRefresh2023() ? views::style::STYLE_BODY_3
                                        : views::style::STYLE_SECONDARY));
  }
}

}  // namespace

// ProfileMenuViewBase ---------------------------------------------------------

ProfileMenuViewBase::EditButtonParams::EditButtonParams(
    const gfx::VectorIcon* edit_icon,
    const std::u16string& edit_tooltip_text,
    base::RepeatingClosure edit_action)
    : edit_icon(edit_icon),
      edit_tooltip_text(edit_tooltip_text),
      edit_action(edit_action) {}

ProfileMenuViewBase::EditButtonParams::~EditButtonParams() = default;

ProfileMenuViewBase::EditButtonParams::EditButtonParams(
    const EditButtonParams&) = default;

ProfileMenuViewBase::ProfileMenuViewBase(views::Button* anchor_button,
                                         Browser* browser)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      anchor_button_(anchor_button),
      close_bubble_helper_(this, browser) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  // TODO(tluk): Remove when fixing https://crbug.com/822075
  // The sign in webview will be clipped on the bottom corners without these
  // margins, see related bug <http://crbug.com/593203>.
  SetPaintClientToLayer(true);
  set_margins(gfx::Insets(0));
  DCHECK(anchor_button);
  views::InkDrop::Get(anchor_button)
      ->AnimateToState(views::InkDropState::ACTIVATED, nullptr);

  SetEnableArrowKeyTraversal(true);

  // TODO(crbug.com/1341017): Using `SetAccessibleWindowRole(kMenu)` here will
  // result in screenreader to announce the menu having only one item. This is
  // probably because this API sets the a11y role for the widget, but not root
  // view in it. This is confusing and prone to misuse. We should unify the two
  // sets of API for BubbleDialogDelegateView.
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenu);

  RegisterWindowClosingCallback(base::BindOnce(
      &ProfileMenuViewBase::OnWindowClosing, base::Unretained(this)));
}

ProfileMenuViewBase::~ProfileMenuViewBase() = default;

gfx::ImageSkia ProfileMenuViewBase::GetSyncIcon() const {
  return gfx::ImageSkia();
}

// This function deals with the somewhat complicated layout to build the part of
// the profile identity info that has a colored background.
void ProfileMenuViewBase::BuildProfileBackgroundContainer(
    std::unique_ptr<views::View> heading_label,
    SkColor background_color,
    std::unique_ptr<views::View> avatar_image_view,
    std::unique_ptr<views::View> edit_button,
    const ui::ThemedVectorIcon& avatar_header_art) {
  profile_background_container_ = identity_info_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());

  auto background_container_insets = gfx::Insets::VH(0, kMenuEdgeMargin);
  if (edit_button) {
    // Compensate for the edit button on the right with an extra margin on the
    // left so that the rest is centered.
    background_container_insets.set_left(background_container_insets.left() +
                                         kCircularImageButtonSize);
  }
  profile_background_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  profile_background_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kEnd);
  profile_background_container_->SetInteriorMargin(background_container_insets);

  // Show a colored background iff there is no art.
  if (features::IsChromeRefresh2023() && avatar_header_art.empty()) {
    identity_info_color_callback_ = base::BindRepeating(
        &ProfileMenuViewBase::BuildIdentityInfoColorCallback,
        base::Unretained(this));
  } else if (avatar_header_art.empty()) {
    // TODO(crbug.com/1147038): Remove the zero-radius rounded background.
    profile_background_container_->SetBackground(
        views::CreateBackgroundFromPainter(
            views::Painter::CreateSolidRoundRectPainter(
                background_color, /*radius=*/0, kBackgroundInsets)));
  } else {
    DCHECK_EQ(SK_ColorTRANSPARENT, background_color);
    profile_background_container_->SetBackground(
        views::CreateThemedVectorIconBackground(avatar_header_art));
  }

  // |avatar_margin| is derived from |avatar_header_art| asset height, it
  // increases margin for the avatar icon to make |avatar_header_art| visible
  // above the center of the avatar icon.
  const int avatar_margin = avatar_header_art.empty()
                                ? kMenuEdgeMargin
                                : kHeaderArtHeight - kHalfOfAvatarImageViewSize;

  // The |heading_and_image_container| is on the left and it stretches almost
  // the full width. It contains the profile heading and the avatar image.
  views::FlexLayoutView* heading_and_image_container =
      profile_background_container_->AddChildView(
          std::make_unique<views::FlexLayoutView>());
  heading_and_image_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(1));
  heading_and_image_container->SetOrientation(
      views::LayoutOrientation::kVertical);
  heading_and_image_container->SetMainAxisAlignment(
      views::LayoutAlignment::kCenter);
  heading_and_image_container->SetCrossAxisAlignment(
      views::LayoutAlignment::kCenter);
  heading_and_image_container->SetInteriorMargin(
      gfx::Insets::TLBR(avatar_margin, 0, 0, 0));
  if (heading_label) {
    DCHECK(avatar_header_art.empty());
    heading_label->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::VH(kDefaultMargin, 0)));
    heading_label_ = (views::Label*)heading_and_image_container->AddChildView(
        std::move(heading_label));
  }

  heading_and_image_container->AddChildView(std::move(avatar_image_view));

  // The |edit_button| is on the right and has fixed width.
  if (edit_button) {
    edit_button->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kPreferred)
            .WithOrder(2));
    views::View* edit_button_container =
        profile_background_container_->AddChildView(
            std::make_unique<views::View>());
    edit_button_container->SetLayoutManager(CreateBoxLayout(
        views::BoxLayout::Orientation::kVertical,
        views::BoxLayout::CrossAxisAlignment::kCenter,
        gfx::Insets::TLBR(0, 0, kHalfOfAvatarImageViewSize + kDefaultMargin,
                          0)));
    edit_button_container->AddChildView(std::move(edit_button));
  }
}

void ProfileMenuViewBase::SetProfileIdentityInfo(
    const std::u16string& profile_name,
    SkColor profile_background_color,
    absl::optional<EditButtonParams> edit_button_params,
    const ui::ImageModel& image_model,
    const std::u16string& title,
    const std::u16string& subtitle,
    const ui::ThemedVectorIcon& avatar_header_art) {
  constexpr int kBottomMargin = kDefaultMargin;

  identity_info_container_->RemoveAllChildViews();
  // The colored background fully bleeds to the edges of the menu and to achieve
  // that margin is set to 0. Further margins will be added by children views.
  identity_info_container_->SetLayoutManager(
      CreateBoxLayout(views::BoxLayout::Orientation::kVertical,
                      views::BoxLayout::CrossAxisAlignment::kStretch,
                      gfx::Insets::TLBR(0, 0, kBottomMargin, 0)));

  auto avatar_image_view = std::make_unique<AvatarImageView>(image_model, this);

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // crbug.com/1161166: Orca does not read the accessible window title of the
  // bubble, so we duplicate it in the top-level menu item. To be revisited
  // after considering other options, including fixes on the AT side.
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());
#endif

  std::unique_ptr<views::Label> heading_label;
  if (!profile_name.empty()) {
    if (features::IsChromeRefresh2023()) {
      heading_label = std::make_unique<views::Label>(
          profile_name, views::style::CONTEXT_LABEL,
          views::style::STYLE_HEADLINE_5);
    } else {
      views::Label::CustomFont font = {
          views::Label::GetDefaultFontList()
              .DeriveWithSizeDelta(2)
              .DeriveWithWeight(gfx::Font::Weight::BOLD)};
      heading_label = std::make_unique<views::Label>(profile_name, font);
    }
    heading_label->SetElideBehavior(gfx::ELIDE_TAIL);
    heading_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    heading_label->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    if (avatar_header_art.empty()) {
      heading_label->SetAutoColorReadabilityEnabled(false);
      heading_label->SetEnabledColor(
          GetProfileForegroundTextColor(profile_background_color));
    }
  }

  std::unique_ptr<views::View> edit_button;
  if (edit_button_params.has_value()) {
    if (features::IsChromeRefresh2023()) {
      edit_button = std::make_unique<CircularImageButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this),
                              std::move(edit_button_params->edit_action)),
          *edit_button_params->edit_icon, edit_button_params->edit_tooltip_text,
          kCircularImageButtonTransparentRefreshSize);
    } else {
      edit_button = std::make_unique<CircularImageButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this),
                              std::move(edit_button_params->edit_action)),
          *edit_button_params->edit_icon, edit_button_params->edit_tooltip_text,
          kCircularImageButtonSize, /*has_background_color=*/false,
          /*show_border=*/false,
          avatar_header_art.empty()
              ? GetProfileForegroundIconColor(profile_background_color)
              : SK_ColorTRANSPARENT);
    }
  }

  BuildProfileBackgroundContainer(
      std::move(heading_label), profile_background_color,
      std::move(avatar_image_view), std::move(edit_button), avatar_header_art);
  BuildProfileTitleAndSubtitle(/*parent=*/identity_info_container_, title,
                               subtitle);
}

void ProfileMenuViewBase::BuildSyncInfoWithCallToAction(
    const std::u16string& description,
    const std::u16string& button_text,
    ui::ColorId background_color_id,
    const base::RepeatingClosure& action,
    bool show_sync_badge) {
  const int kDescriptionIconSpacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  sync_info_container_->RemoveAllChildViews();
  sync_info_container_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(kSyncInfoInsidePadding, 0));
  sync_info_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kVertical,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded, true,
                               views::MinimumFlexSizeRule::kScaleToZero));
  sync_info_container_->SetProperty(
      views::kMarginsKey, gfx::Insets::VH(kDefaultMargin, kMenuEdgeMargin));

  // Add icon + description at the top.
  views::View* description_container =
      sync_info_container_->AddChildView(std::make_unique<views::View>());
  description_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kVertical,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded, true,
                               views::MinimumFlexSizeRule::kScaleToZero));
  views::FlexLayout* description_layout =
      &description_container
           ->SetLayoutManager(std::make_unique<views::FlexLayout>())
           ->SetOrientation(views::LayoutOrientation::kHorizontal)
           .SetIgnoreDefaultMainAxisMargins(true)
           .SetCollapseMargins(true)
           .SetDefault(views::kMarginsKey,
                       gfx::Insets::VH(0, kDescriptionIconSpacing));

  if (show_sync_badge) {
    description_container->AddChildView(std::make_unique<SyncImageView>(this));
  } else {
    // If there is no image, the description is centered.
    description_layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  }

  views::Label* label = description_container->AddChildView(
      features::IsChromeRefresh2023()
          ? std::make_unique<views::Label>(description,
                                           views::style::CONTEXT_LABEL,
                                           views::style::STYLE_BODY_3_EMPHASIS)
          : std::make_unique<views::Label>(description));
  label->SetMultiLine(true);
  label->SetHandlesTooltips(false);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred, true));

  // Set sync info description as the name of the parent container, so
  // accessibility tools can read it together with the button text. The role
  // change is required by Windows ATs.
  sync_info_container_->GetViewAccessibility().OverrideRole(
      ax::mojom::Role::kGroup);
  sync_info_container_->GetViewAccessibility().OverrideName(description);

  // Add the prominent button at the bottom.
  auto* button =
      sync_info_container_->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this), std::move(action)),
          button_text));
  button->SetProminent(true);

  // TODO(crbug.com/1422119): Remove `background_color_id` parameter after
  // Chrome Refresh 2023 is launched.
  sync_info_background_callback_ = base::BindRepeating(
      &ProfileMenuViewBase::BuildSyncInfoCallToActionBackground,
      base::Unretained(this),
      features::IsChromeRefresh2023() ? kColorProfileMenuSyncInfoBackground
                                      : background_color_id);
}

void ProfileMenuViewBase::BuildSyncInfoWithoutCallToAction(
    const std::u16string& text,
    const base::RepeatingClosure& action) {
  sync_info_container_->RemoveAllChildViews();
  sync_info_container_->SetLayoutManager(std::make_unique<views::FillLayout>());
  sync_info_container_->AddChildView(std::make_unique<SyncButton>(
      base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                          base::Unretained(this), std::move(action)),
      this, text));

  // No background required, so ui::ColorProvider isn't needed and
  // |sync_info_background_callback_| can be set to base::DoNothing().
  sync_info_container_->SetBackground(nullptr);
  sync_info_background_callback_ = base::DoNothing();
}

void ProfileMenuViewBase::AddShortcutFeatureButton(
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    base::RepeatingClosure action) {
  const int kButtonSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  // Initialize layout if this is the first time a button is added.
  if (!shortcut_features_container_->GetLayoutManager()) {
    views::BoxLayout* layout = shortcut_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets::TLBR(kDefaultMargin / 2, 0, kMenuEdgeMargin, 0),
            kButtonSpacing));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
  }

  views::Button* button = shortcut_features_container_->AddChildView(
      std::make_unique<CircularImageButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this), std::move(action)),
          icon, text,
          features::IsChromeRefresh2023() ? kCircularImageButtonRefreshSize
                                          : kCircularImageButtonSize,
          /*has_background_color=*/true,
          /*show_border=*/!features::IsChromeRefresh2023()));
  button->SetFlipCanvasOnPaintForRTLUI(false);
}

void ProfileMenuViewBase::AddFeatureButton(const std::u16string& text,
                                           base::RepeatingClosure action,
                                           const gfx::VectorIcon& icon,
                                           float icon_to_image_ratio) {
  // Initialize layout if this is the first time a button is added.
  if (!features_container_->GetLayoutManager()) {
    features_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
  }

  if (&icon == &gfx::kNoneIcon) {
    features_container_->AddChildView(std::make_unique<HoverButton>(
        base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                            base::Unretained(this), std::move(action)),
        text));
  } else {
    auto icon_view =
        std::make_unique<FeatureButtonIconView>(icon, icon_to_image_ratio);
    features_container_->AddChildView(std::make_unique<HoverButton>(
        base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                            base::Unretained(this), std::move(action)),
        std::move(icon_view), text));
  }
}

void ProfileMenuViewBase::SetProfileManagementHeading(
    const std::u16string& heading) {
  profile_mgmt_heading_ = heading;

  // Add separator before heading.
  profile_mgmt_separator_container_->RemoveAllChildViews();
  profile_mgmt_separator_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  profile_mgmt_separator_container_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(kDefaultMargin, 0)));
  profile_mgmt_separator_container_->AddChildView(
      std::make_unique<views::Separator>());

  // Initialize heading layout.
  profile_mgmt_heading_container_->RemoveAllChildViews();
  profile_mgmt_heading_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  profile_mgmt_heading_container_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kDefaultMargin, kMenuEdgeMargin)));

  // Add heading.
  views::Label* label = profile_mgmt_heading_container_->AddChildView(
      std::make_unique<views::Label>(heading, views::style::CONTEXT_LABEL,
                                     features::IsChromeRefresh2023()
                                         ? views::style::STYLE_BODY_3_EMPHASIS
                                         : views::style::STYLE_HINT));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetHandlesTooltips(false);
}

void ProfileMenuViewBase::AddAvailableProfile(const ui::ImageModel& image_model,
                                              const std::u16string& name,
                                              bool is_guest,
                                              bool is_enabled,
                                              base::RepeatingClosure action) {
  // Initialize layout if this is the first time a button is added.
  if (!selectable_profiles_container_->GetLayoutManager()) {
    selectable_profiles_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    // Give the container an accessible name so accessibility tools can provide
    // context for the buttons inside it. The role change is required by Windows
    // ATs.
    selectable_profiles_container_->GetViewAccessibility().OverrideRole(
        ax::mojom::Role::kGroup);
    selectable_profiles_container_->GetViewAccessibility().OverrideName(
        profile_mgmt_heading_);
  }

  DCHECK(!image_model.IsEmpty());
  ui::ImageModel sized_image =
      SizeImageModel(image_model, profiles::kMenuAvatarIconSize);
  views::Button* button = selectable_profiles_container_->AddChildView(
      std::make_unique<HoverButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this), std::move(action)),
          sized_image, name));

  button->SetEnabled(is_enabled);

  if (!is_guest && !first_profile_button_)
    first_profile_button_ = button;
}

void ProfileMenuViewBase::AddProfileManagementShortcutFeatureButton(
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    base::RepeatingClosure action) {
  // Initialize layout if this is the first time a button is added.
  if (!profile_mgmt_shortcut_features_container_->GetLayoutManager()) {
    profile_mgmt_shortcut_features_container_->SetLayoutManager(
        CreateBoxLayout(views::BoxLayout::Orientation::kHorizontal,
                        views::BoxLayout::CrossAxisAlignment::kCenter,
                        gfx::Insets::TLBR(0, 0, 0, kMenuEdgeMargin)));
  }

  if (features::IsChromeRefresh2023()) {
    profile_mgmt_shortcut_features_container_->AddChildView(
        views::ImageButton::CreateIconButton(
            base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                                base::Unretained(this), std::move(action)),
            icon, text, CircularImageButton::MaterialIconStyle::kSmall));

  } else {
    profile_mgmt_shortcut_features_container_->AddChildView(
        std::make_unique<CircularImageButton>(
            base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                                base::Unretained(this), std::move(action)),
            icon, text));
  }
}

void ProfileMenuViewBase::AddProfileManagementManagedHint(
    const gfx::VectorIcon& icon,
    const std::u16string& text) {
  // Initialize layout if this is the first time a button is added.
  if (!profile_mgmt_shortcut_features_container_->GetLayoutManager()) {
    profile_mgmt_shortcut_features_container_->SetLayoutManager(
        CreateBoxLayout(views::BoxLayout::Orientation::kHorizontal,
                        views::BoxLayout::CrossAxisAlignment::kCenter,
                        gfx::Insets::TLBR(0, 0, 0, kMenuEdgeMargin)));
  }

  views::ImageView* icon_button =
      profile_mgmt_shortcut_features_container_->AddChildView(
          std::make_unique<ProfileManagementIconView>(icon));
  icon_button->SetTooltipText(text);
}

void ProfileMenuViewBase::AddProfileManagementFeatureButton(
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    base::RepeatingClosure action) {
  // Initialize layout if this is the first time a button is added.
  if (!profile_mgmt_features_container_->GetLayoutManager()) {
    profile_mgmt_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    profile_mgmt_features_container_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kDefaultMargin, 0)));
  }

  profile_mgmt_features_container_->AddChildView(
      std::make_unique<ProfileManagementFeatureButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this), std::move(action)),
          icon, text));
}

gfx::ImageSkia ProfileMenuViewBase::ColoredImageForMenu(
    const gfx::VectorIcon& icon,
    ui::ColorId color) const {
  return gfx::CreateVectorIcon(icon, kMaxImageSize,
                               GetColorProvider()->GetColor(color));
}

void ProfileMenuViewBase::RecordClick(ActionableItem item) {
  // TODO(tangltom): Separate metrics for incognito and guest menu.
  base::UmaHistogramEnumeration("Profile.Menu.ClickedActionableItem", item);
}

int ProfileMenuViewBase::GetMaxHeight() const {
  gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect screen_space =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  int available_space = screen_space.bottom() - anchor_rect.bottom();
#if BUILDFLAG(IS_WIN)
  // On Windows the bubble can also be show to the top of the anchor.
  available_space =
      std::max(available_space, anchor_rect.y() - screen_space.y());
#endif
  return std::max(kMinimumScrollableContentHeight, available_space);
}

void ProfileMenuViewBase::Reset() {
  RemoveAllChildViews();

  auto components = std::make_unique<views::View>();
  components->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  // Create and add new component containers in the correct order.
  // First, add the parts of the current profile.
  identity_info_container_ =
      components->AddChildView(std::make_unique<views::View>());
  shortcut_features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  sync_info_container_ =
      components->AddChildView(std::make_unique<views::View>());
  features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  profile_mgmt_separator_container_ =
      components->AddChildView(std::make_unique<views::View>());
  // Second, add the profile management header. This includes the heading and
  // the shortcut feature(s) next to it.
  auto profile_mgmt_header = std::make_unique<views::View>();
  views::BoxLayout* profile_mgmt_header_layout =
      profile_mgmt_header->SetLayoutManager(
          CreateBoxLayout(views::BoxLayout::Orientation::kHorizontal,
                          views::BoxLayout::CrossAxisAlignment::kCenter));
  profile_mgmt_heading_container_ =
      profile_mgmt_header->AddChildView(std::make_unique<views::View>());
  profile_mgmt_header_layout->SetFlexForView(profile_mgmt_heading_container_,
                                             1);
  profile_mgmt_shortcut_features_container_ =
      profile_mgmt_header->AddChildView(std::make_unique<views::View>());
  profile_mgmt_header_layout->SetFlexForView(
      profile_mgmt_shortcut_features_container_, 0);
  components->AddChildView(std::move(profile_mgmt_header));
  // Third, add the profile management buttons.
  selectable_profiles_container_ =
      components->AddChildView(std::make_unique<views::View>());
  profile_mgmt_features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  first_profile_button_ = nullptr;

  // Create a scroll view to hold the components.
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // TODO(https://crbug.com/871762): it's a workaround for the crash.
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());
  scroll_view->SetContents(std::move(components));

  // Create a table layout to set the menu width.
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(
          views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
          views::TableLayout::kFixedSize,
          views::TableLayout::ColumnSize::kFixed, kMenuWidth, kMenuWidth)
      .AddRows(1, 1.0f);
  AddChildView(std::move(scroll_view));
}

void ProfileMenuViewBase::FocusFirstProfileButton() {
  if (first_profile_button_)
    first_profile_button_->RequestFocus();
}

void ProfileMenuViewBase::BuildIdentityInfoColorCallback(
    const ui::ColorProvider* color_provider) {
  profile_background_container_->SetBackground(
      views::CreateBackgroundFromPainter(
          views::Painter::CreateSolidRoundRectPainter(
              color_provider->GetColor(kColorProfileMenuHeaderBackground),
              /*radius=*/0, kBackgroundInsets)));
  if (heading_label_) {
    heading_label_->SetEnabledColor(
        color_provider->GetColor(kColorProfileMenuHeaderLabel));
  }
}

void ProfileMenuViewBase::BuildSyncInfoCallToActionBackground(
    ui::ColorId background_color_id,
    const ui::ColorProvider* color_provider) {
  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  if (features::IsChromeRefresh2023()) {
    sync_info_container_->SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetColor(background_color_id), radius));
    sync_info_container_->SetBorder(views::CreatePaddedBorder(
        views::CreateRoundedRectBorder(
            0, radius,
            color_provider->GetColor(kColorProfileMenuSyncInfoBackground)),
        gfx::Insets(kSyncInfoRefreshInsidePadding)));
  } else {
    sync_info_container_->SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetColor(background_color_id), radius, 1));
    sync_info_container_->SetBorder(views::CreatePaddedBorder(
        views::CreateRoundedRectBorder(
            1, radius, color_provider->GetColor(ui::kColorMenuSeparator)),
        gfx::Insets(kSyncInfoInsidePadding)));
  }
}

void ProfileMenuViewBase::Init() {
  Reset();
  BuildMenu();
}

void ProfileMenuViewBase::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  SetBackground(views::CreateSolidBackground(
      color_provider->GetColor(ui::kColorDialogBackground)));
  if (features::IsChromeRefresh2023()) {
    identity_info_color_callback_.Run(color_provider);
  }
  sync_info_background_callback_.Run(color_provider);
}

void ProfileMenuViewBase::OnWindowClosing() {
  if (!anchor_button())
    return;

  views::InkDrop::Get(anchor_button())
      ->AnimateToState(views::InkDropState::DEACTIVATED, nullptr);
}

bool ProfileMenuViewBase::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void ProfileMenuViewBase::ButtonPressed(base::RepeatingClosure action) {
  DCHECK(action);
  signin_ui_util::RecordProfileMenuClick(browser()->profile());
  action.Run();
}

void ProfileMenuViewBase::CreateAXWidgetObserver(views::Widget* widget) {
  ax_widget_observer_ = std::make_unique<AXMenuWidgetObserver>(this, widget);
}

// Despite ProfileMenuViewBase being a dialog, we are enforcing it to behave
// like a menu from the accessibility POV because it fits better with a menu UX.
// The dialog exposes the kMenuBar role, and the top-level container is kMenu.
// This class is responsible for emitting menu accessible events when the dialog
// is activated or deactivated.
class ProfileMenuViewBase::AXMenuWidgetObserver : public views::WidgetObserver {
 public:
  AXMenuWidgetObserver(ProfileMenuViewBase* owner, views::Widget* widget)
      : owner_(owner) {
    observation_.Observe(widget);
  }
  ~AXMenuWidgetObserver() override = default;

  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (active) {
      owner_->NotifyAccessibilityEvent(ax::mojom::Event::kMenuStart, true);
      owner_->NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupStart, true);
    } else {
      owner_->NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupEnd, true);
      owner_->NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd, true);
    }
  }

 private:
  raw_ptr<ProfileMenuViewBase> owner_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};
