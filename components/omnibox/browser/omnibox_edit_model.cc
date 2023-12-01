// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_edit_model.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/history_fuzzy_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/cookies/cookie_util.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/vector_icons/vector_icons.h"  // nogncheck
#endif

using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;
using omnibox::mojom::NavigationPredictor;

// Helpers --------------------------------------------------------------------

namespace {

// The possible histogram values emitted when escape is pressed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OmniboxEscapeAction {
  // `kNone` doesn't mean escape did nothing (e.g. it could have stopped a
  // navigation), just that it did not affect the omnibox state.
  //  kNone = 0, No longer used since escape now always blurs the omnibox if it
  //             does nothing else.
  kRevertTemporaryText = 1,
  kClosePopup = 2,
  kClearUserInput = 3,
  kClosePopupAndClearUserInput = 4,
  kBlur = 5,
  kMaxValue = kBlur,
};

const char kOmniboxFocusResultedInNavigation[] =
    "Omnibox.FocusResultedInNavigation";

// Histogram name which counts the number of times the user enters
// keyword hint mode and via what method.  The possible values are listed
// in the metrics OmniboxEnteredKeywordMode2 enum which is defined in metrics
// enum XML file.
const char kEnteredKeywordModeHistogram[] = "Omnibox.EnteredKeywordMode2";

// Histogram name which counts the number of milliseconds a user takes
// between focusing and editing the omnibox.
const char kFocusToEditTimeHistogram[] = "Omnibox.FocusToEditTime";

// Histogram name which counts the number of milliseconds a user takes
// between focusing and opening an omnibox match.
const char kFocusToOpenTimeHistogram[] =
    "Omnibox.FocusToOpenTimeAnyPopupState3";

// Histogram name which counts the number of times the user completes a search
// in keyword mode, enumerated by how they enter keyword mode.
const char kAcceptedKeywordSuggestionHistogram[] =
    "Omnibox.AcceptedKeywordSuggestion";

// Histogram name which counts the number of times the user completes a search
// in keyword mode, enumerated by the type of search engine.
const char kKeywordModeUsageByEngineTypeEnteredHistogramName[] =
    "Omnibox.KeywordModeUsageByEngineType.Entered";

// Histogram name which counts the number of times the user completes a search
// in keyword mode, enumerated by the type of search engine.
const char kKeywordModeUsageByEngineTypeAcceptedHistogramName[] =
    "Omnibox.KeywordModeUsageByEngineType.Accepted";

void EmitEnteredKeywordModeHistogram(
    OmniboxEventProto::KeywordModeEntryMethod entry_method,
    const TemplateURL* turl) {
  UMA_HISTOGRAM_ENUMERATION(
      kEnteredKeywordModeHistogram, static_cast<int>(entry_method),
      static_cast<int>(OmniboxEventProto::KeywordModeEntryMethod_MAX + 1));

  if (turl != nullptr) {
    base::UmaHistogramEnumeration(
        kKeywordModeUsageByEngineTypeEnteredHistogramName,
        turl->GetBuiltinEngineType(),
        BuiltinEngineType::KEYWORD_MODE_ENGINE_TYPE_MAX);
  }
}

void EmitAcceptedKeywordSuggestionHistogram(
    OmniboxEventProto::KeywordModeEntryMethod entry_method,
    const TemplateURL* turl) {
  UMA_HISTOGRAM_ENUMERATION(
      kAcceptedKeywordSuggestionHistogram, static_cast<int>(entry_method),
      static_cast<int>(OmniboxEventProto::KeywordModeEntryMethod_MAX + 1));

  if (turl != nullptr) {
    base::UmaHistogramEnumeration(
        kKeywordModeUsageByEngineTypeAcceptedHistogramName,
        turl->GetBuiltinEngineType(),
        BuiltinEngineType::KEYWORD_MODE_ENGINE_TYPE_MAX);
  }
}

// `executed_selection` indicates which OmniboxAction within `result`
// was executed, and leaving this parameter as the default indicates
// that no action was executed.
void RecordActionShownForAllActions(
    const AutocompleteResult& result,
    OmniboxPopupSelection executed_selection =
        OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch)) {
  // Record the presence of all actions in the result set.
  for (size_t line_index = 0; line_index < result.size(); ++line_index) {
    const AutocompleteMatch& match = result.match_at(line_index);
    // Record the presence of the takeover action on this line, if any.
    if (match.takeover_action) {
      match.takeover_action->RecordActionShown(
          line_index,
          /*executed=*/line_index == executed_selection.line &&
              executed_selection.state == OmniboxPopupSelection::NORMAL);
    }
    for (size_t action_index = 0; action_index < match.actions.size();
         ++action_index) {
      match.actions[action_index]->RecordActionShown(
          line_index, /*executed=*/line_index == executed_selection.line &&
                          action_index == executed_selection.action_index &&
                          executed_selection.state ==
                              OmniboxPopupSelection::FOCUSED_BUTTON_ACTION);
    }
  }
}

// Find the number of IPv4 parts if the user inputs a URL with an IP address
// host. Returns 0 if the user does not manually types the full IP address.
size_t CountNumberOfIPv4Parts(const std::u16string& text,
                              const GURL& url,
                              size_t completed_length) {
  if (!url.HostIsIPAddress() || !url.SchemeIsHTTPOrHTTPS() ||
      completed_length > 0) {
    return 0;
  }

  url::Parsed parsed;
  url::ParseStandardURL(text.data(), text.length(), &parsed);
  if (!parsed.host.is_valid()) {
    return 0;
  }

  size_t parts = 1;
  bool potential_part = false;
  for (int i = parsed.host.begin; i < parsed.host.end(); i++) {
    if (text[i] == '.') {
      potential_part = true;
    }
    if (potential_part && text[i] >= '0' && text[i] <= '9') {
      parts++;
      potential_part = false;
    }
  }
  return parts;
}

}  // namespace

// OmniboxEditModel::State ----------------------------------------------------

OmniboxEditModel::State::State(
    bool user_input_in_progress,
    const std::u16string& user_text,
    const std::u16string& keyword,
    bool is_keyword_hint,
    OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method,
    OmniboxFocusState focus_state,
    OmniboxFocusSource focus_source,
    const AutocompleteInput& autocomplete_input)
    : user_input_in_progress(user_input_in_progress),
      user_text(user_text),
      keyword(keyword),
      is_keyword_hint(is_keyword_hint),
      keyword_mode_entry_method(keyword_mode_entry_method),
      focus_state(focus_state),
      focus_source(focus_source),
      autocomplete_input(autocomplete_input) {}

OmniboxEditModel::State::State(const State& other) = default;

OmniboxEditModel::State::~State() = default;

// OmniboxEditModel -----------------------------------------------------------

OmniboxEditModel::OmniboxEditModel(OmniboxController* controller,
                                   OmniboxView* view)
    : controller_(controller),
      view_(view),
      user_input_in_progress_(false),
      user_input_since_focus_(true),
      focus_resulted_in_navigation_(false),
      just_deleted_text_(false),
      has_temporary_text_(false),
      paste_state_(NONE),
      control_key_state_(UP),
      is_keyword_hint_(false),
      keyword_mode_entry_method_(OmniboxEventProto::INVALID),
      in_revert_(false),
      allow_exact_keyword_match_(false) {}

OmniboxEditModel::~OmniboxEditModel() = default;

void OmniboxEditModel::set_popup_view(OmniboxPopupView* popup_view) {
  popup_view_ = popup_view;

  // Clear/reset popup-related state.
  rich_suggestion_bitmaps_.clear();
  old_focused_url_ = GURL();
  popup_selection_ = OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                           OmniboxPopupSelection::NORMAL);
}

metrics::OmniboxEventProto::PageClassification
OmniboxEditModel::GetPageClassification() const {
  return controller_->client()->GetLocationBarModel()->GetPageClassification(
      focus_source_);
}

OmniboxEditModel::State OmniboxEditModel::GetStateForTabSwitch() const {
  // NOTE: it's important this doesn't attempt to access any state that
  // may come from the active WebContents. At the time this is called, the
  // active WebContents has already changed.

  // Like typing, switching tabs "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  std::u16string user_text;
  if (user_input_in_progress_) {
    const std::u16string display_text = GetText();
    if (!MaybePrependKeyword(display_text).empty())
      user_text = display_text;
    // Else case is user deleted all the text. The expectation (which matches
    // other browsers) is when the user restores the state a revert happens as
    // well as a select all. The revert shouldn't be done here, as at the time
    // this is called a revert would revert to the url of the newly activated
    // tab (because at the time this is called, the WebContents has already
    // changed). By leaving the |user_text| empty downstream code is able to
    // detect this and select all.
  } else {
    user_text = user_text_;
  }
  return State(user_input_in_progress_, user_text, keyword_, is_keyword_hint_,
               keyword_mode_entry_method_, focus_state_, focus_source_, input_);
}

void OmniboxEditModel::RestoreState(const State* state) {
  // We need to update the permanent display texts correctly and revert the
  // view regardless of whether there is saved state.
  ResetDisplayTexts();

  if (view_) {
    view_->RevertAll();
  }
  // Restore the autocomplete controller's input, or clear it if this is a new
  // tab.
  input_ = state ? state->autocomplete_input : AutocompleteInput();
  if (!state)
    return;

  // The tab-management system saves the last-focused control for each tab and
  // restores it. That operation also updates this edit model's focus_state_
  // if necessary. This occurs before we reach this point in the code.
  //
  // The only reason we need to separately save and restore our focus state is
  // to preserve our special "invisible focus" state used for the fakebox.
  //
  // However, in some circumstances (if the last-focused control was destroyed),
  // the Omnibox will be focused by default, and the edit model's saved state
  // may be invalid. We make a check to guard against that.
  bool saved_focus_state_invalid = focus_state_ == OMNIBOX_FOCUS_VISIBLE &&
                                   state->focus_state == OMNIBOX_FOCUS_NONE;
  if (!saved_focus_state_invalid) {
    SetFocusState(state->focus_state, OMNIBOX_FOCUS_CHANGE_TAB_SWITCH);
    focus_source_ = state->focus_source;
  }

  // Restore any user editing.
  if (state->user_input_in_progress) {
    // NOTE: Be sure to set keyword-related state AFTER invoking
    // SetUserText(), as SetUserText() clears the keyword state.
    if ((!state->user_text.empty() || !state->keyword.empty()) && view_) {
      view_->SetUserText(state->user_text, false);
    }
    keyword_ = state->keyword;
    is_keyword_hint_ = state->is_keyword_hint;
    keyword_mode_entry_method_ = state->keyword_mode_entry_method;
  } else if (!state->user_text.empty()) {
    // If the |user_input_in_progress| is false but we have |user_text|,
    // restore the |user_text| to the model and the view. It's likely unelided
    // text that the user has not made any modifications to.
    InternalSetUserText(state->user_text);

    // We let the View manage restoring the cursor position afterwards.
    if (view_) {
      view_->SetWindowTextAndCaretPos(state->user_text, 0, false, false);
    }
  }
}

AutocompleteMatch OmniboxEditModel::CurrentMatch(
    GURL* alternate_nav_url) const {
  // If we have a valid match use it. Otherwise get one for the current text.
  AutocompleteMatch match = current_match_;
  if (!match.destination_url.is_valid()) {
    GetInfoForCurrentText(&match, alternate_nav_url);
  } else if (alternate_nav_url) {
    AutocompleteProviderClient* provider_client =
        controller_->autocomplete_controller()->autocomplete_provider_client();
    *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        input_, match, provider_client);
  }
  return match;
}

bool OmniboxEditModel::ResetDisplayTexts() {
  const std::u16string old_display_text = GetPermanentDisplayText();

  LocationBarModel* location_bar_model =
      controller_->client()->GetLocationBarModel();
  url_for_editing_ = location_bar_model->GetFormattedFullURL();

#if BUILDFLAG(IS_IOS)
  // iOS is unusual in that it uses a separate LocationView to show the
  // LocationBarModel's display-only URL. The actual OmniboxViewIOS widget is
  // hidden in the defocused state, and always contains the URL for editing.
  display_text_ = url_for_editing_;
#else
  display_text_ = location_bar_model->GetURLForDisplay();
#endif

  // When there's new permanent text, and the user isn't interacting with the
  // omnibox, we want to revert the edit to show the new text.  We could simply
  // define "interacting" as "the omnibox has focus", but we still allow updates
  // when the omnibox has focus as long as the user hasn't begun editing, and
  // isn't seeing zerosuggestions (because changing this text would require
  // changing or hiding those suggestions).  When the omnibox doesn't have
  // focus, we assume the user may have abandoned their interaction and it's
  // always safe to change the text; this also prevents someone toggling "Show
  // URL" (which sounds as if it might be persistent) from seeing just that URL
  // forever afterwards.
  return (GetPermanentDisplayText() != old_display_text) &&
         (!has_focus() || (!user_input_in_progress_ && !PopupIsOpen()));
}

std::u16string OmniboxEditModel::GetPermanentDisplayText() const {
  return display_text_;
}

