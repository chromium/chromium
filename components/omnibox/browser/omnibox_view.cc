// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions shared by the various implementations
// of OmniboxView.

#include "components/omnibox/browser/omnibox_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "url/url_constants.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "ui/gfx/paint_vector_icon.h"

#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// GN doesn't understand conditional includes, so we need nogncheck here.
#include "extensions/common/constants.h"  // nogncheck
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Return true if the given match uses a vector icon with a background.
bool HasVectorIconBackground(const AutocompleteMatch& match) {
  return match.type == AutocompleteMatchType::HISTORY_CLUSTER ||
         match.type == AutocompleteMatchType::PEDAL;
}
#endif

}  // namespace

OmniboxView::~OmniboxView() = default;

bool OmniboxView::IsEditingOrEmpty() const {
  return model()->user_input_in_progress() || GetOmniboxTextLength() == 0 ||
         (OmniboxFieldTrial::IsOnFocusZeroSuggestEnabledInContext(
              model()->GetPageClassification()) &&
          model()->PopupIsOpen());
}

// TODO (manukh) OmniboxView::GetIcon is very similar to
// OmniboxPopupModel::GetMatchIcon. They contain certain inconsistencies
// concerning what flags are required to display url favicons and bookmark star
// icons. OmniboxPopupModel::GetMatchIcon also doesn't display default search
// provider icons. It's possible they have other inconsistencies as well. We may
// want to consider reusing the same code for both the popup and omnibox icons.
ui::ImageModel OmniboxView::GetIcon(int dip_size,
                                    SkColor color_current_page_icon,
                                    SkColor color_vectors,
                                    SkColor color_bright_vectors,
                                    SkColor color_vectors_with_background,
                                    IconFetchedCallback on_icon_fetched,
                                    bool dark_mode) const {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // This is used on desktop only.
  NOTREACHED();
#else

  if (model()->ShouldShowCurrentPageIcon()) {
    return ui::ImageModel::FromVectorIcon(
        controller_->client()->GetVectorIcon(), color_current_page_icon,
        dip_size);
  }

  gfx::Image favicon;
  AutocompleteMatch match = model()->CurrentMatch(nullptr);
  if (!match.icon_url.is_empty()) {
    const SkBitmap* bitmap = model()->GetIconBitmap(match.icon_url);
    if (bitmap) {
      return ui::ImageModel::FromImage(
          controller_->client()->GetSizedIcon(bitmap));
    }
  }
  if (AutocompleteMatch::IsSearchType(match.type) ||
      match.enterprise_search_aggregator_type ==
          AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE) {
    const TemplateURL* turl =
        !match.keyword.empty() ? controller_->client()
                                     ->GetTemplateURLService()
                                     ->GetTemplateURLForKeyword(match.keyword)
                               : nullptr;
    // For search queries, display match's search engine's favicon. If the
    // search engine is google, return the icon instead of favicon for
    // search queries with the chrome refresh feature.
    if (turl && search::TemplateURLIsGoogle(turl, controller_->client()
                                                      ->GetTemplateURLService()
                                                      ->search_terms_data())) {
      // For non-chrome builds this would return an empty image model. In
      // those cases revert to using the favicon.
      ui::ImageModel icon = model()->GetSuperGIcon(dip_size, dark_mode);
      if (!icon.IsEmpty()) {
        return icon;
      }
    } else if (turl && turl->CreatedByEnterpriseSearchAggregatorPolicy()) {
      // If the search engine is enterprise search aggregator, return the icon
      // from the bitmap instead of favicon.
      const SkBitmap* bitmap = model()->GetIconBitmap(turl->favicon_url());
      if (bitmap) {
        return ui::ImageModel::FromImage(
            controller_->client()->GetSizedIcon(bitmap));
      }
      // For non-chrome builds this would return an empty image model. In
      // those cases revert to using the favicon.
      gfx::Image icon = model()->GetAgentspaceIcon(dark_mode);
      if (!icon.IsEmpty()) {
        return ui::ImageModel::FromImage(icon);
      }
    }

    if (!match.keyword.empty()) {
      favicon = controller_->client()->GetFaviconForKeywordSearchProvider(
          turl, std::move(on_icon_fetched));
    } else {
      favicon = controller_->client()->GetFaviconForDefaultSearchProvider(
          std::move(on_icon_fetched));
    }
  } else if (match.type != AutocompleteMatchType::HISTORY_CLUSTER) {
    // The starter pack suggestions are a unique case. These suggestions
    // normally use a favicon image that cannot be styled further by client
    // code. In order to apply custom styling to the icon (e.g. colors), we
    // ignore this favicon in favor of using a vector icon which has better
    // styling support.
    if (!AutocompleteMatch::IsStarterPackType(match.type)) {
      // For site suggestions, display site's favicon.
      favicon = controller_->client()->GetFaviconForPageUrl(
          match.destination_url, std::move(on_icon_fetched));
    }
  }

  if (!favicon.IsEmpty())
    return ui::ImageModel::FromImage(
        controller_->client()->GetSizedIcon(favicon));
  // If the client returns an empty favicon, fall through to provide the
  // generic vector icon. |on_icon_fetched| may or may not be called later.
  // If it's never called, the vector icon we provide below should remain.

  // For bookmarked suggestions, display bookmark icon.
  bookmarks::BookmarkModel* bookmark_model =
      controller_->client()->GetBookmarkModel();
  const bool is_bookmarked =
      bookmark_model && bookmark_model->IsBookmarked(match.destination_url);

  // For starter pack suggestions, use template url to generate proper vector
  // icon.
  const TemplateURL* turl =
      match.associated_keyword
          ? controller_->client()
                ->GetTemplateURLService()
                ->GetTemplateURLForKeyword(match.associated_keyword->keyword)
          : nullptr;
  const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmarked, turl);
  const auto& color = (match.type == AutocompleteMatchType::HISTORY_CLUSTER ||
                       match.type == AutocompleteMatchType::STARTER_PACK)
                          ? color_bright_vectors
                          : color_vectors;
  return ui::ImageModel::FromVectorIcon(
      vector_icon,
      HasVectorIconBackground(match) ? color_vectors_with_background : color,
      dip_size);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

