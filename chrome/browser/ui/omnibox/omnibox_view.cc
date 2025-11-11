// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions shared by the various implementations
// of OmniboxView.

#include "chrome/browser/ui/omnibox/omnibox_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/buildflags/buildflags.h"
#include "url/url_constants.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "ui/gfx/paint_vector_icon.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
// GN doesn't understand conditional includes, so we need nogncheck here.
#include "extensions/common/constants.h"  // nogncheck
#endif
namespace {

#if !BUILDFLAG(IS_ANDROID)
// Return true if the given match uses a vector icon with a background.
bool HasVectorIconBackground(const AutocompleteMatch& match) {
  return match.type == AutocompleteMatchType::HISTORY_CLUSTER ||
         match.type == AutocompleteMatchType::PEDAL;
}
#endif

}  // namespace

bool OmniboxView::IsEditingOrEmpty() const {
  return controller()->edit_model()->user_input_in_progress() ||
         GetOmniboxTextLength() == 0 ||
         (OmniboxFieldTrial::IsOnFocusZeroSuggestEnabledInContext(
              controller()->edit_model()->GetPageClassification()) &&
          controller()->IsPopupOpen());
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(manukh): OmniboxView::GetIcon is very similar to
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
  if (controller()->edit_model()->ShouldShowAddContextButton()) {
    return controller()->edit_model()->GetAddContextIcon(dip_size);
  }

  if (controller()->edit_model()->ShouldShowCurrentPageIcon()) {
    return ui::ImageModel::FromVectorIcon(
        controller()->client()->GetVectorIcon(), color_current_page_icon,
        dip_size);
  }

  gfx::Image favicon;
  AutocompleteMatch match = controller()->edit_model()->CurrentMatch();
  if (!match.icon_url.is_empty()) {
    const SkBitmap* bitmap =
        controller()->edit_model()->GetIconBitmap(match.icon_url);
    if (bitmap) {
      return ui::ImageModel::FromImage(
          controller()->client()->GetSizedIcon(bitmap));
    }
  }
  if (AutocompleteMatch::IsSearchType(match.type) ||
      match.enterprise_search_aggregator_type ==
          AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE) {
    const TemplateURL* turl =
        !match.keyword.empty() ? controller()
                                     ->client()
                                     ->GetTemplateURLService()
                                     ->GetTemplateURLForKeyword(match.keyword)
                               : nullptr;
    // For search queries, display match's search engine's favicon. If the
    // search engine is google, return the icon instead of favicon for
    // search queries with the chrome refresh feature.
    if ((turl &&
         search::TemplateURLIsGoogle(turl, controller()
                                               ->client()
                                               ->GetTemplateURLService()
                                               ->search_terms_data())) ||
        (!turl && search::DefaultSearchProviderIsGoogle(
                      controller()->client()->GetTemplateURLService()))) {
      // For non-chrome builds this would return an empty image model. In
      // those cases revert to using the favicon.
      ui::ImageModel icon =
          controller()->edit_model()->GetSuperGIcon(dip_size, dark_mode);
      if (!icon.IsEmpty()) {
        return icon;
      }
    } else if (turl && turl->CreatedByEnterpriseSearchAggregatorPolicy()) {
      // If the search engine is enterprise search aggregator, return the icon
      // from the bitmap instead of favicon.
      const SkBitmap* bitmap =
          controller()->edit_model()->GetIconBitmap(turl->favicon_url());
      if (bitmap) {
        return ui::ImageModel::FromImage(
            controller()->client()->GetSizedIcon(bitmap));
      }
      // For non-chrome builds this would return an empty image model. In
      // those cases revert to using the favicon.
      gfx::Image icon =
          controller()->edit_model()->GetAgentspaceIcon(dark_mode);
      if (!icon.IsEmpty()) {
        return ui::ImageModel::FromImage(icon);
      }
    }
    favicon = turl ? controller()->client()->GetFaviconForKeywordSearchProvider(
                         turl, std::move(on_icon_fetched))
                   : controller()->client()->GetFaviconForDefaultSearchProvider(
                         std::move(on_icon_fetched));

  } else if (match.type != AutocompleteMatchType::HISTORY_CLUSTER) {
    // The starter pack suggestions are a unique case. These suggestions
    // normally use a favicon image that cannot be styled further by client
    // code. In order to apply custom styling to the icon (e.g. colors), we
    // ignore this favicon in favor of using a vector icon which has better
    // styling support.
    if (!AutocompleteMatch::IsStarterPackType(match.type)) {
      // For site suggestions, display site's favicon.
      favicon = controller()->client()->GetFaviconForPageUrl(
          match.destination_url, std::move(on_icon_fetched));
    }
  }

  if (!favicon.IsEmpty()) {
    return ui::ImageModel::FromImage(
        controller()->client()->GetSizedIcon(favicon));
  }
  // If the client returns an empty favicon, fall through to provide the
  // generic vector icon. |on_icon_fetched| may or may not be called later.
  // If it's never called, the vector icon we provide below should remain.

  // For bookmarked suggestions, display bookmark icon.
  const bookmarks::BookmarkModel* bookmark_model =
      controller()->client()->GetBookmarkModel();
  const bool is_bookmarked =
      bookmark_model && bookmark_model->IsBookmarked(match.destination_url);

  // For starter pack suggestions, use template url to generate proper vector
  // icon.
  const TemplateURL* turl =
      match.associated_keyword.empty()
          ? nullptr
          : controller()
                ->client()
                ->GetTemplateURLService()
                ->GetTemplateURLForKeyword(match.associated_keyword);
  OmniboxAction* action = nullptr;
  if (match.IsToolbelt() && omnibox_feature_configs::Toolbelt::Get()
                                .use_action_icons_in_location_bar) {
    OmniboxPopupSelection selection =
        controller()->edit_model()->GetPopupSelection();
    if (selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_ACTION &&
        selection.action_index < match.actions.size()) {
      action = match.actions[selection.action_index].get();
    }
  }
  const gfx::VectorIcon& vector_icon =
      action ? action->GetVectorIcon()
             : match.GetVectorIcon(is_bookmarked, turl);
  const auto& color = (match.type == AutocompleteMatchType::HISTORY_CLUSTER ||
                       match.type == AutocompleteMatchType::STARTER_PACK)
                          ? color_bright_vectors
                          : color_vectors;
  return ui::ImageModel::FromVectorIcon(
      vector_icon,
      HasVectorIconBackground(match) ? color_vectors_with_background : color,
      dip_size);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void OmniboxView::SetUserText(const std::u16string& text) {
  SetUserText(text, true);
}

void OmniboxView::SetUserText(const std::u16string& text, bool update_popup) {
  controller()->edit_model()->SetUserText(text);
  SetWindowTextAndCaretPos(text, text.size(), update_popup, true);
}

void OmniboxView::RevertAll() {
  // This will clear the model's `user_input_in_progress_`.
  controller()->edit_model()->Revert();

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
  // Reset focus ring for the AIM button if it was set.
  ApplyFocusRingToAimButton(false);
}

bool OmniboxView::IsImeShowingPopup() const {
  // Default to claiming that the IME is not showing a popup, since hiding the
  // omnibox dropdown is a bad user experience when we don't know for sure that
  // we have to.
  return false;
}

void OmniboxView::ShowVirtualKeyboardIfEnabled() {}

void OmniboxView::HideImeIfNeeded() {}

OmniboxView::State OmniboxView::GetState() const {
  State state;
  state.text = GetText();
  state.keyword = controller()->edit_model()->keyword();
  state.is_keyword_selected = controller()->edit_model()->is_keyword_selected();
  state.selection = GetSelectionBounds();
  return state;
}

// static
OmniboxView::StateChanges OmniboxView::GetStateChanges(const State& before,
                                                       const State& after) {
  OmniboxView::StateChanges state_changes;
  state_changes.old_text = &before.text;
  state_changes.new_text = &after.text;
  state_changes.new_selection = after.selection;
  state_changes.selection_differs =
      (!before.selection.is_empty() || !after.selection.is_empty()) &&
      !before.selection.EqualsIgnoringDirection(after.selection);
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
      before.text.size() > after.text.size() &&
      after.selection.start() <= before.selection.GetMin();

  return state_changes;
}

OmniboxView::OmniboxView(OmniboxController* controller)
    : controller_(controller) {
  controller_->SetView(this);
}

OmniboxView::~OmniboxView() {
  // Clear the references to this to avoid dangling pointers.
  controller_->SetView(nullptr);
}

OmniboxController* OmniboxView::controller() {
  return const_cast<OmniboxController*>(
      const_cast<const OmniboxView*>(this)->controller());
}

const OmniboxController* OmniboxView::controller() const {
  return controller_;
}

void OmniboxView::TextChanged() {
  EmphasizeURLComponents();
  controller()->edit_model()->OnChanged();
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
    kEverything,
    kAllButScheme,
    kAllButHost,
    kNothing,
  } deemphasize = kNothing;

  url::Component scheme;
  url::Component host;
  AutocompleteInput::ParseForEmphasizeComponents(display_text, classifier,
                                                 &scheme, &host);

  const std::u16string url_scheme =
      display_text.substr(scheme.begin, scheme.len);

  const bool is_extension_url =
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      base::EqualsASCII(url_scheme, extensions::kExtensionScheme);
#else
      false;
#endif

  // Extension IDs are not human-readable, so deemphasize everything to draw
  // attention to the human-readable name in the location icon text.
  // Data URLs are rarely human-readable and can be used for spoofing, so draw
  // attention to the scheme to emphasize "this is just a bunch of data".
  // For normal URLs, the host is the best proxy for "identity".
  if (is_extension_url) {
    deemphasize = kEverything;
  } else if (url_scheme == url::kDataScheme16) {
    deemphasize = kAllButScheme;
  } else if (host.is_nonempty()) {
    deemphasize = kAllButHost;
  }

  gfx::Range scheme_range = scheme.is_nonempty()
                                ? gfx::Range(scheme.begin, scheme.end())
                                : gfx::Range::InvalidRange();
  switch (deemphasize) {
    case kEverything:
      SetEmphasis(false, gfx::Range::InvalidRange());
      break;
    case kAllButScheme:
      DCHECK(scheme_range.IsValid());
      SetEmphasis(false, gfx::Range::InvalidRange());
      SetEmphasis(true, scheme_range);
      break;
    case kAllButHost:
      SetEmphasis(false, gfx::Range::InvalidRange());
      SetEmphasis(true, gfx::Range(host.begin, host.end()));
      break;
    case kNothing:
      SetEmphasis(true, gfx::Range::InvalidRange());
      break;
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!controller()->edit_model()->user_input_in_progress() &&
      scheme_range.IsValid()) {
    UpdateSchemeStyle(scheme_range);
  }
}