void OmniboxEditModel::SetUserText(const std::u16string& text) {
  SetInputInProgress(true);
  keyword_.clear();
  is_keyword_hint_ = false;
  keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
  InternalSetUserText(text);
  GetInfoForCurrentText(&current_match_, nullptr);
  paste_state_ = NONE;
  has_temporary_text_ = false;
}

bool OmniboxEditModel::Unelide() {
  // Unelision should not occur if the user has already inputted text.
  if (user_input_in_progress())
    return false;

  // No need to unelide if we are already displaying the full URL.
  if (GetText() ==
      controller_->client()->GetLocationBarModel()->GetFormattedFullURL()) {
    return false;
  }

  // Set the user text to the unelided URL, but don't change
  // |user_input_in_progress_|. This is to save the unelided URL on tab switch.
  InternalSetUserText(url_for_editing_);

  if (view_) {
    view_->SetWindowTextAndCaretPos(url_for_editing_, 0, false, false);

    // Select all in reverse to ensure the beginning of the URL is shown.
    view_->SelectAll(true /* reversed */);
  }

  return true;
}

void OmniboxEditModel::OnChanged() {
  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match =
      user_input_in_progress_ ? CurrentMatch(nullptr) : AutocompleteMatch();

  controller_->client()->OnTextChanged(current_match, user_input_in_progress_,
                                       user_text_, controller_->result(),
                                       has_focus());
}

void OmniboxEditModel::GetDataForURLExport(GURL* url,
                                           std::u16string* title,
                                           gfx::Image* favicon) {
  *url = CurrentMatch(nullptr).destination_url;
  if (*url == controller_->client()->GetURL()) {
    *title = controller_->client()->GetTitle();
    *favicon = controller_->client()->GetFavicon();
  }
}

bool OmniboxEditModel::CurrentTextIsURL() const {
  // If !user_input_in_progress_, we can determine if the text is a URL without
  // starting the autocomplete system. This speeds browser startup.
  return !user_input_in_progress_ ||
         !AutocompleteMatch::IsSearchType(CurrentMatch(nullptr).type);
}

void OmniboxEditModel::AdjustTextForCopy(int sel_min,
                                         std::u16string* text,
                                         GURL* url_from_text,
                                         bool* write_url) {
  DCHECK(text);
  DCHECK(url_from_text);
  DCHECK(write_url);

  *write_url = false;

  // Do not adjust if selection did not start at the beginning of the field.
  if (sel_min != 0)
    return;

  // If the user has not modified the display text and is copying the whole URL
  // text (whether it's in the elided or unelided form), copy the omnibox
  // contents as a hyperlink to the current page.
  if (!user_input_in_progress_ &&
      (*text == display_text_ || *text == url_for_editing_)) {
    *url_from_text = controller_->client()->GetLocationBarModel()->GetURL();
    *write_url = true;

    // Don't let users copy Reader Mode page URLs.
    // We display the original article's URL in the omnibox, so users will
    // expect that to be what is copied to the clipboard.
    if (dom_distiller::url_utils::IsDistilledPage(*url_from_text)) {
      *url_from_text = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
          *url_from_text);
    }
    *text = base::UTF8ToUTF16(url_from_text->spec());

    return;
  }

  // This code early exits if the copied text looks like a search query. It's
  // not at the very top of this method, as it would interpret the intranet URL
  // "printer/path" as a search query instead of a URL.
  //
  // We can't use CurrentTextIsURL() or GetDataForURLExport() because right now
  // the user is probably holding down control to cause the copy, which will
  // screw up our calculation of the desired_tld.
  AutocompleteMatch match_from_text;
  controller_->client()->GetAutocompleteClassifier()->Classify(
      *text, is_keyword_selected(), true, GetPageClassification(),
      &match_from_text, nullptr);
  if (AutocompleteMatch::IsSearchType(match_from_text.type))
    return;

  // Make our best GURL interpretation of |text|.
  *url_from_text = match_from_text.destination_url;

  // Get the current page GURL (or the GURL of the currently selected match).
  GURL current_page_url =
      controller_->client()->GetLocationBarModel()->GetURL();
  if (PopupIsOpen()) {
    AutocompleteMatch current_match = CurrentMatch(nullptr);
    if (!AutocompleteMatch::IsSearchType(current_match.type) &&
        current_match.destination_url.is_valid()) {
      // If the popup is open and a valid match is selected, treat that as the
      // current page, since the URL in the Omnibox will be from that match.
      current_page_url = current_match.destination_url;
    }
  }

  // If the user has altered the host piece of the omnibox text, then we cannot
  // guess at user intent, so early exit and leave |text| as-is as plain text.
  if (!current_page_url.SchemeIsHTTPOrHTTPS() ||
      !url_from_text->SchemeIsHTTPOrHTTPS() ||
      current_page_url.host_piece() != url_from_text->host_piece()) {
    return;
  }

  // Infer the correct scheme for the copied text, and prepend it if necessary.
  {
    const std::u16string http =
        base::StrCat({url::kHttpScheme16, url::kStandardSchemeSeparator16});
    const std::u16string https =
        base::StrCat({url::kHttpsScheme16, url::kStandardSchemeSeparator16});

    const std::u16string& current_page_url_prefix =
        current_page_url.SchemeIs(url::kHttpScheme) ? http : https;

    // Only prepend a scheme if the text doesn't already have a scheme.
    if (!base::StartsWith(*text, http, base::CompareCase::INSENSITIVE_ASCII) &&
        !base::StartsWith(*text, https, base::CompareCase::INSENSITIVE_ASCII)) {
      *text = current_page_url_prefix + *text;

      // Amend the copied URL to match the prefixed string.
      GURL::Replacements replace_scheme;
      replace_scheme.SetSchemeStr(current_page_url.scheme_piece());
      *url_from_text = url_from_text->ReplaceComponents(replace_scheme);
    }
  }

  // If the URL derived from |text| is valid, mark |write_url| true, and modify
  // |text| to contain the canonical URL spec with non-ASCII characters escaped.
  if (url_from_text->is_valid()) {
    *write_url = true;
    *text = base::UTF8ToUTF16(url_from_text->spec());
  }
}

bool OmniboxEditModel::ShouldShowCurrentPageIcon() const {
  // If the popup is open, don't show the current page's icon. The caller is
  // instead expected to show the current match's icon.
  if (PopupIsOpen()) {
    return false;
  }

  // On the New Tab Page, the omnibox textfield is empty. We want to display
  // the default search provider favicon instead of the NTP security icon.
  if (GetText().empty()) {
    return false;
  }

  // If user input is not in progress, always show the current page's icon.
  if (!user_input_in_progress())
    return true;

  // If user input is in progress, keep showing the current page's icon so long
  // as the text matches the current page's URL, elided or unelided.
  return GetText() == display_text_ || GetText() == url_for_editing_;
}

ui::ImageModel OmniboxEditModel::GetSuperGIcon(int image_size,
                                               bool dark_mode) const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (dark_mode) {
    return ui::ImageModel::FromVectorIcon(
        vector_icons::kGoogleGLogoMonochromeIcon, ui::kColorRefPrimary100,
        image_size);
  } else {
    // The icon color does not matter in this case since this icon has colors
    // hardcoded into it.
    return ui::ImageModel::FromVectorIcon(vector_icons::kGoogleSuperGIcon,
                                          gfx::kPlaceholderColor, image_size);
  }
#else
  return ui::ImageModel();
#endif
}

void OmniboxEditModel::UpdateInput(bool has_selected_text,
                                   bool prevent_inline_autocomplete) {
  bool changed_to_user_input_in_progress = SetInputInProgressNoNotify(true);
  if (!has_focus()) {
    if (changed_to_user_input_in_progress)
      NotifyObserversInputInProgress(true);
    return;
  }

  if (!is_keyword_selected() && changed_to_user_input_in_progress &&
      user_text_.empty()) {
    // In the case the user enters user-input-in-progress mode by clearing
    // everything (i.e. via Backspace), ask for ZeroSuggestions instead of the
    // normal prefix (as-you-type) autocomplete.
    //
    // We also check that no keyword is selected, as otherwise that breaks
    // entering keyword mode via Ctrl+K.
    //
    // TODO(tommycli): The difference between a ZeroSuggest request and a normal
    // prefix autocomplete request is getting fuzzier, and should be fully
    // encapsulated by the AutocompleteInput::focus_type() member. We should
    // merge these two calls soon, lest we confuse future developers.
    StartZeroSuggestRequest(/*user_clobbered_permanent_text=*/true);
  } else {
    // Otherwise run the normal prefix (as-you-type) autocomplete.
    StartAutocomplete(has_selected_text, prevent_inline_autocomplete);
  }

  if (changed_to_user_input_in_progress)
    NotifyObserversInputInProgress(true);
}

void OmniboxEditModel::SetInputInProgress(bool in_progress) {
  if (SetInputInProgressNoNotify(in_progress))
    NotifyObserversInputInProgress(in_progress);
}

void OmniboxEditModel::Revert() {
  SetInputInProgress(false);
  input_.Clear();
  paste_state_ = NONE;
  InternalSetUserText(std::u16string());
  keyword_.clear();
  is_keyword_hint_ = false;
  keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
  has_temporary_text_ = false;
  size_t start, end;
  if (view_) {
    view_->GetSelectionBounds(&start, &end);
  }
  current_match_ = AutocompleteMatch();
  // First home the cursor, so view of text is scrolled to left, then correct
  // it. |SetCaretPos()| doesn't scroll the text, so doing that first wouldn't
  // accomplish anything.
  std::u16string current_permanent_url = GetPermanentDisplayText();
  if (view_) {
    view_->SetWindowTextAndCaretPos(current_permanent_url, 0, false, true);
    view_->SetCaretPos(std::min(current_permanent_url.length(), start));
  }
  controller_->client()->OnRevert();
}

void OmniboxEditModel::StartAutocomplete(bool has_selected_text,
                                         bool prevent_inline_autocomplete) {
  const std::u16string input_text = MaybePrependKeyword(user_text_);

  size_t start, cursor_position;
  // This method currently only works when there's a view, but ideally the
  // model should be primary for determining such state.
  CHECK(view_);
  view_->GetSelectionBounds(&start, &cursor_position);

  // For keyword searches, the text that AutocompleteInput expects is
  // of the form "<keyword> <query>", where our query is |user_text_|.
  // So we need to adjust the cursor position forward by the length of
  // any keyword added by MaybePrependKeyword() above.
  if (is_keyword_selected()) {
    // If there is user text, the cursor is past the keyword and doesn't
    // account for its size.  Add the keyword's size to the position passed
    // to autocomplete.
    if (!user_text_.empty()) {
      cursor_position += input_text.length() - user_text_.length();
    } else {
      // Otherwise, cursor may point into keyword or otherwise not account
      // for the keyword's size (depending on how this code is reached).
      // Pass a cursor at end of input to autocomplete.  This is safe in all
      // conditions.
      cursor_position = input_text.length();
    }
  }
  input_ = AutocompleteInput(
      input_text, cursor_position, GetPageClassification(),
      controller_->client()->GetSchemeClassifier(),
      controller_->client()->ShouldDefaultTypedNavigationsToHttps(),
      controller_->client()->GetHttpsPortForTesting(),
      controller_->client()->IsUsingFakeHttpsForHttpsUpgradeTesting());
  input_.set_current_url(controller_->client()->GetURL());
  input_.set_current_title(controller_->client()->GetTitle());
  input_.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete || just_deleted_text_ ||
      (has_selected_text && inline_autocompletion_.empty() &&
       prefix_autocompletion_.empty()) ||
      paste_state_ != NONE);
  input_.set_prefer_keyword(is_keyword_selected());
  input_.set_allow_exact_keyword_match(is_keyword_selected() ||
                                       allow_exact_keyword_match_);
  input_.set_keyword_mode_entry_method(keyword_mode_entry_method_);

  controller_->StartAutocomplete(input_);
}

void OmniboxEditModel::StartPrefetch() {
  auto page_classification =
      controller_->client()->GetLocationBarModel()->GetPageClassification(
          OmniboxFocusSource::OMNIBOX,
          /*is_prefetch=*/true);
  if (!OmniboxFieldTrial::IsZeroSuggestPrefetchingEnabledInContext(
          page_classification)) {
    return;
  }

  const bool is_ntp_page = omnibox::IsNTPPage(page_classification);
  const bool interaction_clobber_focus_type = base::FeatureList::IsEnabled(
      omnibox::kOmniboxOnClobberFocusTypeOnContent);

  GURL current_url = controller_->client()->GetURL();
  std::u16string text = base::UTF8ToUTF16(current_url.spec());

  if (is_ntp_page || interaction_clobber_focus_type) {
    text.clear();
  }

  AutocompleteInput input(text, page_classification,
                          controller_->client()->GetSchemeClassifier());
  input.set_current_url(current_url);
  input.set_focus_type(interaction_clobber_focus_type && !is_ntp_page
                           ? metrics::OmniboxFocusType::INTERACTION_CLOBBER
                           : metrics::OmniboxFocusType::INTERACTION_FOCUS);
  controller_->autocomplete_controller()->StartPrefetch(input);
}