void OmniboxView::SetUserText(const std::u16string& text) {
  SetUserText(text, true);
}

void OmniboxView::SetUserText(const std::u16string& text, bool update_popup) {
  model()->SetUserText(text);
  SetWindowTextAndCaretPos(text, text.length(), update_popup, true);
}

void OmniboxView::RevertAll() {
  // This will clear the model's `user_input_in_progress_`.
  model()->Revert();

  // This will stop the `AutocompleteController`. This should happen after
  // `user_input_in_progress_` is cleared above; otherwise, closing the popup
  // will trigger unnecessary `AutocompleteClassifier::Classify()` calls to
  // try to update the views which are unnecessary since they'll be thrown
  // away during the model revert anyways.
  CloseOmniboxPopup();

  TextChanged();
}

void OmniboxView::CloseOmniboxPopup() {
  controller()->StopAutocomplete(/*clear_result=*/true);
}

bool OmniboxView::IsImeShowingPopup() const {
  // Default to claiming that the IME is not showing a popup, since hiding the
  // omnibox dropdown is a bad user experience when we don't know for sure that
  // we have to.
  return false;
}

void OmniboxView::ShowVirtualKeyboardIfEnabled() {}

void OmniboxView::HideImeIfNeeded() {}

bool OmniboxView::IsIndicatingQueryRefinement() const {
  // The default implementation always returns false.  Mobile ports can override
  // this method and implement as needed.
  return false;
}

void OmniboxView::GetState(State* state) {
  state->text = GetText();
  state->keyword = model()->keyword();
  state->is_keyword_selected = model()->is_keyword_selected();
  GetSelectionBounds(&state->sel_start, &state->sel_end);
}

