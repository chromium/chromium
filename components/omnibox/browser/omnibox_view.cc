// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions shared by the various implementations
// of OmniboxView.

#include "components/omnibox/browser/omnibox_view.h"

#include <algorithm>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/common/omnibox_features.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "ui/gfx/paint_vector_icon.h"
#endif

// static
base::string16 OmniboxView::StripJavascriptSchemas(const base::string16& text) {
  const base::string16 kJsPrefix(
      base::ASCIIToUTF16(url::kJavaScriptScheme) + base::ASCIIToUTF16(":"));

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
base::string16 OmniboxView::SanitizeTextForPaste(const base::string16& text) {
  if (text.empty())
    return base::string16();  // Nothing to do.

  size_t end = text.find_first_not_of(base::kWhitespaceUTF16);
  if (end == base::string16::npos)
    return base::ASCIIToUTF16(" ");  // Convert all-whitespace to single space.
  // Because |end| points at the first non-whitespace character, the loop
  // below will skip leading whitespace.

  // Reserve space for the sanitized output.
  base::string16 output;
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
        text.substr(begin, (end == base::string16::npos) ? end : (end - begin));
  };
  constexpr base::char16 kNewline[] = {'\n', 0};
  constexpr base::char16 kSpace[] = {' ', 0};
  while (true) {
    // Copy this non-whitespace sequence.
    size_t begin = end;
    end = text.find_first_of(base::kWhitespaceUTF16, begin + 1);
    copy_range(begin, end);

    // Now there is either a whitespace sequence, or the end of the string.
    if (end != base::string16::npos) {
      // There is a whitespace sequence; see if it contains CR/LF.
      begin = end;
      end = text.find_first_not_of(base::kWhitespaceNoCrLfUTF16, begin);
      if ((end != base::string16::npos) && (text[end] != '\n') &&
          (text[end] != '\r')) {
        // Found a non-trailing whitespace sequence without CR/LF.  Copy it.
        seen_non_lf_whitespace = true;
        copy_range(begin, end);
        continue;
      }
    }

    // |end| either points at the end of the string or a CR/LF.
    if (end != base::string16::npos)
      end = text.find_first_not_of(base::kWhitespaceUTF16, end + 1);
    if (end == base::string16::npos)
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
                       seen_non_lf_whitespace ? kSpace : base::string16(),
                       &output);
  }

  return StripJavascriptSchemas(output);
}

OmniboxView::~OmniboxView() {
}

void OmniboxView::OpenMatch(const AutocompleteMatch& match,
                            WindowOpenDisposition disposition,
                            const GURL& alternate_nav_url,
                            const base::string16& pasted_text,
                            size_t selected_line,
                            base::TimeTicks match_selection_timestamp) {
  // Invalid URLs such as chrome://history can end up here.
  if (!match.destination_url.is_valid() || !model_)
    return;
  model_->OpenMatch(match, disposition, alternate_nav_url, pasted_text,
                    selected_line, match_selection_timestamp);
}

bool OmniboxView::IsEditingOrEmpty() const {
  return (model_.get() && model_->user_input_in_progress()) ||
      (GetOmniboxTextLength() == 0);
}