void OmniboxEditModel::StopAutocomplete() {
  controller_->autocomplete_controller()->Stop(true);
}

bool OmniboxEditModel::CanPasteAndGo(const std::u16string& text) const {
  if (!controller_->client()->IsPasteAndGoEnabled()) {
    return false;
  }

  AutocompleteMatch match;
  ClassifyString(text, &match, nullptr);
  return match.destination_url.is_valid();
}

void OmniboxEditModel::PasteAndGo(const std::u16string& text,
                                  base::TimeTicks match_selection_timestamp) {
  DCHECK(CanPasteAndGo(text));

  if (view_) {
    view_->RevertAll();
  }
  AutocompleteMatch match;
  GURL alternate_nav_url;
  ClassifyString(text, &match, &alternate_nav_url);

  GURL upgraded_url;
  if (match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED &&
      controller_->client()->ShouldDefaultTypedNavigationsToHttps() &&
      AutocompleteInput::ShouldUpgradeToHttps(
          text, match.destination_url,
          controller_->client()->GetHttpsPortForTesting(),
          controller_->client()->IsUsingFakeHttpsForHttpsUpgradeTesting(),
          &upgraded_url)) {
    input_.set_added_default_scheme_to_typed_url(true);
    DCHECK(upgraded_url.is_valid());
    match.destination_url = upgraded_url;
  } else {
    input_.set_added_default_scheme_to_typed_url(false);
  }

  OpenMatch(OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch), match,
            WindowOpenDisposition::CURRENT_TAB, alternate_nav_url, text,
            match_selection_timestamp);
}

void OmniboxEditModel::EnterKeywordModeForDefaultSearchProvider(
    OmniboxEventProto::KeywordModeEntryMethod entry_method) {
  if (!controller_->client()->IsDefaultSearchProviderEnabled()) {
    return;
  }

  controller_->autocomplete_controller()->Stop(false);

  const TemplateURL* default_search_provider = controller_->client()
                                                   ->GetTemplateURLService()
                                                   ->GetDefaultSearchProvider();
  DCHECK(default_search_provider);
  keyword_ = default_search_provider->keyword();
  is_keyword_hint_ = false;
  keyword_mode_entry_method_ = entry_method;

  std::u16string display_text =
      user_input_in_progress_ ? GetText() : std::u16string();
  size_t caret_pos = display_text.length();
  if (entry_method == OmniboxEventProto::QUESTION_MARK) {
    display_text.erase(0, 1);
    caret_pos = 0;
  }

  InternalSetUserText(display_text);
  if (view_) {
    view_->SetWindowTextAndCaretPos(display_text, caret_pos, true, false);
    if (entry_method == OmniboxEventProto::KEYBOARD_SHORTCUT) {
      view_->SelectAll(false);
    }
  }

  EmitEnteredKeywordModeHistogram(entry_method, default_search_provider);
}

void OmniboxEditModel::OpenSelection(OmniboxPopupSelection selection,
                                     base::TimeTicks timestamp,
                                     WindowOpenDisposition disposition) {
  // Intentionally accept input when selection has no line.
  // This will usually reach `OpenMatch` indirectly.
  if (selection.line >= controller_->result().size()) {
    AcceptInput(disposition, timestamp);
    return;
  }

  // The keyword mode button doesn't commit the omnibox, it's a
  // transient UI element leading to other normal omnibox selections.
  if (selection.state == OmniboxPopupSelection::KEYWORD_MODE) {
    return;
  }

  const AutocompleteMatch& match =
      controller_->result().match_at(selection.line);

  if (selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_HEADER) {
    DCHECK(match.suggestion_group_id.has_value());

    const bool current_visibility =
        controller_->IsSuggestionGroupHidden(match.suggestion_group_id.value());
    controller_->SetSuggestionGroupHidden(match.suggestion_group_id.value(),
                                          !current_visibility);
  } else if (selection.state ==
             OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION) {
    TryDeletingPopupLine(selection.line);
  } else {
    // Mark instant keyword as used if we're in keyword mode for a
    // starter pack keyword with its original '@' prefix intact.
    if (OmniboxFieldTrial::IsKeywordModeRefreshEnabled() && !keyword_.empty()) {
      PrefService* prefs = GetPrefService();
      TemplateURL* turl = controller_->client()
                              ->GetTemplateURLService()
                              ->GetTemplateURLForKeyword(keyword_);
      if (prefs && turl && turl->starter_pack_id() != 0 &&
          turl->keyword().starts_with(u'@')) {
        prefs->SetBoolean(omnibox::kOmniboxInstantKeywordUsed, true);
      }
    }

    // Open the match.
    GURL alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        input_, match,
        controller_->autocomplete_controller()->autocomplete_provider_client());
    OpenMatch(selection, match, disposition, alternate_nav_url,
              std::u16string(), timestamp);
  }
}

void OmniboxEditModel::OpenSelection(base::TimeTicks timestamp,
                                     WindowOpenDisposition disposition) {
  OpenSelection(popup_selection_, timestamp, disposition);
}

bool OmniboxEditModel::AcceptKeyword(
    OmniboxEventProto::KeywordModeEntryMethod entry_method) {
  TRACE_EVENT0("omnibox", "OmniboxEditModel::AcceptKeyword");

  DCHECK(!keyword_.empty()) << keyword_;

  controller_->autocomplete_controller()->Stop(false);

  is_keyword_hint_ = false;
  keyword_mode_entry_method_ = entry_method;
  if (original_user_text_with_keyword_.empty()) {
    original_user_text_with_keyword_ = user_text_;
  }
  user_text_ = MaybeStripKeyword(user_text_);

  if (PopupIsOpen()) {
    OmniboxPopupSelection selection = GetPopupSelection();
    selection.state = OmniboxPopupSelection::KEYWORD_MODE;
    SetPopupSelection(selection);
  } else {
    StartAutocomplete(false, true);
  }

  // When user text is empty (the user hasn't typed anything beyond the
  // keyword), the new text to show is whatever the newly-selected match in the
  // dropdown is.  When user text is not empty, however, we should make sure to
  // use the actual |user_text_| as the basis for the new text.  This ensures
  // that if the user types "<keyword><space>" and the default match would have
  // inline autocompleted a further string (e.g. because there's a past
  // multi-word search beginning with this keyword), the inline autocompletion
  // doesn't get filled in as the keyword search query text.
  //
  // We also treat tabbing into keyword mode like tabbing through the popup in
  // that we set |has_temporary_text_|, whereas pressing space is treated like
  // a new keystroke that changes the current match instead of overlaying it
  // with a temporary one.  This is important because rerunning autocomplete
  // after the user pressed space, which will have happened just before reaching
  // here, may have generated a new match, which the user won't actually see and
  // which we don't want to switch back to when exiting keyword mode; see
  // comments in ClearKeyword().

  if (view_) {
    view_->OnTemporaryTextMaybeChanged(user_text_, {}, !has_temporary_text_,
                                       true);
    if (!user_text_.empty())
      view_->UpdatePopup();
  }

  base::RecordAction(base::UserMetricsAction("AcceptedKeywordHint"));
  const TemplateURL* turl =
      controller_->client()->GetTemplateURLService()->GetTemplateURLForKeyword(
          keyword_);
  EmitEnteredKeywordModeHistogram(entry_method, turl);

  return true;
}

void OmniboxEditModel::AcceptTemporaryTextAsUserText() {
  InternalSetUserText(GetText());
  has_temporary_text_ = false;

  if (user_input_in_progress_ || !in_revert_)
    controller_->client()->OnInputStateChanged();
}

void OmniboxEditModel::ClearKeyword() {
  if (!is_keyword_selected() || !view_) {
    return;
  }

  TRACE_EVENT0("omnibox", "OmniboxEditModel::ClearKeyword");
  controller_->autocomplete_controller()->Stop(false);

  // While we're always in keyword mode upon reaching here, sometimes we've just
  // toggled in via space or tab, and sometimes we're on a non-toggled line
  // (usually because the user has typed a search string).  Keep track of the
  // difference, as we'll need it below. `popup_view_` may be nullptr in tests.
  bool was_toggled_into_keyword_mode =
      popup_view_ &&
      GetPopupSelection().state == OmniboxPopupSelection::KEYWORD_MODE;

  bool entry_by_tab = keyword_mode_entry_method_ == OmniboxEventProto::TAB;

  controller_->ClearPopupKeywordMode();

  // There are several possible states we could have been in before the user hit
  // backspace or shift-tab to enter this function:
  // (1) was_toggled_into_keyword_mode == false, entry_by_tab == false
  //     The user typed a further key after being in keyword mode already, e.g.
  //     "google.com f".
  // (2) was_toggled_into_keyword_mode == false, entry_by_tab == true
  //     The user tabbed away from a dropdown entry in keyword mode, then tabbed
  //     back to it, e.g. "google.com f<tab><shift-tab>".
  // (3) was_toggled_into_keyword_mode == true, entry_by_tab == false
  //     The user had just typed space to enter keyword mode, e.g.
  //     "google.com ".
  // (4) was_toggled_into_keyword_mode == true, entry_by_tab == true
  //     The user had just typed tab to enter keyword mode, e.g.
  //     "google.com<tab>".
  //
  // For states 1-3, we can safely handle the exit from keyword mode by using
  // OnBefore/AfterPossibleChange() to do a complete state update of all
  // objects.  However, with state 4, if we do this, and if the user had tabbed
  // into keyword mode on a line in the middle of the dropdown instead of the
  // first line, then the state update will rerun autocompletion and reset the
  // whole dropdown, and end up with the first line selected instead, instead of
  // just "undoing" the keyword mode entry on the non-first line.  So in this
  // case we simply reset |is_keyword_hint_| to true and update the window text.
  //
  // You might wonder why we don't simply do this in all cases.  In states 1-2,
  // getting out of keyword mode likely shouldn't put us in keyword hint mode;
  // if the user typed "google.com f" and then put the cursor before 'f' and hit
  // backspace, the resulting text would be "google.comf", which is unlikely to
  // be a keyword.  Unconditionally putting things back in keyword hint mode is
  // going to lead to internally inconsistent state, and possible future
  // crashes.  State 3 is more subtle; here we need to do the full state update
  // because before entering keyword mode to begin with, we will have re-run
  // autocomplete in ways that can produce surprising results if we just switch
  // back out of keyword mode.  For example, if a user has a keyword named "x",
  // an inline-autocompletable history site "xyz.com", and a lower-ranked
  // inline-autocompletable search "x y", then typing "x" will inline-
  // autocomplete to "xyz.com", hitting space will toggle into keyword mode, but
  // then hitting backspace could wind up with the default match as the "x y"
  // search, which feels bizarre.
  if (was_toggled_into_keyword_mode && entry_by_tab) {
    // State 4 above.
    is_keyword_hint_ = true;
    keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
    const std::u16string window_text = keyword_ + view_->GetText();
    view_->SetWindowTextAndCaretPos(window_text, keyword_.length(), false,
                                    true);
    // TODO (manukh): Exiting keyword mode in this case will not restore
    // the omnibox additional text. E.g., if before entering keyword mode, the
    // location bar displays 'google.com | (Google)', after entering (via tab)
    // and leaving keyword mode, it will display 'google.com' leaving keyword
    // mode, it will only display 'Google.com'.
  } else {
    // States 1-3 above.
    view_->OnBeforePossibleChange();
    // Add a space after the keyword to allow the user to continue typing
    // without re-enabling keyword mode.  The common case is state 3, where
    // the user entered keyword mode unintentionally, so backspacing
    // immediately out of keyword mode should keep the space.  In states 1 and
    // 2, having the space is "safer" behavior.  For instance, if the user types
    // "google.com f" or "google.com<tab>f" in the omnibox, moves the cursor to
    // the left, and presses backspace to leave keyword mode (state 1), it's
    // better to have the space because it may be what the user wanted.  The
    // user can easily delete it.  On the other hand, if there is no space and
    // the user wants it, it's more work to add because typing the space will
    // enter keyword mode, which then the user would have to leave again.

    // If we entered keyword mode in a special way like using a keyboard
    // shortcut or typing a question mark in a blank omnibox, don't restore the
    // keyword.  Instead, restore the question mark iff the user originally
    // typed one.
    std::u16string prefix;
    if (keyword_mode_entry_method_ == OmniboxEventProto::QUESTION_MARK)
      prefix = u"?";
    else if (keyword_mode_entry_method_ != OmniboxEventProto::KEYBOARD_SHORTCUT)
      prefix = keyword_ + u" ";

    keyword_.clear();
    is_keyword_hint_ = false;
    keyword_mode_entry_method_ = OmniboxEventProto::INVALID;

    view_->SetWindowTextAndCaretPos(prefix + view_->GetText(), prefix.length(),
                                    false, false);

    view_->OnAfterPossibleChange(false);
  }
}

