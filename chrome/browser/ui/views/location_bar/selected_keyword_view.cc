// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"

#include "base/check.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"

// static
SelectedKeywordView::KeywordLabelNames
SelectedKeywordView::GetKeywordLabelNames(const std::u16string& keyword,
                                          const TemplateURLService* service) {
  KeywordLabelNames names;
  if (service) {
    bool is_extension_keyword = false;
    bool is_gemini_keyword = false;
    names.short_name = service->GetKeywordShortName(
        keyword, &is_extension_keyword, &is_gemini_keyword);
    if (is_gemini_keyword) {
      names.full_name = l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_SELECTED_KEYWORD_CHAT_TEXT, names.short_name);
    } else if (is_extension_keyword) {
      names.full_name = names.short_name;
    } else {
      names.full_name = l10n_util::GetStringFUTF16(IDS_OMNIBOX_KEYWORD_TEXT_MD,
                                                   names.short_name);
    }
  }
  return names;
}

SelectedKeywordView::SelectedKeywordView(
    IconLabelBubbleView::Delegate* delegate,
    Profile* profile,
    const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list, delegate), profile_(profile) {
  full_label_.SetFontList(font_list);
  full_label_.SetVisible(false);
  partial_label_.SetFontList(font_list);
  partial_label_.SetVisible(false);
  label()->SetElideBehavior(gfx::FADE_TAIL);

  // TODO(crbug.com/40890218): `IconLabelBubbleView::GetAccessibleNodeData`
  // would set the name to explicitly empty when the name was missing.
  // That function no longer exists. As a result we need to handle that here.
  // Regarding this view's namelessness: Until this view has a keyword and
  // labels with text, there will be no accessible name. But this view claims to
  // be focusable, so paint checks will fail due to a lack of name. It might
  // make more sense to only set `FocusBehavior` when this view will be shown.
  // For now, Eliminate the paint check failure.
  if (GetViewAccessibility().GetCachedName().empty()) {
    GetViewAccessibility().SetName(
        std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }
}

SelectedKeywordView::~SelectedKeywordView() {}

void SelectedKeywordView::SetCustomImage(const gfx::Image& image) {
  using_custom_image_ = !image.IsEmpty();
  if (using_custom_image_) {
    IconLabelBubbleView::SetImageModel(ui::ImageModel::FromImage(image));
    return;
  }

  // Use the search icon for most keywords. Use special icons for '@gemini' and
  // @history'.
  const TemplateURL* template_url =
      TemplateURLServiceFactory::GetForProfile(profile_)
          ->GetTemplateURLForKeyword(keyword_);

  auto* vector_icon = &vector_icons::kSearchIcon;
  if (template_url &&
      template_url->starter_pack_id() == TemplateURLStarterPackData::kGemini) {
    vector_icon = &omnibox::kSparkIcon;
  } else if (history_embeddings::IsHistoryEmbeddingsEnabledForProfile(
                 profile_) &&
             history_embeddings::kOmniboxScoped.Get() && template_url &&
             template_url->starter_pack_id() ==
                 TemplateURLStarterPackData::kHistory) {
    vector_icon = &omnibox::kSearchSparkIcon;
  }

  IconLabelBubbleView::SetImageModel(ui::ImageModel::FromVectorIcon(
      *vector_icon, GetForegroundColor(),
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE)));
}

void SelectedKeywordView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetLabelForCurrentWidth();
}

SkColor SelectedKeywordView::GetForegroundColor() const {
  return GetColorProvider()->GetColor(kColorOmniboxKeywordSelected);
}

gfx::Size SelectedKeywordView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(full_label_.GetPreferredSize().width());
}

gfx::Size SelectedKeywordView::GetMinimumSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(0);
}

void SelectedKeywordView::OnThemeChanged() {
  IconLabelBubbleView::OnThemeChanged();
  if (!using_custom_image_)
    SetCustomImage(gfx::Image());
}

void SelectedKeywordView::SetKeyword(const std::u16string& keyword) {
  if (keyword_ == keyword)
    return;
  keyword_ = keyword;
  OnPropertyChanged(&keyword_, views::kPropertyEffectsNone);

  const auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  // TODO(pkasting): Arguably, much of the code below would be better as
  // property change handlers in file-scope subclasses of Label etc.
  if (keyword.empty() || !template_url_service) {
    return;
  }

  KeywordLabelNames names = GetKeywordLabelNames(keyword, template_url_service);
  full_label_.SetText(names.full_name);
  partial_label_.SetText(names.short_name);

  // Update the label now so ShouldShowLabel() works correctly when the parent
  // class is calculating the preferred size. It will be updated again during
  // layout, taking into account how much space has actually been allotted.
  SetLabelForCurrentWidth();
  NotifyAccessibilityEvent(ax::mojom::Event::kLiveRegionChanged, true);
}

const std::u16string& SelectedKeywordView::GetKeyword() const {
  return keyword_;
}

int SelectedKeywordView::GetExtraInternalSpacing() const {
  // Align the label text with the suggestion text.
  return 14;
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

BEGIN_METADATA(SelectedKeywordView)
ADD_PROPERTY_METADATA(std::u16string, Keyword)
END_METADATA
