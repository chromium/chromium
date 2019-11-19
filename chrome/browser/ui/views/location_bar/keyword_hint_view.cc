// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search_engines/template_url_service.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

class ChipLabel : public views::Label {
 public:
  using views::Label::Label;

  // views::Label
  gfx::Insets GetInsets() const override {
    const int icon_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
    const int chip_corner_radius =
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::EMPHASIS_MAXIMUM, gfx::Size(icon_size, icon_size));
    return gfx::Insets(0, chip_corner_radius, 0, chip_corner_radius);
  }
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(views::Label::CalculatePreferredSize().width(),
                     GetLayoutConstant(LOCATION_BAR_ICON_SIZE));
  }
};

}  // namespace

KeywordHintView::KeywordHintView(LocationBarView* parent, Profile* profile)
    : Button(parent), profile_(profile) {
  auto chip_container = std::make_unique<views::View>();
  auto chip_label =
      std::make_unique<ChipLabel>(base::string16(), CONTEXT_OMNIBOX_DECORATION);

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification::ForSizeRule(
                      views::MinimumFlexSizeRule::kPreferredSnapToZero,
                      views::MaximumFlexSizeRule::kPreferred, true));

  const ui::ThemeProvider* theme_provider =
      &ThemeService::GetThemeProviderForProfile(profile_);
  const SkColor leading_label_text_color =
      GetOmniboxColor(theme_provider, OmniboxPart::LOCATION_BAR_TEXT_DEFAULT);
  const SkColor background_color =
      GetOmniboxColor(theme_provider, OmniboxPart::LOCATION_BAR_BACKGROUND);
  leading_label_ = CreateLabel(leading_label_text_color, background_color);

  const SkColor tab_border_color =
      GetOmniboxColor(theme_provider, OmniboxPart::LOCATION_BAR_BUBBLE_OUTLINE);
  SkColor text_color = leading_label_text_color;
  SkColor tab_bg_color =
      GetOmniboxColor(theme_provider, OmniboxPart::RESULTS_BACKGROUND);
  if (OmniboxFieldTrial::IsExperimentalKeywordModeEnabled()) {
    text_color = SK_ColorWHITE;
    tab_bg_color = tab_border_color;
  }
  chip_label->SetEnabledColor(text_color);
  chip_label->SetBackgroundColor(tab_bg_color);

  chip_container->SetBackground(CreateBackgroundFromPainter(
      views::Painter::CreateRoundRectWith1PxBorderPainter(
          tab_bg_color, tab_border_color,
          views::LayoutProvider::Get()->GetCornerRadiusMetric(
              views::EMPHASIS_HIGH))));
  chip_label_ = chip_container->AddChildView(std::move(chip_label));
  chip_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  chip_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification::ForSizeRule(
          views::MinimumFlexSizeRule::kPreferred,
          views::MaximumFlexSizeRule::kPreferred, true));
  chip_container->SizeToPreferredSize();
  chip_container_ = AddChildView(std::move(chip_container));

  trailing_label_ = CreateLabel(text_color, background_color);

  SetFocusBehavior(FocusBehavior::NEVER);

  // Use leaf alert role so that name is spoken by screen reader, but redundant
  // child label text is not also spoken.
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kGenericContainer);
  GetViewAccessibility().OverrideIsLeaf(true);
}

KeywordHintView::~KeywordHintView() {}

void KeywordHintView::SetKeyword(const base::string16& keyword) {
  // When the virtual keyboard is visible, we show a modified touch UI
  // containing only the chip and no surrounding labels.
  const bool was_touch_ui = leading_label_->GetText().empty();
  const bool is_touch_ui =
      LocationBarView::IsVirtualKeyboardVisible(GetWidget());
  if (is_touch_ui == was_touch_ui && keyword_ == keyword)
    return;

  keyword_ = keyword;
  if (keyword_.empty())
    return;
  DCHECK(profile_);
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!url_service)
    return;

  bool is_extension_keyword;
  base::string16 short_name(
      url_service->GetKeywordShortName(keyword, &is_extension_keyword));

  if (is_touch_ui) {
    int message_id = is_extension_keyword
                         ? IDS_OMNIBOX_EXTENSION_KEYWORD_HINT_TOUCH
                         : IDS_OMNIBOX_KEYWORD_HINT_TOUCH;
    base::string16 visible_text =
        l10n_util::GetStringFUTF16(message_id, short_name);
    chip_label_->SetText(visible_text);
    SetAccessibleName(visible_text);

    leading_label_->SetText(base::string16());
    trailing_label_->SetText(base::string16());
  } else {
    chip_label_->SetText(l10n_util::GetStringUTF16(IDS_APP_TAB_KEY));

    std::vector<size_t> content_param_offsets;
    int message_id = is_extension_keyword ? IDS_OMNIBOX_EXTENSION_KEYWORD_HINT
                                          : IDS_OMNIBOX_KEYWORD_HINT;
    const base::string16 keyword_hint = l10n_util::GetStringFUTF16(
        message_id, base::string16(), short_name, &content_param_offsets);
    DCHECK_EQ(2U, content_param_offsets.size());
    leading_label_->SetText(
        keyword_hint.substr(0, content_param_offsets.front()));
    trailing_label_->SetText(
        keyword_hint.substr(content_param_offsets.front()));

    const base::string16 tab_key_name =
        l10n_util::GetStringUTF16(IDS_OMNIBOX_KEYWORD_HINT_KEY_ACCNAME);
    SetAccessibleName(leading_label_->GetText() + tab_key_name +
                      trailing_label_->GetText());
  }

  // Fire an accessibility event, causing the hint to be spoken.
  NotifyAccessibilityEvent(ax::mojom::Event::kLiveRegionChanged, true);
}