void OmniboxEditModel::ClearAdditionalText() {
  TRACE_EVENT0("omnibox", "OmniboxEditModel::ClearAdditionalText");
  if (view_) {
    view_->SetAdditionalText(std::u16string());
  }
}

void OmniboxEditModel::OnSetFocus(bool control_down) {
  TRACE_EVENT0("omnibox", "OmniboxEditModel::OnSetFocus");
  last_omnibox_focus_ = base::TimeTicks::Now();
  user_input_since_focus_ = false;
  focus_resulted_in_navigation_ = false;

  // If the omnibox lost focus while the caret was hidden and then regained
  // focus, OnSetFocus() is called and should restore visibility. Note that
  // focus can be regained without an accompanying call to
  // OmniboxView::SetFocus(), e.g. by tabbing in.
  SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  // On focusing the omnibox, if the ctrl key is pressed, we don't want to
  // trigger ctrl-enter behavior unless it is released and re-pressed. For
  // example, if the user presses ctrl-l to focus the omnibox.
  control_key_state_ = control_down ? DOWN_AND_CONSUMED : UP;

  if (user_input_in_progress_ || !in_revert_)
    controller_->client()->OnInputStateChanged();
}

void OmniboxEditModel::StartZeroSuggestRequest(
    bool user_clobbered_permanent_text) {
  // Early exit if a query is already in progress or the popup is already open.
  // This is what allows this method to be called multiple times in multiple
  // code locations without harm.
  if (controller_->query_in_progress() || PopupIsOpen()) {
    return;
  }

  // Early exit if the page has not loaded yet, so we don't annoy users.
  if (!controller_->client()->CurrentPageExists()) {
    return;
  }

  // Early exit if the user already has a navigation or search query in mind.
  if (user_input_in_progress_ && !user_clobbered_permanent_text)
    return;

  TRACE_EVENT0("omnibox", "OmniboxEditModel::StartZeroSuggestRequest");

  // Send the textfield contents exactly as-is, as otherwise the verbatim
  // match can be wrong. The full page URL is anyways in set_current_url().
  // Don't attempt to use https as the default scheme for these requests.
  input_ = AutocompleteInput(
      GetText(), GetPageClassification(),
      controller_->client()->GetSchemeClassifier(),
      /*should_use_https_as_default_scheme=*/false,
      controller_->client()->GetHttpsPortForTesting(),
      controller_->client()->IsUsingFakeHttpsForHttpsUpgradeTesting());
  input_.set_current_url(controller_->client()->GetURL());
  input_.set_current_title(controller_->client()->GetTitle());
  input_.set_focus_type(user_clobbered_permanent_text
                            ? metrics::OmniboxFocusType::INTERACTION_CLOBBER
                            : metrics::OmniboxFocusType::INTERACTION_FOCUS);
  controller_->autocomplete_controller()->Start(input_);
}

void OmniboxEditModel::SetCaretVisibility(bool visible) {
  // Caret visibility only matters if the omnibox has focus.
  if (focus_state_ != OMNIBOX_FOCUS_NONE) {
    SetFocusState(visible ? OMNIBOX_FOCUS_VISIBLE : OMNIBOX_FOCUS_INVISIBLE,
                  OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  }
}

void OmniboxEditModel::ConsumeCtrlKey() {
  if (control_key_state_ == DOWN)
    control_key_state_ = DOWN_AND_CONSUMED;
}

void OmniboxEditModel::OnWillKillFocus() {
  if (user_input_in_progress_ || !in_revert_)
    controller_->client()->OnInputStateChanged();
}

void OmniboxEditModel::OnKillFocus() {
  UMA_HISTOGRAM_BOOLEAN(kOmniboxFocusResultedInNavigation,
                        focus_resulted_in_navigation_);
  SetFocusState(OMNIBOX_FOCUS_NONE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  focus_source_ = OmniboxFocusSource::INVALID;
  last_omnibox_focus_ = base::TimeTicks();
  paste_state_ = NONE;
  control_key_state_ = UP;
#if BUILDFLAG(IS_WIN)
  if (view_) {
    view_->HideImeIfNeeded();
  }
#endif
}

bool OmniboxEditModel::OnEscapeKeyPressed() {
  const char* kOmniboxEscapeHistogramName = "Omnibox.Escape";

  // If there is temporary text (i.e. a non default suggestion is selected),
  // revert it.
  if (has_temporary_text_) {
    base::UmaHistogramEnumeration(kOmniboxEscapeHistogramName,
                                  OmniboxEscapeAction::kRevertTemporaryText);
    RevertTemporaryTextAndPopup();
    return true;
  }

  // We do not clear the pending entry from the omnibox when a load is first
  // stopped.  If the user presses Escape while stopped, whether editing or not,
  // we clear it.
  if (controller_->client()->CurrentPageExists() &&
      !controller_->client()->IsLoading()) {
    controller_->client()->DiscardNonCommittedNavigations();
    if (view_) {
      view_->Update();
    }
  }

  // Close the popup if it's open.
  if (PopupIsOpen()) {
    base::UmaHistogramEnumeration(kOmniboxEscapeHistogramName,
                                  OmniboxEscapeAction::kClosePopup);
    if (view_) {
      view_->CloseOmniboxPopup();
    }
    return true;
  }

  // Unconditionally revert/select all.  This ensures any popup, whether due to
  // normal editing or ZeroSuggest, is closed, and the full text is selected.
  // This in turn allows the user to use escape to quickly select all the text
  // for ease of replacement, and matches other browsers.
  bool user_input_was_in_progress = user_input_in_progress_;
  // TODO(crbug.com/1340378): If the popup was open, `user_input_in_progress_`
  //  *should* also be true; checking `user_text_` in the DCHECK below, and
  //  checking `popup_was_open` in the if predicate below *should* be
  //  unnecessary. However, that's not always the case (see
  //  `user_input_in_progress_` comment in the header).
  if (view_) {
    view_->RevertAll();
    view_->SelectAll(true);
  }
  if (user_input_was_in_progress) {
    base::UmaHistogramEnumeration(kOmniboxEscapeHistogramName,
                                  OmniboxEscapeAction::kClearUserInput);
    // If the user was in the midst of editing, don't cancel any underlying page
    // load.  This doesn't match IE or Firefox, but seems more correct.  Note
    // that we do allow the page load to be stopped in the case where
    // ZeroSuggest was visible; this is so that it's still possible to focus the
    // address bar and hit escape once to stop a load even if the address being
    // loaded triggers the ZeroSuggest popup.
    return true;
  }

  // Blur the omnibox and focus the web contents.
  base::UmaHistogramEnumeration(kOmniboxEscapeHistogramName,
                                OmniboxEscapeAction::kBlur);
  controller_->client()->FocusWebContents();
  return true;
}

void OmniboxEditModel::OnControlKeyChanged(bool pressed) {
  if (pressed == (control_key_state_ == UP))
    control_key_state_ = pressed ? DOWN : UP;
}

void OmniboxEditModel::OnPaste() {
  UMA_HISTOGRAM_COUNTS_1M("Omnibox.Paste", 1);
  paste_state_ = PASTING;
}

void OmniboxEditModel::OnUpOrDownPressed(bool down, bool page) {
  // NOTE: This purposefully doesn't trigger any code that resets
  // `paste_state_`.

  // The popup could be working on a query but is not open. In that case,
  // force it to open immediately.
  if (MaybeStartQueryForPopup() || !PopupIsOpen())
    return;

  const auto direction =
      down ? OmniboxPopupSelection::kForward : OmniboxPopupSelection::kBackward;
  const auto step = page ? OmniboxPopupSelection::kAllLines
                         : OmniboxPopupSelection::kWholeLine;

  // The popup is open, so the user should be able to interact with it normally.

  // If, as a result of the key press, we would select the first result, then
  // we should revert the temporary text same as what pressing escape would
  // have done.
  //
  // Reverting, however, does not make sense for on-focus suggestions
  // (user_input_in_progress_ is false) unless the first result is a
  // verbatim match of the omnibox input (on-focus query refinements on SERP).
  const OmniboxPopupSelection next_selection =
      popup_selection_.GetNextSelection(
          controller_->result(), GetPrefService(),
          controller_->client()->GetTemplateURLService(), direction, step);
  if (controller_->result().default_match() && has_temporary_text_ &&
      next_selection.line == 0 &&
      (user_input_in_progress_ ||
       controller_->result().default_match()->IsVerbatimType())) {
    RevertTemporaryTextAndPopup();
  } else {
    // Call `StepPopupSelection()` instead of `SetPopupSelection()`, as the
    // former handles entering and leaving keyword mode before calling the
    // latter.
    StepPopupSelection(direction, step);
    DCHECK(popup_selection_ == next_selection);

    // Inform the client that a new row is now selected.
    OnNavigationLikely(popup_selection_.line,
                       NavigationPredictor::kUpOrDownArrowButton);
  }
}

void OmniboxEditModel::OnTabPressed(bool shift) {
  StepPopupSelection(shift ? OmniboxPopupSelection::kBackward
                           : OmniboxPopupSelection::kForward,
                     OmniboxPopupSelection::kStateOrLine);
}

bool OmniboxEditModel::OnSpacePressed() {
  if (!OmniboxFieldTrial::IsKeywordModeRefreshEnabled()) {
    return false;
  }
  if (!GetPrefService()->GetBoolean(omnibox::kKeywordSpaceTriggeringEnabled)) {
    return false;
  }
  if (!is_keyword_hint_ && keyword_.empty() &&
      input_.cursor_position() == input_.text().length()) {
    // Keywords can now be accessed anywhere in the match list. If one is
    // found on an instant keyword match, select and accept it.
    const AutocompleteResult& result = controller_->result();
    for (size_t i = 0; i < result.size(); i++) {
      const AutocompleteMatch& match = result.match_at(i);
      if (input_.text() == match.keyword &&
          match.HasInstantKeyword(
              controller_->client()->GetTemplateURLService())) {
        SetPopupSelection(OmniboxPopupSelection(i));
        AcceptKeyword(metrics::OmniboxEventProto::SPACE_AT_END);
        return true;
      }
    }
  }
  return false;
}

void OmniboxEditModel::OnNavigationLikely(
    size_t line,
    NavigationPredictor navigation_predictor) {
  if (controller_->result().empty()) {
    return;
  }

  if (line == OmniboxPopupSelection::kNoMatch) {
    return;
  }

  if (line >= controller_->result().size()) {
    return;
  }

  controller_->client()->OnNavigationLikely(
      line, controller_->result().match_at(line), navigation_predictor);
}

void OmniboxEditModel::OpenMatchForTesting(
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t index,
    base::TimeTicks match_selection_timestamp) {
  OpenMatch(OmniboxPopupSelection(index), match, disposition, alternate_nav_url,
            pasted_text, match_selection_timestamp);
}

void OmniboxEditModel::OnPopupDataChanged(
    const std::u16string& temporary_text,
    bool is_temporary_text,
    const std::u16string& inline_autocompletion,
    const std::u16string& prefix_autocompletion,
    const std::u16string& keyword,
    bool is_keyword_hint,
    const std::u16string& additional_text,
    const AutocompleteMatch& new_match) {
  current_match_ = new_match;
  if (!original_user_text_with_keyword_.empty() && !is_temporary_text &&
      (keyword.empty() || is_keyword_hint)) {
    user_text_ = original_user_text_with_keyword_;
    original_user_text_with_keyword_.clear();
  }

  // Update keyword/hint-related local state.
  bool keyword_state_changed =
      (keyword_ != keyword) ||
      ((is_keyword_hint_ != is_keyword_hint) && !keyword.empty());
  if (keyword_state_changed) {
    bool keyword_was_selected = is_keyword_selected();
    keyword_ = keyword;
    is_keyword_hint_ = is_keyword_hint;
    if (!keyword_was_selected && is_keyword_selected()) {
      // Since we entered keyword mode, record the reason. Note that we
      // don't do this simply because the keyword changes, since the user
      // never left keyword mode.
      keyword_mode_entry_method_ = OmniboxEventProto::SELECT_SUGGESTION;
    } else if (!is_keyword_selected()) {
      // We've left keyword mode, so align the entry method field with that.
      keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
    }

    // |is_keyword_hint_| should always be false if |keyword_| is empty.
    DCHECK(!keyword_.empty() || !is_keyword_hint_);
  }

  // Handle changes to temporary text.
  if (is_temporary_text) {
    const bool save_original_selection = !has_temporary_text_;
    if (save_original_selection) {
      // Save the original selection and URL so it can be reverted later.
      has_temporary_text_ = true;
      inline_autocompletion_.clear();
      prefix_autocompletion_.clear();
      if (view_) {
        view_->OnInlineAutocompleteTextCleared();
      }
    }
    // Arrowing around the popup cancels control-enter.
    ConsumeCtrlKey();
    // Now things are a bit screwy: the desired_tld has changed, but if we
    // update the popup, the new order of entries won't match the old, so the
    // user's selection gets screwy; and if we don't update the popup, and the
    // user reverts, then the selected item will be as if control is still
    // pressed, even though maybe it isn't any more.  There is no obvious
    // right answer here :(

    if (view_) {
      view_->OnTemporaryTextMaybeChanged(
          MaybeStripKeyword(temporary_text), current_match_,
          save_original_selection && original_user_text_with_keyword_.empty(),
          true);
    }
    return;
  }

  inline_autocompletion_ = inline_autocompletion;
  prefix_autocompletion_ = prefix_autocompletion;
  if (inline_autocompletion_.empty() && prefix_autocompletion_.empty()) {
    if (view_) {
      view_->OnInlineAutocompleteTextCleared();
    }
  }

  const std::u16string& user_text =
      user_input_in_progress_ ? user_text_ : input_.text();
  if (keyword_state_changed && is_keyword_selected() &&
      inline_autocompletion_.empty() && prefix_autocompletion_.empty()) {
    // If we reach here, the user most likely entered keyword mode by inserting
    // a space between a keyword name and a search string (as pressing space or
    // tab after the keyword name alone would have been be handled in
    // MaybeAcceptKeywordBySpace() by calling AcceptKeyword(), which won't reach
    // here).  In this case, we don't want to call
    // OnInlineAutocompleteTextMaybeChanged() as normal, because that will
    // correctly change the text (to the search string alone) but move the caret
    // to the end of the string; instead we want the caret at the start of the
    // search string since that's where it was in the original input.  So we set
    // the text and caret position directly.
    //
    // It may also be possible to reach here if we're reverting from having
    // temporary text back to a default match that's a keyword search, but in
    // that case the RevertTemporaryTextAndPopup() call below will reset the
    // caret or selection correctly so the caret positioning we do here won't
    // matter.
    if (view_) {
      view_->SetWindowTextAndCaretPos(user_text, 0, false, true);
    }
  } else {
    std::u16string display_text;
    std::vector<gfx::Range> selections = {};
    display_text = prefix_autocompletion_ + user_text + inline_autocompletion_;
    selections.emplace_back(
        display_text.size(),
        user_text.length() + prefix_autocompletion_.length());
    if (prefix_autocompletion_.length()) {
      selections.emplace_back(0, prefix_autocompletion_.length());
    }
    if (view_) {
      view_->OnInlineAutocompleteTextMaybeChanged(display_text, selections,
                                                  prefix_autocompletion_,
                                                  inline_autocompletion_);
      view_->SetAdditionalText(additional_text);
    }
  }
  // We need to invoke OnChanged in case the destination url changed (as could
  // happen when control is toggled).
  OnChanged();
}

bool OmniboxEditModel::OnAfterPossibleChange(
    const OmniboxView::StateChanges& state_changes,
    bool allow_keyword_ui_change) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (paste_state_ == PASTING) {
    paste_state_ = PASTED;

    GURL url = GURL(*(state_changes.new_text));
    if (url.is_valid()) {
      controller_->client()->OnUserPastedInOmniboxResultingInValidURL();
    }
  } else if (state_changes.text_differs)
    paste_state_ = NONE;

  if (state_changes.text_differs || state_changes.selection_differs) {
    // Record current focus state for this input if we haven't already.
    if (focus_source_ == OmniboxFocusSource::INVALID) {
      // We should generally expect the omnibox to have focus at this point, but
      // it doesn't always on Linux. This is because, unlike other platforms,
      // right clicking in the omnibox on Linux doesn't focus it. So pasting via
      // right-click can change the contents without focusing the omnibox.
      // TODO(samarth): fix Linux focus behavior and add a DCHECK here to
      // check that the omnibox does have focus.
      focus_source_ = (focus_state_ == OMNIBOX_FOCUS_INVISIBLE)
                          ? OmniboxFocusSource::FAKEBOX
                          : OmniboxFocusSource::OMNIBOX;
    }

    // Restore caret visibility whenever the user changes text or selection in
    // the omnibox.
    SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_TYPING);
  }

  // When the user performs an action with the ctrl key pressed down, we assume
  // the ctrl key was intended for that action. If they then press enter without
  // releasing the ctrl key, we prevent "ctrl-enter" behavior.
  ConsumeCtrlKey();

  // If the user text does not need to be changed, return now, so we don't
  // change any other state, lest arrowing around the omnibox do something like
  // reset |just_deleted_text_|.  Note that modifying the selection accepts any
  // inline autocompletion, which results in a user text change.
  if (!state_changes.text_differs &&
      (!state_changes.selection_differs ||
       (inline_autocompletion_.empty() && prefix_autocompletion_.empty()))) {
    if (state_changes.keyword_differs && view_) {
      // We won't need the below logic for creating a keyword by a space at the
      // end or in the middle, or by typing a '?', but we do need to update the
      // popup view because the keyword can change without the text changing,
      // for example when the keyword is "youtube.com" and the user presses
      // ctrl-k to change it to "google.com", or if the user text is empty and
      // the user presses backspace.
      view_->UpdatePopup();
    }
    return state_changes.keyword_differs;
  }

  InternalSetUserText(*state_changes.new_text);
  has_temporary_text_ = false;
  just_deleted_text_ = state_changes.just_deleted_text;

  const bool no_selection =
      state_changes.new_sel_start == state_changes.new_sel_end;

  // Update the popup for the change, in the process changing to keyword mode
  // if the user hit space in mid-string after a keyword.
  // |allow_exact_keyword_match_| will be used by StartAutocomplete() method,
  // which will be called by |view_->UpdatePopup()|; so after that returns we
  // can safely reset this flag.
  // If entering keyword mode by space is disabled, do not set
  // |allow_exact_keyword_match_|.
  allow_exact_keyword_match_ =
      AllowKeywordSpaceTriggering() && state_changes.text_differs &&
      allow_keyword_ui_change && !state_changes.just_deleted_text &&
      no_selection &&
      CreatedKeywordSearchByInsertingSpaceInMiddle(
          *state_changes.old_text, user_text_, state_changes.new_sel_start);
  if (view_) {
    view_->UpdatePopup();
  }
  if (allow_exact_keyword_match_) {
    keyword_mode_entry_method_ = OmniboxEventProto::SPACE_IN_MIDDLE;
    const TemplateURL* turl = controller_->client()
                                  ->GetTemplateURLService()
                                  ->GetTemplateURLForKeyword(keyword_);
    EmitEnteredKeywordModeHistogram(OmniboxEventProto::SPACE_IN_MIDDLE, turl);
    allow_exact_keyword_match_ = false;
  }

  if (!state_changes.text_differs || !allow_keyword_ui_change ||
      (state_changes.just_deleted_text && no_selection) ||
      is_keyword_selected() || (paste_state_ != NONE))
    return true;

  // If the user input a "?" at the beginning of the text, put them into
  // keyword mode for their default search provider.
  if ((state_changes.new_sel_start == 1) && (user_text_[0] == '?')) {
    EnterKeywordModeForDefaultSearchProvider(OmniboxEventProto::QUESTION_MARK);
    return false;
  }

  // Change to keyword mode if the user is now pressing space after a keyword
  // name.  Note that if this is the case, then even if there was no keyword
  // hint when we entered this function (e.g. if the user has used space to
  // replace some selected text that was adjoined to this keyword), there will
  // be one now because of the call to UpdatePopup() above; so it's safe for
  // MaybeAcceptKeywordBySpace() to look at |keyword_| and |is_keyword_hint_|
  // to determine what keyword, if any, is applicable.
  //
  // If MaybeAcceptKeywordBySpace() accepts the keyword and returns true, that
  // will have updated our state already, so in that case we don't also return
  // true from this function.
  return (state_changes.new_sel_start != user_text_.length()) ||
         !MaybeAcceptKeywordBySpace(user_text_);
}

