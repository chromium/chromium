// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"

SelectedKeywordView::SelectedKeywordView(LocationBarView* location_bar,
                                         const gfx::FontList& font_list,
                                         Profile* profile)
    : IconLabelBubbleView(font_list),
      location_bar_(location_bar),
      profile_(profile) {
  full_label_.SetFontList(font_list);
  full_label_.SetVisible(false);
  partial_label_.SetFontList(font_list);
  partial_label_.SetVisible(false);
  label()->SetElideBehavior(gfx::FADE_TAIL);
}

SelectedKeywordView::~SelectedKeywordView() {}

void SelectedKeywordView::ResetImage() {
  SetImage(gfx::CreateVectorIcon(vector_icons::kSearchIcon,
                                 GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
                                 GetTextColor()));
}

void SelectedKeywordView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetLabelForCurrentWidth();
}

SkColor SelectedKeywordView::GetTextColor() const {
  return location_bar_->GetColor(OmniboxPart::LOCATION_BAR_SELECTED_KEYWORD);
}

SkColor SelectedKeywordView::GetInkDropBaseColor() const {
  return location_bar_->GetLocationIconInkDropColor();
}

gfx::Size SelectedKeywordView::CalculatePreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(full_label_.GetPreferredSize().width());
}

gfx::Size SelectedKeywordView::GetMinimumSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(0);
}

void SelectedKeywordView::SetKeyword(const base::string16& keyword) {
  keyword_ = keyword;
  if (keyword.empty())
    return;
  DCHECK(profile_);
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!model)
    return;

  bool is_extension_keyword;
  const base::string16 short_name =
      model->GetKeywordShortName(keyword, &is_extension_keyword);
  const base::string16 full_name =
      is_extension_keyword
          ? short_name
          : l10n_util::GetStringFUTF16(IDS_OMNIBOX_KEYWORD_TEXT_MD, short_name);
  full_label_.SetText(full_name);
  partial_label_.SetText(short_name);

  // Update the label now so ShouldShowLabel() works correctly when the parent
  // class is calculating the preferred size. It will be updated again in
  // Layout(), taking into account how much space has actually been allotted.
  SetLabelForCurrentWidth();
  NotifyAccessibilityEvent(ax::mojom::Event::kLiveRegionChanged, true);
}

int SelectedKeywordView::GetExtraInternalSpacing() const {
  // Align the label text with the suggestion text.
  return 11;
}

const char* SelectedKeywordView::GetClassName() const {
  return "SelectedKeywordView";
}

void SelectedKeywordView::SetLabelForCurrentWidth() {
  // Keep showing the full label as long as there's more than enough width for
  // the partial label. Otherwise there will be empty space displayed next to
  // the partial label.
  bool use_full_label =
      width() >
      GetSizeForLabelWidth(partial_label_.GetPreferredSize().width()).width();
  SetLabel(use_full_label ? full_label_.GetText() : partial_label_.GetText());
}