// TODO (manukh) OmniboxView::GetIcon is very similar to
// OmniboxPopupModel::GetMatchIcon. They contain certain inconsistencies
// concerning what flags are required to display url favicons and bookmark star
// icons. OmniboxPopupModel::GetMatchIcon also doesn't display default search
// provider icons. It's possible they have other inconsistencies as well. We may
// want to consider reusing the same code for both the popup and omnibox icons.
gfx::ImageSkia OmniboxView::GetIcon(int dip_size,
                                    SkColor color,
                                    IconFetchedCallback on_icon_fetched) const {
#if defined(OS_ANDROID) || defined(OS_IOS)
  // This is used on desktop only.
  NOTREACHED();
  return gfx::ImageSkia();
#else

  // For tests, model_ will be null.
  if (!model_) {
    AutocompleteMatch fake_match;
    fake_match.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
    const gfx::VectorIcon& vector_icon = fake_match.GetVectorIcon(false);
    return gfx::CreateVectorIcon(vector_icon, dip_size, color);
  }

  if (model_->ShouldShowCurrentPageIcon()) {
    // Query in Omnibox.
    LocationBarModel* location_bar_model = controller_->GetLocationBarModel();
    if (location_bar_model->GetDisplaySearchTerms(nullptr /* search_terms */)) {
      gfx::Image icon = model_->client()->GetFaviconForDefaultSearchProvider(
          std::move(on_icon_fetched));
      if (!icon.IsEmpty())
        return model_->client()->GetSizedIcon(icon).AsImageSkia();
    }

    return gfx::CreateVectorIcon(location_bar_model->GetVectorIcon(), dip_size,
                                 color);
  }

  gfx::Image favicon;
  AutocompleteMatch match = model_->CurrentMatch(nullptr);
  if (AutocompleteMatch::IsSearchType(match.type)) {
    // For search queries, display default search engine's favicon.
    favicon = model_->client()->GetFaviconForDefaultSearchProvider(
        std::move(on_icon_fetched));

  } else if (base::FeatureList::IsEnabled(
                 omnibox::kUIExperimentShowSuggestionFavicons)) {
    // For site suggestions, display site's favicon.
    favicon = model_->client()->GetFaviconForPageUrl(
        match.destination_url, std::move(on_icon_fetched));
  }

  if (!favicon.IsEmpty())
    return model_->client()->GetSizedIcon(favicon).AsImageSkia();
  // If the client returns an empty favicon, fall through to provide the
  // generic vector icon. |on_icon_fetched| may or may not be called later.
  // If it's never called, the vector icon we provide below should remain.

  // For bookmarked suggestions, display bookmark icon.
  bookmarks::BookmarkModel* bookmark_model =
      model_->client()->GetBookmarkModel();
  const bool is_bookmarked =
      bookmark_model && bookmark_model->IsBookmarked(match.destination_url);

  const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmarked);

  return gfx::CreateVectorIcon(vector_icon, dip_size, color);
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
}

void OmniboxView::SetUserText(const base::string16& text) {
  SetUserText(text, true);
}

void OmniboxView::SetUserText(const base::string16& text,
                              bool update_popup) {
  if (model_)
    model_->SetUserText(text);
  SetWindowTextAndCaretPos(text, text.length(), update_popup, true);
}

void OmniboxView::RevertAll() {
  CloseOmniboxPopup();
  if (model_)
    model_->Revert();
  TextChanged();
}

void OmniboxView::CloseOmniboxPopup() {
  if (model_)
    model_->StopAutocomplete();
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
      (before.text.length() > after.text.length()) &&
      (after.sel_start <= std::min(before.sel_start, before.sel_end));

  return state_changes;
}

OmniboxView::OmniboxView(OmniboxEditController* controller,
                         std::unique_ptr<OmniboxClient> client)
    : controller_(controller) {
  // |client| can be null in tests.
  if (client) {
    model_.reset(new OmniboxEditModel(this, controller, std::move(client)));
  }
}

void OmniboxView::TextChanged() {
  EmphasizeURLComponents();
  if (model_)
    model_->OnChanged();
}

bool OmniboxView::UpdateTextStyle(
    const base::string16& display_text,
    const bool text_is_url,
    const AutocompleteSchemeClassifier& classifier) {
  if (!text_is_url) {
    SetEmphasis(true, gfx::Range::InvalidRange());
    return false;  // Path not eligible for fading if it's not even a URL.
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

  const base::string16 url_scheme =
      display_text.substr(scheme.begin, scheme.len);
  // Extension IDs are not human-readable, so deemphasize everything to draw
  // attention to the human-readable name in the location icon text.
  // Data URLs are rarely human-readable and can be used for spoofing, so draw
  // attention to the scheme to emphasize "this is just a bunch of data".
  // For normal URLs, the host is the best proxy for "identity".
  if (url_scheme == base::UTF8ToUTF16(extensions::kExtensionScheme))
    deemphasize = EVERYTHING;
  else if (url_scheme == base::UTF8ToUTF16(url::kDataScheme))
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

  // Path is eligible for fading only when the host is the only emphasized part.
  return deemphasize == ALL_BUT_HOST;
}