// TODO(beaudoin): Merge OnPopupDataChanged with this method once the popup
// handling has completely migrated to omnibox_controller.
void OmniboxEditModel::OnCurrentMatchChanged() {
  has_temporary_text_ = false;

  DCHECK(controller_->result().default_match());
  const AutocompleteMatch& match = *controller_->result().default_match();

  // We store |keyword| and |is_keyword_hint| in temporary variables since
  // OnPopupDataChanged use their previous state to detect changes.
  std::u16string keyword;
  bool is_keyword_hint;
  TemplateURLService* service = controller_->client()->GetTemplateURLService();
  match.GetKeywordUIState(service, &keyword, &is_keyword_hint);
  OnPopupResultChanged();

  if (!is_keyword_selected() && !is_keyword_hint && !keyword.empty()) {
    // We just entered keyword mode, so remove the keyword from the input.
    // We don't call MaybeStripKeyword, as we haven't yet updated our internal
    // state (keyword_ and is_keyword_hint_), and MaybeStripKeyword checks this.
    user_text_ =
        KeywordProvider::SplitReplacementStringFromInput(user_text_, false);
    original_user_text_with_keyword_.clear();
  }

  // OnPopupDataChanged() resets OmniboxController's |current_match_| early
  // on.  Therefore, copy match.inline_autocompletion to a temp to preserve
  // its value across the entire call.
  OnPopupDataChanged(std::u16string(),
                     /*is_temporary_text=*/false, match.inline_autocompletion,
                     match.prefix_autocompletion, keyword, is_keyword_hint,
                     match.additional_text, match);
}

// static
const char OmniboxEditModel::kCutOrCopyAllTextHistogram[] =
    "Omnibox.CutOrCopyAllText";

void OmniboxEditModel::SetAccessibilityLabel(const AutocompleteMatch& match) {
  if (view_) {
    view_->SetAccessibilityLabel(view_->GetText(), match, true);
  }
}

void OmniboxEditModel::InternalSetUserText(const std::u16string& text) {
  user_text_ = text;
  original_user_text_with_keyword_.clear();
  just_deleted_text_ = false;
  inline_autocompletion_.clear();
  prefix_autocompletion_.clear();
  if (view_) {
    view_->OnInlineAutocompleteTextCleared();
  }
}

std::u16string OmniboxEditModel::MaybeStripKeyword(
    const std::u16string& text) const {
  return is_keyword_selected()
             ? KeywordProvider::SplitReplacementStringFromInput(text, false)
             : text;
}

std::u16string OmniboxEditModel::MaybePrependKeyword(
    const std::u16string& text) const {
  return is_keyword_selected() ? (keyword_ + u' ' + text) : text;
}

void OmniboxEditModel::GetInfoForCurrentText(AutocompleteMatch* match,
                                             GURL* alternate_nav_url) const {
  DCHECK(match);

  // If there's a query in progress or the popup is open, pick out the default
  // match or selected match, if there is one.
  bool found_match_for_text = false;
  if (controller_->query_in_progress() || PopupIsOpen()) {
    if (controller_->query_in_progress() &&
        controller_->result().default_match()) {
      // The user cannot have manually selected a match, or the query would have
      // stopped. So the default match must be the desired selection.
      *match = *controller_->result().default_match();
      found_match_for_text = true;
    } else if (PopupIsOpen() &&
               GetPopupSelection().line != OmniboxPopupSelection::kNoMatch) {
      const OmniboxPopupSelection selection = GetPopupSelection();
      const AutocompleteMatch& selected_match =
          controller_->result().match_at(selection.line);
      *match = (selection.state == OmniboxPopupSelection::KEYWORD_MODE)
                   ? *selected_match.associated_keyword
                   : selected_match;
      found_match_for_text = true;
    }
    if (found_match_for_text && alternate_nav_url &&
        (!popup_view_ || IsPopupSelectionOnInitialLine())) {
      AutocompleteProviderClient* provider_client =
          controller_->autocomplete_controller()
              ->autocomplete_provider_client();
      *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
          input_, *match, provider_client);
    }
  }

  if (!found_match_for_text) {
    // For match generation, we use the unelided |url_for_editing_|, unless the
    // user input is in progress.
    std::u16string text_for_match_generation =
        user_input_in_progress() ? user_text_ : url_for_editing_;

    controller_->client()->GetAutocompleteClassifier()->Classify(
        MaybePrependKeyword(text_for_match_generation), is_keyword_selected(),
        true, GetPageClassification(), match, alternate_nav_url);
  }
}

void OmniboxEditModel::RevertTemporaryTextAndPopup() {
  // The user typed something, then selected a different item.  Restore the
  // text they typed and change back to the default item.
  // NOTE: This purposefully does not reset paste_state_.
  just_deleted_text_ = false;
  has_temporary_text_ = false;

  ResetPopupToInitialState();

  // There are two cases in which resetting to the default match doesn't restore
  // the proper original text:
  //  1. If user input is not in progress, we are reverting an on-focus
  //     suggestion. These may be unrelated to the original input.
  //  2. If there's no default match at all.
  //
  // The original selection will be restored in OnRevertTemporaryText() below.
  if ((!user_input_in_progress_ || !controller_->result().default_match()) &&
      view_) {
    view_->SetWindowTextAndCaretPos(input_.text(), /*caret_pos=*/0,
                                    /*update_popup=*/false,
                                    /*notify_text_changed=*/true);
  }

  if (view_) {
    const AutocompleteMatch& match = CurrentMatch(nullptr);
    view_->OnRevertTemporaryText(match.fill_into_edit, match);
  }
}

