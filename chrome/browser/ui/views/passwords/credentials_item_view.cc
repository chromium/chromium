// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credentials_item_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

class CircularImageView : public views::ImageView {
  METADATA_HEADER(CircularImageView, views::ImageView)

 public:
  CircularImageView() = default;
  CircularImageView(const CircularImageView&) = delete;
  CircularImageView& operator=(const CircularImageView&) = delete;

 private:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;
};

void CircularImageView::OnPaint(gfx::Canvas* canvas) {
  // Display the avatar picture as a circle.
  gfx::Rect bounds(GetImageBounds());
  SkPath circular_mask;
  circular_mask.addCircle(
      SkIntToScalar(bounds.x() + bounds.right()) / 2,
      SkIntToScalar(bounds.y() + bounds.bottom()) / 2,
      SkIntToScalar(std::min(bounds.height(), bounds.width())) / 2);
  canvas->ClipPath(circular_mask, true);
  ImageView::OnPaint(canvas);
}

BEGIN_METADATA(CircularImageView)
END_METADATA

}  // namespace

CredentialsItemView::CredentialsItemView(
    PressedCallback callback,
    const std::u16string& upper_text,
    const std::u16string& lower_text,
    const password_manager::PasswordForm* form,
    network::mojom::URLLoaderFactory* loader_factory,
    const url::Origin& initiator,
    int upper_text_style,
    int lower_text_style)
    : Button(std::move(callback)) {
  SetNotifyEnterExitOnChild(true);
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  // Create an image-view for the avatar. Make sure it ignores events so that
  // the parent can receive the events instead.
  auto image_view = std::make_unique<CircularImageView>();
  image_view_ = image_view.get();
  image_view_->SetCanProcessEventsWithinSubtree(false);
  gfx::Image image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE);
  DCHECK(image.Width() >= kAvatarImageSize &&
         image.Height() >= kAvatarImageSize);
  UpdateAvatar(image.AsImageSkia());
  if (form->icon_url.is_valid()) {
    // Fetch the actual avatar.
    AccountAvatarFetcher* fetcher = new AccountAvatarFetcher(
        form->icon_url, weak_ptr_factory_.GetWeakPtr());
    fetcher->Start(loader_factory, initiator);
  }
  AddChildView(std::move(image_view));

  // TODO(tapted): Check these (and the STYLE_ values below) against the spec on
  // http://crbug.com/651681.
  const int kLabelContext = CONTEXT_DIALOG_BODY_TEXT_SMALL;

  views::View* text_container = nullptr;
  if (!upper_text.empty() || !lower_text.empty()) {
    text_container = AddChildView(std::make_unique<views::View>());
    views::BoxLayout* text_layout =
        text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    text_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    layout->SetFlexForView(text_container, 1);
  }

  if (!upper_text.empty()) {
    auto upper_label = std::make_unique<views::Label>(upper_text, kLabelContext,
                                                      upper_text_style);
    upper_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    upper_label_ = text_container->AddChildView(std::move(upper_label));
  }

  if (!lower_text.empty()) {
    auto lower_label = std::make_unique<views::Label>(lower_text, kLabelContext,
                                                      lower_text_style);
    lower_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    lower_label->SetMultiLine(true);
    lower_label_ = text_container->AddChildView(std::move(lower_label));
  }

  // Add info icon with a tooltip providing the source of the PSL, affiliated
  // and grouped credentials.
  if (form->match_type.has_value() &&
      password_manager_util::GetMatchType(*form) !=
          password_manager_util::GetLoginMatchType::kExact) {
    auto facet =
        affiliations::FacetURI::FromPotentiallyInvalidSpec(form->signon_realm);
    if (facet.IsValidAndroidFacetURI()) {
      std::u16string app_name = base::UTF8ToUTF16(form->app_display_name);
      if (app_name.empty()) {
        app_name = l10n_util::GetStringFUTF16(
            IDS_SETTINGS_PASSWORDS_ANDROID_APP,
            base::UTF8ToUTF16(facet.android_package_name()));
      }
      info_icon_ = AddChildView(std::make_unique<views::TooltipIcon>(app_name));
    } else {
      info_icon_ = AddChildView(std::make_unique<views::TooltipIcon>(
          base::UTF8ToUTF16(form->url.DeprecatedGetOriginAsURL().spec())));
    }
  }

  if (!upper_text.empty() && !lower_text.empty()) {
    GetViewAccessibility().SetName(upper_text + u"\n" + lower_text);
  } else {
    GetViewAccessibility().SetName(upper_text + lower_text);
  }

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(true);
  // With Focus Ring on Focus there is a line around the button.
  // We want to remove this line so setting the thickness as 0.
  views::FocusRing::Get(this)->SetHaloThickness(0.0f);
}

CredentialsItemView::~CredentialsItemView() = default;

void CredentialsItemView::UpdateAvatar(const gfx::ImageSkia& image) {
  image_view_->SetImage(
      ui::ImageModel::FromImageSkia(ScaleImageForAccountAvatar(image)));
}

int CredentialsItemView::GetPreferredHeight() const {
  return GetPreferredSize().height();
}

void CredentialsItemView::OnPaintBackground(gfx::Canvas* canvas) {
  if (GetState() == STATE_PRESSED || GetState() == STATE_HOVERED ||
      HasFocus()) {
    canvas->DrawColor(
        GetColorProvider()->GetColor(ui::kColorMenuItemBackgroundSelected));
  }
}

BEGIN_METADATA(CredentialsItemView)
END_METADATA