OmniboxView::StateChanges OmniboxView::GetStateChanges(const State& before,
                                                       const State& after) {
  OmniboxView::StateChanges state_changes;
  state_changes.old_text = &before.text;
  state_changes.new_text = &after.text;
  state_changes.new_sel_start = after.sel_start;
  state_changes.new_sel_end = after.sel_end;
  const bool old_sel_empty = before.sel_start == before.sel_end;
  const bool new_sel_empty = after.sel_start == after.sel_end;
  const bool sel_same_ignoring_direction =
      std::min(before.sel_start, before.sel_end) ==
          std::min(after.sel_start, after.sel_end) &&
      std::max(before.sel_start, before.sel_end) ==
          std::max(after.sel_start, after.sel_end);
  state_changes.selection_differs =
      (!old_sel_empty || !new_sel_empty) && !sel_same_ignoring_direction;
  state_changes.text_differs = before.text != after.text;
  state_changes.keyword_differs =
      (after.is_keyword_selected != before.is_keyword_selected) ||
      (after.is_keyword_selected && before.is_keyword_selected &&
       after.keyword != before.keyword);

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  state_changes.just_deleted_text =
      before.text.length() > after.text.length() &&
      after.sel_start <= std::min(before.sel_start, before.sel_end);

  return state_changes;
}

OmniboxView::OmniboxView(std::unique_ptr<OmniboxClient> client)
    : controller_(std::make_unique<OmniboxController>(
          /*view=*/this,
          std::move(client))) {}

OmniboxEditModel* OmniboxView::model() {
  return const_cast<OmniboxEditModel*>(
      const_cast<const OmniboxView*>(this)->model());
}

const OmniboxEditModel* OmniboxView::model() const {
  return controller_->edit_model();
}

OmniboxController* OmniboxView::controller() {
  return const_cast<OmniboxController*>(
      const_cast<const OmniboxView*>(this)->controller());
}

const OmniboxController* OmniboxView::controller() const {
  return controller_.get();
}

void OmniboxView::TextChanged() {
  EmphasizeURLComponents();
  model()->OnChanged();
}

void OmniboxView::UpdateTextStyle(
    const std::u16string& display_text,
    const bool text_is_url,
    const AutocompleteSchemeClassifier& classifier) {
  if (!text_is_url) {
    SetEmphasis(true, gfx::Range::InvalidRange());
    return;
  }

  enum DemphasizeComponents {
    EVERYTHING,
    ALL_BUT_SCHEME,
    ALL_BUT_HOST,
    NOTHING,
  } deemphasize = NOTHING;

  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(display_text, classifier,
                                                 &scheme, &host);

  const std::u16string url_scheme =
      display_text.substr(scheme.begin, scheme.len);

  const bool is_extension_url =
#if BUILDFLAG(ENABLE_EXTENSIONS)
      base::EqualsASCII(url_scheme, extensions::kExtensionScheme);
#else
      false;
#endif

  // Extension IDs are not human-readable, so deemphasize everything to draw
  // attention to the human-readable name in the location icon text.
  // Data URLs are rarely human-readable and can be used for spoofing, so draw
  // attention to the scheme to emphasize "this is just a bunch of data".
  // For normal URLs, the host is the best proxy for "identity".
  if (is_extension_url)
    deemphasize = EVERYTHING;
  else if (url_scheme == url::kDataScheme16)
    deemphasize = ALL_BUT_SCHEME;
  else if (host.is_nonempty())
    deemphasize = ALL_BUT_HOST;

  gfx::Range scheme_range = scheme.is_nonempty()
                                ? gfx::Range(scheme.begin, scheme.end())
                                : gfx::Range::InvalidRange();
  switch (deemphasize) {
    case EVERYTHING:
      SetEmphasis(false, gfx::Range::InvalidRange());
      break;
    case NOTHING:
      SetEmphasis(true, gfx::Range::InvalidRange());
      break;
    case ALL_BUT_SCHEME:
      DCHECK(scheme_range.IsValid());
      SetEmphasis(false, gfx::Range::InvalidRange());
      SetEmphasis(true, scheme_range);
      break;
    case ALL_BUT_HOST:
      SetEmphasis(false, gfx::Range::InvalidRange());
      SetEmphasis(true, gfx::Range(host.begin, host.end()));
      break;
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model()->user_input_in_progress() && scheme_range.IsValid())
    UpdateSchemeStyle(scheme_range);
}
