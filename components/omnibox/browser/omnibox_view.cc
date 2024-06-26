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

// Return true if either non-prefix autocompletion is enabled.
bool RichAutocompletionEitherNonPrefixEnabled() {
  return OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixAll.Get() ||
         OmniboxFieldTrial::
             kRichAutocompletionAutocompleteNonPrefixShortcutProvider.Get();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Return true if the given match uses a vector icon with a background.
bool HasVectorIconBackground(const AutocompleteMatch& match) {
  return match.type == AutocompleteMatchType::HISTORY_CLUSTER ||
         match.type == AutocompleteMatchType::PEDAL;
}
#endif

}  // namespace

OmniboxView::State::State() = default;
OmniboxView::State::State(const State& state) = default;

// static
std::u16string OmniboxView::StripJavascriptSchemas(const std::u16string& text) {
  const std::u16string kJsPrefix(
      base::StrCat({url::kJavaScriptScheme16, u":"}));

  bool found_JavaScript = false;
  size_t i = 0;
  // Find the index of the first character that isn't whitespace, a control
  // character, or a part of a JavaScript: scheme.
  while (i < text.size()) {
    if (base::IsUnicodeWhitespace(text[i]) || (text[i] < 0x20)) {
      ++i;
    } else {
      if (!base::EqualsCaseInsensitiveASCII(text.substr(i, kJsPrefix.length()),
                                            kJsPrefix))
        break;

      // We've found a JavaScript scheme. Continue searching to ensure that
      // strings like "javascript:javascript:alert()" are fully stripped.
      found_JavaScript = true;
      i += kJsPrefix.length();
    }
  }

  // If we found any "JavaScript:" schemes in the text, return the text starting
  // at the first non-whitespace/control character after the last instance of
  // the scheme.
  if (found_JavaScript)
    return text.substr(i);

  return text;
}

// static
std::u16string OmniboxView::SanitizeTextForPaste(const std::u16string& text) {
  if (text.empty())
    return std::u16string();  // Nothing to do.

  size_t end = text.find_first_not_of(base::kWhitespaceUTF16);
  if (end == std::u16string::npos)
    return u" ";  // Convert all-whitespace to single space.
  // Because |end| points at the first non-whitespace character, the loop
  // below will skip leading whitespace.

  // Reserve space for the sanitized output.
  std::u16string output;
  output.reserve(text.size());  // Guaranteed to be large enough.

  // Copy all non-whitespace sequences.
  // Do not copy trailing whitespace.
  // Copy all other whitespace sequences that do not contain CR/LF.
  // Convert all other whitespace sequences that do contain CR/LF to either ' '
  // or nothing, depending on whether there are any other sequences that do not
  // contain CR/LF.
  bool output_needs_lf_conversion = false;
  bool seen_non_lf_whitespace = false;
  const auto copy_range = [&text, &output](size_t begin, size_t end) {
    output +=
        text.substr(begin, (end == std::u16string::npos) ? end : (end - begin));
  };
  constexpr char16_t kNewline[] = {'\n', 0};
  constexpr char16_t kSpace[] = {' ', 0};
  while (true) {
    // Copy this non-whitespace sequence.
    size_t begin = end;
    end = text.find_first_of(base::kWhitespaceUTF16, begin + 1);
    copy_range(begin, end);

    // Now there is either a whitespace sequence, or the end of the string.
    if (end != std::u16string::npos) {
      // There is a whitespace sequence; see if it contains CR/LF.
      begin = end;
      end = text.find_first_not_of(base::kWhitespaceNoCrLfUTF16, begin);
      if ((end != std::u16string::npos) && (text[end] != '\n') &&
          (text[end] != '\r')) {
        // Found a non-trailing whitespace sequence without CR/LF.  Copy it.
        seen_non_lf_whitespace = true;
        copy_range(begin, end);
        continue;
      }
    }

    // |end| either points at the end of the string or a CR/LF.
    if (end != std::u16string::npos)
      end = text.find_first_not_of(base::kWhitespaceUTF16, end + 1);
    if (end == std::u16string::npos)
      break;  // Ignore any trailing whitespace.

    // The preceding whitespace sequence contained CR/LF.  Convert to a single
    // LF that we'll fix up below the loop.
    output_needs_lf_conversion = true;
    output += '\n';
  }

  // Convert LFs to ' ' or '' depending on whether there were non-LF whitespace
  // sequences.
  if (output_needs_lf_conversion) {
    base::ReplaceChars(output, kNewline,
                       seen_non_lf_whitespace ? kSpace : std::u16string(),
                       &output);
  }

  return StripJavascriptSchemas(output);
}

OmniboxView::~OmniboxView() = default;

bool OmniboxView::IsEditingOrEmpty() const {
  return model()->user_input_in_progress() || GetOmniboxTextLength() == 0;
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
  NOTREACHED_IN_MIGRATION();
  return ui::ImageModel();
#else

  if (model()->ShouldShowCurrentPageIcon()) {
    return ui::ImageModel::FromVectorIcon(
        controller_->client()->GetVectorIcon(), color_current_page_icon,
        dip_size);
  }

  gfx::Image favicon;
  AutocompleteMatch match = model()->CurrentMatch(nullptr);
  if (AutocompleteMatch::IsSearchType(match.type)) {
    // For search queries, display default search engine's favicon. If the
    // default search engine is google return the icon instead of favicon for
    // search queries with the chrome refresh feature.
      if (search::DefaultSearchProviderIsGoogle(
              controller_->client()->GetTemplateURLService())) {
        // For non chrome builds this would return an empty image model. In
        // those cases revert to using the favicon.
        ui::ImageModel icon = model()->GetSuperGIcon(dip_size, dark_mode);
        if (!icon.IsEmpty()) {
          return icon;
        }
      }

    favicon = controller_->client()->GetFaviconForDefaultSearchProvider(
        std::move(on_icon_fetched));

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
  if (RichAutocompletionEitherNonPrefixEnabled())
    state->all_sel_length = GetAllSelectionsLength();
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
  if (RichAutocompletionEitherNonPrefixEnabled()) {
    state_changes.just_deleted_text =
        state_changes.just_deleted_text &&
        after.sel_start <=
            std::max(before.sel_start, before.sel_end) - before.all_sel_length;
  }

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