bool OmniboxEditModel::ShouldPreventElision() const {
  return controller_->client()->GetLocationBarModel()->ShouldPreventElision();
}

bool OmniboxEditModel::IsStarredMatch(const AutocompleteMatch& match) const {
  auto* bookmark_model = controller_->client()->GetBookmarkModel();
  return bookmark_model && bookmark_model->IsBookmarked(match.destination_url);
}

// Android and iOS have their own platform-specific icon logic.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
gfx::Image OmniboxEditModel::GetMatchIcon(const AutocompleteMatch& match,
                                          SkColor vector_icon_color) const {
  gfx::Image extension_icon =
      controller_->client()->GetIconIfExtensionMatch(match);
  // Extension icons are the correct size for non-touch UI but need to be
  // adjusted to be the correct size for touch mode.
  if (!extension_icon.IsEmpty()) {
    return controller_->client()->GetSizedIcon(extension_icon);
  }

  // Get the favicon for navigational suggestions.
  //
  // The starter pack suggestions are a unique case. These suggestions
  // normally use a favicon image that cannot be styled further by client
  // code. In order to apply custom styling to the icon (e.g. colors), we ignore
  // this favicon in favor of using a vector icon which has better styling
  // support.
  if (!AutocompleteMatch::IsSearchType(match.type) &&
      match.type != AutocompleteMatchType::DOCUMENT_SUGGESTION &&
      match.type != AutocompleteMatchType::HISTORY_CLUSTER &&
      !AutocompleteMatch::IsStarterPackType(match.type)) {
    // Because the Views UI code calls GetMatchIcon in both the layout and
    // painting code, we may generate multiple `OnFaviconFetched` callbacks,
    // all run one after another. This seems to be harmless as the callback
    // just flips a flag to schedule a repaint. However, if it turns out to be
    // costly, we can optimize away the redundant extra callbacks.
    gfx::Image favicon = controller_->client()->GetFaviconForPageUrl(
        match.destination_url,
        base::BindOnce(&OmniboxEditModel::OnFaviconFetched,
                       weak_factory_.GetWeakPtr(), match.destination_url));

    // Extension icons are the correct size for non-touch UI but need to be
    // adjusted to be the correct size for touch mode.
    if (!favicon.IsEmpty()) {
      return controller_->client()->GetSizedIcon(favicon);
    }
  }

  bool is_starred_match = IsStarredMatch(match);
  const TemplateURL* turl =
      match.associated_keyword
          ? controller_->client()
                ->GetTemplateURLService()
                ->GetTemplateURLForKeyword(match.associated_keyword->keyword)
          : nullptr;
  const auto& vector_icon_type = match.GetVectorIcon(is_starred_match, turl);

  return controller_->client()->GetSizedIcon(vector_icon_type,
                                             vector_icon_color);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

bool OmniboxEditModel::PopupIsOpen() const {
  return popup_view_ && popup_view_->IsOpen();
}

void OmniboxEditModel::ResetPopupToInitialState() {
  if (!popup_view_) {
    return;
  }
  size_t new_line = controller_->result().default_match()
                        ? 0
                        : OmniboxPopupSelection::kNoMatch;
  SetPopupSelection(
      OmniboxPopupSelection(new_line, OmniboxPopupSelection::NORMAL),
      /*reset_to_default=*/true);
  get_popup_view()->OnDragCanceled();
}

OmniboxPopupSelection OmniboxEditModel::GetPopupSelection() const {
  DCHECK(popup_view_);
  return popup_selection_;
}

void OmniboxEditModel::SetPopupSelection(OmniboxPopupSelection new_selection,
                                         bool reset_to_default,
                                         bool force_update_ui) {
  DCHECK(popup_view_);

  if (controller_->result().empty()) {
    return;
  }

  // Cancel the query so the matches don't change on the user.
  controller_->autocomplete_controller()->Stop(false);

  if (new_selection == popup_selection_ && !force_update_ui) {
    // This occurs when e.g. pressing tab to select an action chip or the x
    // delete icon. Nothing else to do.
    return;
  }

  // We need to update selection before notifying any views, as they will query
  // `popup_selection_` to update themselves.
  const OmniboxPopupSelection old_selection = popup_selection_;
  popup_selection_ = new_selection;
  popup_view_->OnSelectionChanged(old_selection, popup_selection_);

  // This occurs when e.g., pressing escape to select the null match in
  // zero-input mode.
  if (popup_selection_.line == OmniboxPopupSelection::kNoMatch) {
    current_match_ = AutocompleteMatch();
    return;
  }

  const AutocompleteMatch& match =
      controller_->result().match_at(popup_selection_.line);

  DCHECK((popup_selection_.state != OmniboxPopupSelection::KEYWORD_MODE) ||
         match.associated_keyword.get());
  if (popup_selection_.IsButtonFocused()) {
    old_focused_url_ = match.destination_url;
    SetAccessibilityLabel(match);
    // TODO(tommycli): Fold the focus hint into
    // popup_view_->OnSelectionChanged(). Caveat: We must update the
    // accessibility label before notifying the View.
    popup_view_->ProvideButtonFocusHint(GetPopupSelection().line);
  }

  std::u16string keyword;
  bool is_keyword_hint;
  TemplateURLService* service = controller_->client()->GetTemplateURLService();
  match.GetKeywordUIState(service, &keyword, &is_keyword_hint);

  if (popup_selection_.state == OmniboxPopupSelection::FOCUSED_BUTTON_HEADER) {
    // If the new selection is a Header, the temporary text is an empty string.
    OnPopupDataChanged(std::u16string(),
                       /*is_temporary_text=*/true, std::u16string(),
                       std::u16string(), keyword, is_keyword_hint,
                       std::u16string(), AutocompleteMatch());
  } else if (old_selection.line != popup_selection_.line ||
             (old_selection.IsButtonFocused() &&
              !new_selection.IsButtonFocused() &&
              new_selection.state != OmniboxPopupSelection::KEYWORD_MODE)) {
    // Otherwise, only update the edit model for line number changes, or
    // when the old selection was a button and we're not entering keyword mode.
    // Updating the edit model for every state change breaks keyword mode.
    if (reset_to_default) {
      OnPopupDataChanged(std::u16string(),
                         /*is_temporary_text=*/false,
                         match.inline_autocompletion,
                         match.prefix_autocompletion, keyword, is_keyword_hint,
                         match.additional_text, match);
    } else {
      OnPopupDataChanged(match.fill_into_edit,
                         /*is_temporary_text=*/true, std::u16string(),
                         std::u16string(), keyword, is_keyword_hint,
                         std::u16string(), match);
    }
  }
  // Without this, focus indicators may appear stale (see crbug.com/1369229).
  popup_view_->UpdatePopupAppearance();
}

bool OmniboxEditModel::IsPopupSelectionOnInitialLine() const {
  DCHECK(popup_view_);
  size_t initial_line = controller_->result().default_match()
                            ? 0
                            : OmniboxPopupSelection::kNoMatch;
  return GetPopupSelection().line == initial_line;
}

bool OmniboxEditModel::IsPopupControlPresentOnMatch(
    OmniboxPopupSelection selection) const {
  DCHECK(popup_view_);
  return selection.IsControlPresentOnMatch(controller_->result(),
                                           GetPrefService());
}

void OmniboxEditModel::TryDeletingPopupLine(size_t line) {
  DCHECK(popup_view_);

  // When called with line == GetPopupSelection().line, we could use
  // GetInfoForCurrentText() here, but it seems better to try and delete the
  // actual selection, rather than any "in progress, not yet visible" one.
  if (line == OmniboxPopupSelection::kNoMatch) {
    return;
  }

  // Cancel the query so the matches don't change on the user.
  controller_->autocomplete_controller()->Stop(false);

  const AutocompleteMatch& match = controller_->result().match_at(line);
  if (match.SupportsDeletion()) {
    // Try to preserve the selection even after match deletion.
    size_t old_selected_line = GetPopupSelection().line;

    // This will synchronously notify both the edit and us that the results
    // have changed, causing both to revert to the default match.
    controller_->autocomplete_controller()->DeleteMatch(match);

    // Clamp the old selection to the new size of controller_->result(),
    // since there may be fewer results now.
    if (old_selected_line != OmniboxPopupSelection::kNoMatch) {
      old_selected_line = std::min(line, controller_->result().size() - 1);
    }

    // Move the selection to the next choice after the deleted one.
    // SetPopupSelection() will clamp to take care of the case where we deleted
    // the last item.
    // TODO(pkasting): Eventually the controller should take care of this
    // before notifying us, reducing flicker.  At that point the check for
    // deletability can move there too.
    SetPopupSelection(
        OmniboxPopupSelection(old_selected_line, OmniboxPopupSelection::NORMAL),
        false, true);
  }
}

std::u16string OmniboxEditModel::GetPopupAccessibilityLabelForCurrentSelection(
    const std::u16string& match_text,
    bool include_positional_info,
    int* label_prefix_length) {
  DCHECK(popup_view_);

  size_t line = popup_selection_.line;
  DCHECK_NE(line, OmniboxPopupSelection::kNoMatch)
      << "GetPopupAccessibilityLabelForCurrentSelection should never be called "
         "if the current selection is kNoMatch.";

  const AutocompleteMatch& match = controller_->result().match_at(line);

  int additional_message_id = 0;
  std::u16string additional_message;
  // This switch statement should be updated when new selection types are added.
  static_assert(OmniboxPopupSelection::LINE_STATE_MAX_VALUE == 5);
  switch (popup_selection_.state) {
    case OmniboxPopupSelection::FOCUSED_BUTTON_HEADER: {
      bool group_hidden = controller_->IsSuggestionGroupHidden(
          match.suggestion_group_id.value());
      int message_id = group_hidden ? IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON
                                    : IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON;
      return l10n_util::GetStringFUTF16(
          message_id, controller_->GetHeaderForSuggestionGroup(
                          match.suggestion_group_id.value()));
    }
    case OmniboxPopupSelection::NORMAL: {
      int available_actions_count = 0;
      if (OmniboxPopupSelection(line, OmniboxPopupSelection::KEYWORD_MODE)
              .IsControlPresentOnMatch(controller_->result(),
                                       GetPrefService())) {
        additional_message_id = IDS_ACC_KEYWORD_SUFFIX;
        available_actions_count++;
      }
      if (OmniboxPopupSelection(line,
                                OmniboxPopupSelection::FOCUSED_BUTTON_ACTION)
              .IsControlPresentOnMatch(controller_->result(),
                                       GetPrefService())) {
        additional_message =
            match.GetActionAt(0u)->GetLabelStrings().accessibility_suffix;
        available_actions_count++;
      }
      if (OmniboxPopupSelection(
              line, OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION)
              .IsControlPresentOnMatch(controller_->result(),
                                       GetPrefService())) {
        additional_message_id = IDS_ACC_REMOVE_SUGGESTION_SUFFIX;
        available_actions_count++;
      }
      if (available_actions_count > 1)
        additional_message_id = IDS_ACC_MULTIPLE_ACTIONS_SUFFIX;

      break;
    }
    case OmniboxPopupSelection::KEYWORD_MODE: {
      // In keyword mode, the match we're interested in is actually the
      // associated_keyword of the match we're on. Populate the a11y string
      // with information from the keyword match, rather than the current match.
      CHECK(match.associated_keyword) << match.keyword;
      TemplateURL* turl = match.associated_keyword->GetTemplateURL(
          controller_->client()->GetTemplateURLService(), false);
      std::u16string replacement_string =
          turl ? turl->short_name() : match.contents;
      return l10n_util::GetStringFUTF16(IDS_ACC_KEYWORD_MODE,
                                        replacement_string);
    }
    case OmniboxPopupSelection::FOCUSED_BUTTON_ACTION:
      // When pedal button is focused, the autocomplete suggestion isn't
      // read because it's not relevant to the button's action.
      DCHECK(match.GetActionAt(0u));
      return match.GetActionAt(0u)->GetLabelStrings().accessibility_hint;
    case OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      additional_message_id = IDS_ACC_REMOVE_SUGGESTION_FOCUSED_PREFIX;
      break;
    default:
      break;
  }
  if (additional_message_id != 0 && additional_message.empty()) {
    additional_message = l10n_util::GetStringUTF16(additional_message_id);
  }

  if (popup_selection_.IsButtonFocused()) {
    include_positional_info = false;
  }

  size_t total_matches =
      include_positional_info ? controller_->result().size() : 0;

  // If there's a button focused, we don't want the "n of m" message announced.
  return AutocompleteMatchType::ToAccessibilityLabel(
      match, match_text, line, total_matches, additional_message,
      label_prefix_length);
}

void OmniboxEditModel::OnPopupResultChanged() {
  if (popup_view_) {
    rich_suggestion_bitmaps_.clear();
    const AutocompleteResult& result = controller_->result();
    size_t old_selected_line = GetPopupSelection().line;

    if (result.default_match()) {
      OmniboxPopupSelection selection = GetPopupSelection();
      selection.line = 0;

      const bool has_focused_match =
          selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_ACTION &&
          result.match_at(selection.line).has_tab_match.value_or(false);
      const bool has_changed =
          selection.line != old_selected_line ||
          result.match_at(selection.line).destination_url != old_focused_url_;

      if (!has_focused_match || has_changed) {
        selection.state = OmniboxPopupSelection::NORMAL;
      }
      popup_selection_ = selection;
    } else {
      popup_selection_ = OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                               OmniboxPopupSelection::NORMAL);
    }

    bool popup_was_open = popup_view_->IsOpen();
    popup_view_->UpdatePopupAppearance();
    if (popup_view_->IsOpen() != popup_was_open) {
      controller_->client()->OnPopupVisibilityChanged();
    }
  }
}