gfx::Insets KeywordHintView::GetInsets() const {
  // The location bar and keyword hint view chip have rounded ends. Ensure the
  // chip label's corner with the furthest extent from its midpoint is still at
  // least kMinDistanceFromBorder DIPs away from the location bar rounded end.
  constexpr float kMinDistanceFromBorder = 6.f;
  const float radius = GetLayoutConstant(LOCATION_BAR_HEIGHT) / 2.f;
  const float hypotenuse = radius - kMinDistanceFromBorder;
  const float chip_midpoint = chip_container_->height() / 2.f;
  const float extent = std::max(chip_midpoint - chip_label_->y(),
                                chip_label_->bounds().bottom() - chip_midpoint);
  DCHECK_GE(hypotenuse, extent)
      << "LOCATION_BAR_HEIGHT must be tall enough to contain the chip.";
  const float subsumed_width =
      std::sqrt(hypotenuse * hypotenuse - extent * extent);
  const int horizontal_margin = gfx::ToCeiledInt(radius - subsumed_width);
  // This ensures the end of the KeywordHintView doesn't touch the edge of the
  // omnibox, but the padding should be symmetrical, so use it on both sides,
  // collapsing into the horizontal padding used by the previous View.
  const int left_margin =
      horizontal_margin -
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left();
  return gfx::Insets(0, std::max(0, left_margin), 0, horizontal_margin);
}

gfx::Size KeywordHintView::GetMinimumSize() const {
  // Height will be ignored by the LocationBarView.
  gfx::Size chip_size = chip_container_->GetPreferredSize();
  chip_size.Enlarge(GetInsets().width(), GetInsets().height());
  return chip_size;
}

const char* KeywordHintView::GetClassName() const {
  return "KeywordHintView";
}

void KeywordHintView::OnThemeChanged() {
  const SkColor leading_label_text_color = GetOmniboxColor(
      GetThemeProvider(), OmniboxPart::LOCATION_BAR_TEXT_DEFAULT);
  const SkColor background_color =
      GetOmniboxColor(GetThemeProvider(), OmniboxPart::LOCATION_BAR_BACKGROUND);
  leading_label_->SetEnabledColor(leading_label_text_color);
  leading_label_->SetBackgroundColor(background_color);

  const SkColor tab_border_color = GetOmniboxColor(
      GetThemeProvider(), OmniboxPart::LOCATION_BAR_BUBBLE_OUTLINE);
  SkColor text_color = leading_label_text_color;
  SkColor tab_bg_color =
      GetOmniboxColor(GetThemeProvider(), OmniboxPart::RESULTS_BACKGROUND);
  if (OmniboxFieldTrial::IsExperimentalKeywordModeEnabled()) {
    text_color = SK_ColorWHITE;
    tab_bg_color = tab_border_color;
  }
  chip_label_->SetEnabledColor(text_color);
  chip_label_->SetBackgroundColor(tab_bg_color);

  chip_container_->SetBackground(CreateBackgroundFromPainter(
      views::Painter::CreateRoundRectWith1PxBorderPainter(
          tab_bg_color, tab_border_color,
          views::LayoutProvider::Get()->GetCornerRadiusMetric(
              views::EMPHASIS_HIGH))));

  trailing_label_->SetEnabledColor(text_color);
  trailing_label_->SetBackgroundColor(background_color);
}

views::Label* KeywordHintView::CreateLabel(SkColor text_color,
                                           SkColor background_color) {
  auto label = std::make_unique<views::Label>(base::string16(),
                                              CONTEXT_OMNIBOX_DECORATION);
  label->SetEnabledColor(text_color);
  label->SetBackgroundColor(background_color);
  return AddChildView(std::move(label));
}