const SkBitmap* OmniboxEditModel::GetPopupRichSuggestionBitmap(
    int result_index) const {
  DCHECK(popup_view_);

  const auto iter = rich_suggestion_bitmaps_.find(result_index);
  if (iter == rich_suggestion_bitmaps_.end()) {
    return nullptr;
  }
  return &iter->second;
}

void OmniboxEditModel::SetPopupRichSuggestionBitmap(int result_index,
                                                    const SkBitmap& bitmap) {
  DCHECK(popup_view_);
  rich_suggestion_bitmaps_[result_index] = bitmap;
  popup_view_->UpdatePopupAppearance();
}

void OmniboxEditModel::SetPopupSuggestionGroupVisibility(
    size_t match_index,
    bool suggestion_group_hidden) {
  if (PopupIsOpen()) {
    popup_view_->SetSuggestionGroupVisibility(match_index,
                                              suggestion_group_hidden);
  }
}

PrefService* OmniboxEditModel::GetPrefService() const {
  return controller_->client()->GetPrefs();
}

bool OmniboxEditModel::MaybeStartQueryForPopup() {
  if (PopupIsOpen() || controller_->query_in_progress()) {
    return false;
  }

  // The popup is neither open nor working on a query already. So, start an
  // autocomplete query for the current text. This also sets
  // `user_input_in_progress_` to true, which we want: if the user has started
  // to interact with the popup, changing the `url_for_editing_` shouldn't
  // change the displayed text.
  // Note: This does not force the popup to open immediately.
  // TODO(pkasting): We should, in fact, force this particular query to open
  //   the popup immediately.
  if (!user_input_in_progress_)
    InternalSetUserText(url_for_editing_);
  if (view_) {
    view_->UpdatePopup();
  }
  return true;
}

void OmniboxEditModel::StepPopupSelection(
    OmniboxPopupSelection::Direction direction,
    OmniboxPopupSelection::Step step) {
  DCHECK(popup_view_);
  // This block steps the popup selection, with special consideration
  // for existing keyword logic in the edit model, where ClearKeyword must be
  // called before changing the selected line.
  // AcceptKeyword should be called after changing the selected line so we don't
  // accept keyword on the wrong suggestion when stepping backwards.
  const OmniboxPopupSelection old_selection = GetPopupSelection();
  OmniboxPopupSelection new_selection = old_selection.GetNextSelection(
      controller_->result(), GetPrefService(),
      controller_->client()->GetTemplateURLService(), direction, step);
  if (OmniboxFieldTrial::IsKeywordModeRefreshEnabled()) {
    if (old_selection.IsChangeToKeyword(new_selection)) {
      ClearKeyword();
      SetPopupSelection(new_selection);
    } else if (new_selection.state ==
               OmniboxPopupSelection::LineState::KEYWORD_MODE) {
      // Prepare for keyword mode before accepting it.
      SetPopupSelection(OmniboxPopupSelection(
          new_selection.line, OmniboxPopupSelection::LineState::NORMAL));
      // Note: Popup behavior currently depends on the entry method being tab.
      // This is not ideal for nuanced metrics, but it is how it has worked
      // for a long time. Consider refactoring to fix this if needed.
      AcceptKeyword(metrics::OmniboxEventProto::TAB);
    } else {
      SetPopupSelection(new_selection);
    }
  } else {
    if (old_selection.IsChangeToKeyword(new_selection)) {
      ClearKeyword();
    }
    SetPopupSelection(new_selection);
    if (new_selection.IsChangeToKeyword(old_selection)) {
      AcceptKeyword(metrics::OmniboxEventProto::TAB);
    }
  }
}

void OmniboxEditModel::AcceptInput(WindowOpenDisposition disposition,
                                   base::TimeTicks match_selection_timestamp) {
  // Get the URL and transition type for the selected entry.
  GURL alternate_nav_url;
  AutocompleteMatch match = CurrentMatch(&alternate_nav_url);

  // If CTRL is down it means the user wants to append ".com" to the text they
  // typed. If we can successfully generate a URL_WHAT_YOU_TYPED match doing
  // that, then we use this. These matches are marked as generated by the
  // HistoryURLProvider so we only generate them if this provider is present.
  bool accept_via_control_enter =
      control_key_state_ == DOWN && !is_keyword_selected() &&
      controller_->autocomplete_controller()->history_url_provider();
  base::UmaHistogramBoolean("Omnibox.Search.CtrlEnter.Used",
                            accept_via_control_enter);
  if (accept_via_control_enter) {
    // For generating the hostname of the URL, we use the most recent
    // input instead of the currently visible text. This means we'll ignore any
    // visible inline autocompletion: if a user types "foo" and is autocompleted
    // to "foodnetwork.com", ctrl-enter will navigate to "foo.com", not
    // "foodnetwork.com".  At the time of writing, this behavior matches
    // Internet Explorer, but not Firefox. Two exceptions to our own rule:
    //  1. If the user has selected a suggestion, use the suggestion text.
    //  2. If the user has never edited the text, use the current page's full
    //     URL instead of the elided URL to avoid HTTPS downgrading.
    std::u16string text_for_desired_tld_navigation = input_.text();
    if (has_temporary_text_) {
      text_for_desired_tld_navigation = GetText();
    } else if (!user_input_in_progress()) {
      text_for_desired_tld_navigation = url_for_editing_;
    }

    // Generate a new AutocompleteInput, copying the latest one but using "com"
    // as the desired TLD. Then use this autocomplete input to generate a
    // URL_WHAT_YOU_TYPED AutocompleteMatch.
    AutocompleteInput input(
        text_for_desired_tld_navigation, input_.cursor_position(), "com",
        input_.current_page_classification(),
        controller_->client()->GetSchemeClassifier(),
        controller_->client()->ShouldDefaultTypedNavigationsToHttps(), 0,
        false);
    input.set_prevent_inline_autocomplete(input_.prevent_inline_autocomplete());
    input.set_prefer_keyword(input_.prefer_keyword());
    input.set_keyword_mode_entry_method(input_.keyword_mode_entry_method());
    input.set_allow_exact_keyword_match(input_.allow_exact_keyword_match());
    input.set_omit_asynchronous_matches(input_.omit_asynchronous_matches());
    input.set_focus_type(input_.focus_type());
    input_ = input;
    AutocompleteMatch url_match(VerbatimMatchForInput(
        controller_->autocomplete_controller()->history_url_provider(),
        controller_->autocomplete_controller()->autocomplete_provider_client(),
        input_, input_.canonicalized_url(), false));

    base::UmaHistogramBoolean("Omnibox.Search.CtrlEnter.ResolvedAsUrl",
                              url_match.destination_url.is_valid());

    if (url_match.destination_url.is_valid()) {
      // We have a valid URL, we use this newly generated AutocompleteMatch.
      match = url_match;
      alternate_nav_url = GURL();
    }
  }

  if (!match.destination_url.is_valid()) {
    return;
  }

  if (ui::PageTransitionCoreTypeIs(match.transition,
                                   ui::PAGE_TRANSITION_TYPED) &&
      (match.destination_url ==
       controller_->client()->GetLocationBarModel()->GetURL())) {
    // When the user hit enter on the existing permanent URL, treat it like a
    // reload for scoring purposes.  We could detect this by just checking
    // user_input_in_progress_, but it seems better to treat "edits" that end
    // up leaving the URL unchanged (e.g. deleting the last character and then
    // retyping it) as reloads too.  We exclude non-TYPED transitions because if
    // the transition is GENERATED, the user input something that looked
    // different from the current URL, even if it wound up at the same place
    // (e.g. manually retyping the same search query), and it seems wrong to
    // treat this as a reload.
    match.transition = ui::PAGE_TRANSITION_RELOAD;
  } else if (paste_state_ != NONE &&
             match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED) {
    // When the user pasted in a URL and hit enter, score it like a link click
    // rather than a normal typed URL, so it doesn't get inline autocompleted
    // as aggressively later.
    match.transition = ui::PAGE_TRANSITION_LINK;
  }

  if (popup_view_) {
    OpenMatch(OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch), match,
              disposition, alternate_nav_url, std::u16string(),
              match_selection_timestamp);
  }
}

void OmniboxEditModel::OpenMatch(OmniboxPopupSelection selection,
                                 AutocompleteMatch match,
                                 WindowOpenDisposition disposition,
                                 const GURL& alternate_nav_url,
                                 const std::u16string& pasted_text,
                                 base::TimeTicks match_selection_timestamp) {
  // If the user is executing an action, this will be non-null and some match
  // opening and metrics behavior will be adjusted accordingly.
  OmniboxAction* action = nullptr;
  if (selection.state == OmniboxPopupSelection::NORMAL &&
      match.takeover_action) {
    DCHECK(match_selection_timestamp != base::TimeTicks());
    action = match.takeover_action.get();
  } else if (selection.IsAction()) {
    DCHECK_LT(selection.action_index, match.actions.size());
    action = match.actions[selection.action_index].get();
  }

  // Invalid URLs such as chrome://history can end up here, but that's okay
  // if the user is executing an action instead of navigating to the URL.
  if (!match.destination_url.is_valid() && !action) {
    return;
  }

  // NULL_RESULT_MESSAGE matches are informational only and cannot be acted
  // upon. Immediately return when attempting to open one.
  if (match.type == AutocompleteMatchType::NULL_RESULT_MESSAGE) {
    return;
  }

  // Switch the window disposition to SWITCH_TO_TAB for open tab matches that
  // originated while in keyword mode.  This is to support the keyword mode
  // starter pack's tab search (@tabs) feature, which should open all
  // suggestions in the existing open tab.
  bool is_open_tab_match =
      match.from_keyword &&
      match.provider->type() == AutocompleteProvider::TYPE_OPEN_TAB;
  if (is_open_tab_match) {
    disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  }

  TRACE_EVENT("omnibox", "OmniboxEditModel::OpenMatch", "match", match,
              "disposition", disposition, "altenate_nav_url", alternate_nav_url,
              "pasted_text", pasted_text);
  const base::TimeTicks& now(base::TimeTicks::Now());
  base::TimeDelta elapsed_time_since_user_first_modified_omnibox(
      now - time_user_first_modified_omnibox_);
  controller_->autocomplete_controller()
      ->UpdateMatchDestinationURLWithAdditionalAssistedQueryStats(
          elapsed_time_since_user_first_modified_omnibox, &match);

  GURL destination_url = action ? action->getUrl() : match.destination_url;

  // Save the result of the interaction, but do not record the histogram yet.
  focus_resulted_in_navigation_ = true;

  RecordActionShownForAllActions(controller_->result(), selection);
  HistoryFuzzyProvider::RecordOpenMatchMetrics(controller_->result(), match);

  std::u16string input_text(pasted_text);
  if (input_text.empty()) {
    input_text = user_input_in_progress_ ? user_text_ : url_for_editing_;
  }
  // Create a dummy AutocompleteInput for use in calling VerbatimMatchForInput()
  // to create an alternate navigational match.
  AutocompleteInput alternate_input(
      input_text, GetPageClassification(),
      controller_->client()->GetSchemeClassifier(),
      controller_->client()->ShouldDefaultTypedNavigationsToHttps(), 0, false);
  // Somehow we can occasionally get here with no active tab.  It's not
  // clear why this happens.
  alternate_input.set_current_url(controller_->client()->GetURL());
  alternate_input.set_current_title(controller_->client()->GetTitle());

  base::TimeDelta elapsed_time_since_last_change_to_default_match(
      now - controller_->autocomplete_controller()
                ->last_time_default_match_changed());
  DCHECK(match.provider);
  // These elapsed times don't really make sense for matches that come from
  // omnibox focus (because the user did not modify the omnibox), so for those
  // we set the elapsed times to something that will be ignored by
  // metrics_log.cc.  They also don't necessarily make sense if the omnibox
  // dropdown is closed or the user used paste-and-go.  (In most
  // cases when this happens, the user never modified the omnibox.)
  const bool popup_open = PopupIsOpen();
  if (input_.focus_type() != metrics::OmniboxFocusType::INTERACTION_DEFAULT ||
      !pasted_text.empty()) {
    const base::TimeDelta default_time_delta = base::Milliseconds(-1);
    elapsed_time_since_user_first_modified_omnibox = default_time_delta;
    elapsed_time_since_last_change_to_default_match = default_time_delta;
  }

  // In some unusual cases, we ignore controller_->result() and instead
  // log a fake result set with a single element (|match|) and selected_index of
  // 0. For these cases:
  //  1. If the popup is closed (there is no result set).
  //  2. If the index is out of bounds. This should only happen if
  //  |selection.line| is
  //     kNoMatch, which can happen if the default search provider is disabled.
  //  3. If this is paste-and-go (meaning the contents of the dropdown
  //     are ignored regardless).
  const bool dropdown_ignored =
      !popup_open || selection.line >= controller_->result().size() ||
      !pasted_text.empty();
  ACMatches fake_single_entry_matches;
  fake_single_entry_matches.push_back(match);
  AutocompleteResult fake_single_entry_result;
  fake_single_entry_result.AppendMatches(fake_single_entry_matches);

  const std::u16string& user_text =
      input_.focus_type() != metrics::OmniboxFocusType::INTERACTION_DEFAULT
          ? std::u16string()
          : input_text;
  size_t completed_length = match.allowed_to_be_default_match
                                ? match.inline_autocompletion.length()
                                : std::u16string::npos;
  bool is_incognito = controller_->autocomplete_controller()
                          ->autocomplete_provider_client()
                          ->IsOffTheRecord();
  OmniboxLog log(
      user_text, just_deleted_text_, input_.type(), is_keyword_selected(),
      keyword_mode_entry_method_, popup_open,
      dropdown_ignored ? OmniboxPopupSelection(0) : selection, disposition,
      !pasted_text.empty(),
      SessionID::InvalidValue(),  // don't know tab ID; set later if appropriate
      GetPageClassification(), elapsed_time_since_user_first_modified_omnibox,
      completed_length, elapsed_time_since_last_change_to_default_match,
      dropdown_ignored ? fake_single_entry_result : controller_->result(),
      destination_url, is_incognito);
  DCHECK(dropdown_ignored ||
         (log.elapsed_time_since_user_first_modified_omnibox >=
          log.elapsed_time_since_last_change_to_default_match))
      << "We should've got the notification that the user modified the "
      << "omnibox text at same time or before the most recent time the "
      << "default match changed.";

  if ((disposition == WindowOpenDisposition::CURRENT_TAB) &&
      controller_->client()->CurrentPageExists()) {
    // If we know the destination is being opened in the current tab,
    // we can easily get the tab ID.  (If it's being opened in a new
    // tab, we don't know the tab ID yet.)
    log.tab_id = controller_->client()->GetSessionID();
  }
  controller_->autocomplete_controller()->AddProviderAndTriggeringLogs(&log);

  base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.RichAutocompletion",
                                match.rich_autocompletion_triggered);
  size_t ipv4_parts_count =
      CountNumberOfIPv4Parts(user_text, destination_url, completed_length);
  // The histogram is collected to decide if shortened IPv4 addresses
  // like 127.1 should be deprecated.
  // Only valid IP addresses manually inputted by the user will be counted.
  if (ipv4_parts_count > 0) {
    base::UmaHistogramCounts100("Omnibox.IPv4AddressPartsCount",
                                ipv4_parts_count);
  }

  controller_->client()->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);
  SuggestionAnswer::LogAnswerUsed(match.answer);
  if (!last_omnibox_focus_.is_null()) {
    // Only record focus to open time when a focus actually happened (as
    // opposed to, say, dragging a link onto the omnibox).
    UMA_HISTOGRAM_MEDIUM_TIMES(kFocusToOpenTimeHistogram,
                               now - last_omnibox_focus_);
  }

  IDNA2008DeviationCharacter deviation_char_in_hostname =
      IDNA2008DeviationCharacter::kNone;
  TemplateURLService* service = controller_->client()->GetTemplateURLService();
  TemplateURL* template_url = match.GetTemplateURL(service, false);
  if (template_url) {
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_KEYWORD)) {
      // The user is using a non-substituting keyword or is explicitly in
      // keyword mode.

      // Don't increment usage count for extension keywords.
      if (controller_->client()->ProcessExtensionKeyword(
              input_text, template_url, match, disposition)) {
        if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB && view_) {
          base::AutoReset<bool> tmp(&in_revert_, true);
          view_->RevertAll();
        }
        return;
      }

      base::RecordAction(base::UserMetricsAction("AcceptedKeyword"));
      EmitAcceptedKeywordSuggestionHistogram(keyword_mode_entry_method_,
                                             template_url);
      controller_->client()->GetTemplateURLService()->IncrementUsageCount(
          template_url);
    } else {
      DCHECK(ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_GENERATED) ||
             ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_RELOAD));
      // NOTE: We purposefully don't increment the usage count of the default
      // search engine here like we do for explicit keywords above; see comments
      // in template_url.h.
    }

    AutocompleteMatch::LogSearchEngineUsed(match, service);
  } else {
    // |match| is a URL navigation, not a search.
    // For logging the below histogram, only record uses that depend on the
    // omnibox suggestion system, i.e., TYPED navigations.  That is, exclude
    // omnibox URL interactions that are treated as reloads or link-following
    // (i.e., cut-and-paste of URLs) or paste-and-go.
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_TYPED) &&
        pasted_text.empty()) {
      navigation_metrics::RecordOmniboxURLNavigation(destination_url);
    }

    // The following histograms should be recorded for both TYPED and pasted
    // URLs, but should still exclude reloads.
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_TYPED) ||
        ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                    ui::PAGE_TRANSITION_LINK)) {
      net::cookie_util::RecordCookiePortOmniboxHistograms(destination_url);

      if (destination_url.SchemeIsHTTPOrHTTPS()) {
        // Extract the typed hostname from autocomplete input for IDNA 2008
        // metrics. We can't use GURL here as it removes the deviation
        // characters that we want to measure.
        size_t hostname_begin = input_.parts().host.begin;
        if (input_.added_default_scheme_to_typed_url() && hostname_begin > 0) {
          // If the omnibox upgrades a navigation to https, it offsets
          // components by one to the right due to the added "s" to http. Adjust
          // the offset again. Ideally, hostname_begin should always be non-zero
          // in that case, but we check it for safety.
          --hostname_begin;
        }
        std::u16string hostname(input_.text(), hostname_begin,
                                static_cast<size_t>(input_.parts().host.len));
        deviation_char_in_hostname =
            navigation_metrics::RecordIDNA2008Metrics(hostname);
      }
    }
  }

  if (action) {
    OmniboxAction::ExecutionContext context(
        *(controller_->autocomplete_controller()
              ->autocomplete_provider_client()),
        base::BindOnce(&OmniboxClient::OnAutocompleteAccept,
                       controller_->client()->AsWeakPtr()),
        match_selection_timestamp, disposition);
    action->Execute(context);
  }

  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB && view_) {
    base::AutoReset<bool> tmp(&in_revert_, true);
    view_->RevertAll();  // Revert the box to its unedited state.
  }

  if (!action) {
    // Track whether the destination URL sends us to a search results page
    // using the default search provider.
    TemplateURLService* template_url_service =
        controller_->client()->GetTemplateURLService();
    if (template_url_service &&
        template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
            match.destination_url)) {
      base::RecordAction(
          base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
      base::UmaHistogramBoolean("Omnibox.Search.OffTheRecord", is_incognito);
    }

    if (destination_url.is_valid()) {
      // This calls RevertAll again.
      base::AutoReset<bool> tmp(&in_revert_, true);

      controller_->client()->OnAutocompleteAccept(
          destination_url, match.post_content.get(), disposition,
          ui::PageTransitionFromInt(match.transition |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          match.type, match_selection_timestamp,
          input_.added_default_scheme_to_typed_url(),
          input_.typed_url_had_http_scheme() &&
              match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED,
          input_text, match,
          VerbatimMatchForInput(
              controller_->autocomplete_controller()->history_url_provider(),
              controller_->autocomplete_controller()
                  ->autocomplete_provider_client(),
              alternate_input, alternate_nav_url, false),
          deviation_char_in_hostname);
    }

    BookmarkModel* bookmark_model = controller_->client()->GetBookmarkModel();
    if (bookmark_model && bookmark_model->IsBookmarked(destination_url)) {
      controller_->client()->OnBookmarkLaunched();
    }
  }
}

bool OmniboxEditModel::AllowKeywordSpaceTriggering() const {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  return GetPrefService()->GetBoolean(omnibox::kKeywordSpaceTriggeringEnabled);
#else
  return true;
#endif
}

bool OmniboxEditModel::MaybeAcceptKeywordBySpace(
    const std::u16string& new_text) {
  if (!AllowKeywordSpaceTriggering())
    return false;

  size_t keyword_length = new_text.length() - 1;
  return is_keyword_hint_ && (keyword_.length() == keyword_length) &&
         IsSpaceCharForAcceptingKeyword(new_text[keyword_length]) &&
         !new_text.compare(0, keyword_length, keyword_, 0, keyword_length) &&
         AcceptKeyword(OmniboxEventProto::SPACE_AT_END);
}

bool OmniboxEditModel::CreatedKeywordSearchByInsertingSpaceInMiddle(
    const std::u16string& old_text,
    const std::u16string& new_text,
    size_t caret_position) const {
  DCHECK_GE(new_text.length(), caret_position);

  // Check simple conditions first.
  if ((paste_state_ != NONE) || (caret_position < 2) ||
      (old_text.length() < caret_position) ||
      (new_text.length() == caret_position))
    return false;
  size_t space_position = caret_position - 1;
  if (!IsSpaceCharForAcceptingKeyword(new_text[space_position]) ||
      base::IsUnicodeWhitespace(new_text[space_position - 1]) ||
      new_text.compare(0, space_position, old_text, 0, space_position) ||
      !new_text.compare(space_position, new_text.length() - space_position,
                        old_text, space_position,
                        old_text.length() - space_position)) {
    return false;
  }

  // Then check if the text before the inserted space matches a keyword.
  std::u16string keyword;
  base::TrimWhitespace(new_text.substr(0, space_position), base::TRIM_LEADING,
                       &keyword);
  return !keyword.empty() && !controller_->autocomplete_controller()
                                  ->keyword_provider()
                                  ->GetKeywordForText(keyword)
                                  .empty();
}

//  static
bool OmniboxEditModel::IsSpaceCharForAcceptingKeyword(wchar_t c) {
  switch (c) {
    case 0x0020:  // Space
    case 0x3000:  // Ideographic Space
      return true;
    default:
      return false;
  }
}

void OmniboxEditModel::ClassifyString(const std::u16string& text,
                                      AutocompleteMatch* match,
                                      GURL* alternate_nav_url) const {
  DCHECK(match);
  controller_->client()->GetAutocompleteClassifier()->Classify(
      text, false, false, GetPageClassification(), match, alternate_nav_url);
}

bool OmniboxEditModel::SetInputInProgressNoNotify(bool in_progress) {
  if (in_progress && !user_input_since_focus_) {
    base::TimeTicks now = base::TimeTicks::Now();
    DCHECK(last_omnibox_focus_ <= now);
    UMA_HISTOGRAM_TIMES(kFocusToEditTimeHistogram, now - last_omnibox_focus_);
    user_input_since_focus_ = true;
  }

  if (user_input_in_progress_ == in_progress)
    return false;

  user_input_in_progress_ = in_progress;
  if (user_input_in_progress_) {
    time_user_first_modified_omnibox_ = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction("OmniboxInputInProgress"));
    controller_->autocomplete_controller()->ResetSession();
  }
  return true;
}

void OmniboxEditModel::NotifyObserversInputInProgress(bool in_progress) {
  controller_->client()->OnInputInProgress(in_progress);

  if (user_input_in_progress_ || !in_revert_)
    controller_->client()->OnInputStateChanged();
}

void OmniboxEditModel::SetFocusState(OmniboxFocusState state,
                                     OmniboxFocusChangeReason reason) {
  if (state == focus_state_)
    return;

  // Update state and notify view if the omnibox has focus and the caret
  // visibility changed.
  const bool was_caret_visible = is_caret_visible();
  focus_state_ = state;
  if (focus_state_ != OMNIBOX_FOCUS_NONE &&
      is_caret_visible() != was_caret_visible && view_) {
    view_->ApplyCaretVisibility();
  }

  controller_->client()->OnFocusChanged(focus_state_, reason);
}

void OmniboxEditModel::OnFaviconFetched(const GURL& page_url,
                                        const gfx::Image& icon) const {
  if (icon.IsEmpty() || !PopupIsOpen()) {
    return;
  }

  // Notify all affected matches.
  for (size_t i = 0; i < controller_->result().size(); ++i) {
    auto& match = controller_->result().match_at(i);
    if (!AutocompleteMatch::IsSearchType(match.type) &&
        match.destination_url == page_url) {
      popup_view_->OnMatchIconUpdated(i);
    }
  }
}

std::u16string OmniboxEditModel::GetText() const {
  // Once the model owns primary text, the check for `view_` won't be needed.
  if (view_) {
    return view_->GetText();
  } else {
    NOTREACHED();
    return u"";
  }
}
