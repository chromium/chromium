// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
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
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/contextual_search_provider.h"
#include "components/omnibox/browser/history_fuzzy_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_logging_utils.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_text_util.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/omnibox/browser/verbatim_match.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "net/cookies/cookie_util.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/vector_icons/vector_icons.h"  // nogncheck
#endif

using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;
using omnibox::mojom::NavigationPredictor;

// Helpers --------------------------------------------------------------------

namespace {

const char kOmniboxAimEntrypointShown[] = "Omnibox.AimEntrypoint.Shown";
const char kOmniboxAimEntrypointActivatedUserTextPresent[] =
    "Omnibox.AimEntrypoint.Activated.UserTextPresent";
const char kOmniboxAimEntrypointActivatedViaKeyboard[] =
    "Omnibox.AimEntrypoint.Activated.ViaKeyboard";

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

// Like `kEnteredKeywordModeHistogram` but only logged when user has additional
// text input at the time keyword mode was entered.
const char kEnteredKeywordModeWithInputHistogram[] =
    "Omnibox.EnteredKeywordModeWithInput";

// Histogram name which counts the number of times the user completes a search
// in keyword mode, enumerated by how they enter keyword mode.
const char kAcceptedKeywordSuggestionHistogram[] =
    "Omnibox.AcceptedKeywordSuggestion";

// Histogram name which counts the number of times the user enters the keyword
// mode, enumerated by the type of search engine.
const char kKeywordModeUsageByEngineTypeEnteredHistogramName[] =
    "Omnibox.KeywordModeUsageByEngineType.Entered";

// Histogram name which counts the number of times the user completes a search
// in keyword mode, enumerated by the type of search engine.
const char kKeywordModeUsageByEngineTypeAcceptedHistogramName[] =
    "Omnibox.KeywordModeUsageByEngineType.Accepted";

void EmitEnteredKeywordModeHistogram(
    OmniboxEventProto::KeywordModeEntryMethod entry_method,
    const TemplateURL* turl,
    bool entered_with_input) {
  UMA_HISTOGRAM_ENUMERATION(
      kEnteredKeywordModeHistogram, static_cast<int>(entry_method),
      static_cast<int>(OmniboxEventProto::KeywordModeEntryMethod_MAX + 1));
  if (entered_with_input) {
    UMA_HISTOGRAM_ENUMERATION(
        kEnteredKeywordModeWithInputHistogram, static_cast<int>(entry_method),
        static_cast<int>(OmniboxEventProto::KeywordModeEntryMethod_MAX + 1));
  }

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

}  // namespace

// OmniboxEditModel::State ----------------------------------------------------

OmniboxEditModel::State::State(
    bool user_input_in_progress,
    const std::u16string& user_text,
    const std::u16string& keyword,
    const std::u16string& keyword_placeholder,
    bool is_keyword_hint,
    OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method,
    OmniboxFocusState focus_state,
    const AutocompleteInput& autocomplete_input)
    : user_input_in_progress(user_input_in_progress),
      user_text(user_text),
      keyword(keyword),
      keyword_placeholder(keyword_placeholder),
      is_keyword_hint(is_keyword_hint),
      keyword_mode_entry_method(keyword_mode_entry_method),
      focus_state(focus_state),
      autocomplete_input(autocomplete_input) {}

OmniboxEditModel::State::State(const State& other) = default;

OmniboxEditModel::State::~State() = default;

// OmniboxEditModel -----------------------------------------------------------

OmniboxEditModel::OmniboxEditModel(OmniboxController* controller)
    : controller_(controller) {}

OmniboxEditModel::~OmniboxEditModel() = default;

void OmniboxEditModel::set_popup_view(OmniboxPopupView* popup_view) {
  popup_view_ = popup_view;

  // Clear/reset popup-related state.
  rich_suggestion_bitmaps_.clear();
  icon_bitmaps_.clear();
  old_focused_url_ = GURL();
  popup_selection_ = OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                           OmniboxPopupSelection::NORMAL);
}

void OmniboxEditModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxEditModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

metrics::OmniboxEventProto::PageClassification
OmniboxEditModel::GetPageClassification() const {
  return controller_->client()->GetPageClassification(/*is_prefetch=*/false);
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
    if (!MaybePrependKeyword(display_text).empty()) {
      user_text = display_text;
    }
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
  return State(user_input_in_progress_, user_text, keyword_,
               keyword_placeholder_, is_keyword_hint_,
               keyword_mode_entry_method_, focus_state_, input_);
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
  if (!state) {
    return;
  }

  // The tab-management system saves the last-focused control for each tab and
  // restores it. That operation also updates this edit model's focus_state_
  // if necessary. This occurs before we reach this point in the code.
  //
  // The only reason we need to separately save and restore our focus state is
  // to preserve our special "invisible focus" state used for the fakebox.
  const bool invisible_focus_changed =
      focus_state_ == OMNIBOX_FOCUS_INVISIBLE ||
      state->focus_state == OMNIBOX_FOCUS_INVISIBLE;

  if (invisible_focus_changed) {
    SetFocusState(state->focus_state, OMNIBOX_FOCUS_CHANGE_TAB_SWITCH);
  }

  // Restore any user editing.
  if (state->user_input_in_progress) {
    // NOTE: Be sure to set keyword-related state AFTER invoking
    // SetUserText(), as SetUserText() clears the keyword state.
    if ((!state->user_text.empty() || !state->keyword.empty()) && view_) {
      view_->SetUserText(state->user_text, false);
    }
    SetKeyword(state->keyword);
    SetKeywordPlaceholder(state->keyword_placeholder);
    SetIsKeywordHint(state->is_keyword_hint);
    keyword_mode_entry_method_ = state->keyword_mode_entry_method;
    if (view_) {
      view_->OnKeywordPlaceholderTextChange();
    }
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

AutocompleteMatch OmniboxEditModel::CurrentMatchAndAlternateNavUrl(
    GURL* alternate_nav_url) const {
  // If we have a valid match use it. Otherwise get one for the current text.
  AutocompleteMatch match = current_match_;
  if (!match.destination_url.is_valid()) {
    GetInfoForCurrentText(&match, alternate_nav_url);
  } else if (alternate_nav_url) {
    AutocompleteProviderClient* provider_client =
        autocomplete_controller()->autocomplete_provider_client();
    *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        input_, match, provider_client);
  }
  return match;
}

bool OmniboxEditModel::ResetDisplayTexts() {
  const std::u16string old_display_text = GetPermanentDisplayText();
  url_for_editing_ = controller_->client()->GetFormattedFullURL();
  display_text_ = controller_->client()->GetURLForDisplay();

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
         (!has_focus() ||
          (!user_input_in_progress_ && !controller_->IsPopupOpen()));
}

std::u16string OmniboxEditModel::GetPermanentDisplayText() const {
  return display_text_;
}

void OmniboxEditModel::SetUserText(const std::u16string& text) {
  SetInputInProgress(true);
  SetKeyword(std::u16string());
  keyword_placeholder_.clear();
  SetIsKeywordHint(false);
  keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
  if (view_) {
    view_->OnKeywordPlaceholderTextChange();
  }
  InternalSetUserText(text);
  GetInfoForCurrentText(&current_match_, nullptr);
  paste_state_ = PasteState::kNone;
  has_temporary_text_ = false;
}

bool OmniboxEditModel::Unelide() {
  // Unelision should not occur if the user has already inputted text.
  if (user_input_in_progress()) {
    return false;
  }

  // No need to unelide if we are already displaying the full URL.
  if (GetText() == controller_->client()->GetFormattedFullURL()) {
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
      user_input_in_progress_ ? CurrentMatch() : AutocompleteMatch();

  controller_->client()->OnTextChanged(
      current_match, user_input_in_progress_, user_text_,
      autocomplete_controller()->result(), has_focus());
}

void OmniboxEditModel::GetDataForURLExport(GURL* url,
                                           std::u16string* title,
                                           gfx::Image* favicon) {
  *url = CurrentMatch().destination_url;
  if (*url == controller_->client()->GetURL()) {
    *title = controller_->client()->GetTitle();
    *favicon = controller_->client()->GetFavicon();
  }
}

bool OmniboxEditModel::CurrentTextIsURL() const {
  // If !user_input_in_progress_, we can determine if the text is a URL without
  // starting the autocomplete system. This speeds browser startup.
  return !user_input_in_progress_ ||
         !AutocompleteMatch::IsSearchType(CurrentMatch().type);
}

void OmniboxEditModel::AdjustTextForCopy(int sel_min,
                                         std::u16string* text,
                                         GURL* url_from_text,
                                         bool* write_url) {
  omnibox::AdjustTextForCopy(
      sel_min, text,
      /*has_user_modified_text=*/user_input_in_progress_ ||
          (*text != display_text_ && *text != url_for_editing_),
      is_keyword_selected(),
      controller_->IsPopupOpen()
          ? std::optional<AutocompleteMatch>(CurrentMatch())
          : std::nullopt,
      controller_->client(), url_from_text, write_url);
}

bool OmniboxEditModel::ShouldShowCurrentPageIcon() const {
  // If the popup is open, don't show the current page's icon. The caller is
  // instead expected to show the current match's icon.
  if (controller_->IsPopupOpen()) {
    return false;
  }

  // On the New Tab Page, the omnibox textfield is empty. We want to display
  // the default search provider favicon instead of the NTP security icon.
  if (GetText().empty()) {
    return false;
  }

  // If user input is not in progress, always show the current page's icon.
  if (!user_input_in_progress()) {
    return true;
  }

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

bool OmniboxEditModel::ShouldShowAddContextButton() const {
  const bool aim_button_pref =
      GetPrefService()->GetBoolean(omnibox::kShowAiModeOmniboxButton);
  const bool is_aim_popup_enabled = controller_->client()->IsAimPopupEnabled();
  const bool is_variant_inline =
      omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.Get() ==
      omnibox::AddContextButtonVariant::kInline;
  const bool is_popup_open = controller_->IsPopupOpen();

  return aim_button_pref && is_aim_popup_enabled && is_variant_inline &&
         is_popup_open;
}

ui::ImageModel OmniboxEditModel::GetAddContextIcon(int image_size) const {
  return ui::ImageModel::FromVectorIcon(kAddChromeRefreshIcon,
                                        ui::kColorSysPrimary, image_size);
}

gfx::Image OmniboxEditModel::GetAgentspaceIcon(bool dark_mode) const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (dark_mode) {
    // For dark mode, per Agentspace branding, use `SK_ColorWhite` over the
    // monochrome logo.
    return base::FeatureList::IsEnabled(omnibox::kUseAgentspace25Logo)
               ? controller_->client()->GetSizedIcon(
                     vector_icons::kGoogleAgentspaceMonochromeLogo25Icon,
                     SK_ColorWHITE)
               : controller_->client()->GetSizedIcon(
                     vector_icons::kGoogleAgentspaceMonochromeLogoIcon,
                     SK_ColorWHITE);
  } else {
    return base::FeatureList::IsEnabled(omnibox::kUseAgentspace25Logo)
               ? controller_->client()->GetSizedIcon(
                     ui::ResourceBundle::GetSharedInstance()
                         .GetImageNamed(IDR_GOOGLE_AGENTSPACE_LOGO_25)
                         .ToSkBitmap())
               : controller_->client()->GetSizedIcon(
                     ui::ResourceBundle::GetSharedInstance()
                         .GetImageNamed(IDR_GOOGLE_AGENTSPACE_LOGO)
                         .ToSkBitmap());
  }
#else
  return gfx::Image();
#endif
}

void OmniboxEditModel::UpdateInput(bool has_selected_text,
                                   bool prevent_inline_autocomplete) {
  bool changed_to_user_input_in_progress = SetInputInProgressNoNotify(true);
  if (!has_focus()) {
    if (changed_to_user_input_in_progress) {
      NotifyObserversInputInProgress(true);
    }
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

  if (changed_to_user_input_in_progress) {
    NotifyObserversInputInProgress(true);
  }
}

void OmniboxEditModel::SetInputInProgress(bool in_progress) {
  if (SetInputInProgressNoNotify(in_progress)) {
    NotifyObserversInputInProgress(in_progress);
  }
}

void OmniboxEditModel::Revert() {
  SetInputInProgress(false);
  input_.Clear();
  paste_state_ = PasteState::kNone;
  InternalSetUserText(std::u16string());
  SetKeyword(std::u16string());
  keyword_placeholder_.clear();
  SetIsKeywordHint(false);
  keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
  if (view_) {
    view_->OnKeywordPlaceholderTextChange();
  }
  has_temporary_text_ = false;
  gfx::Range selection;
  if (view_) {
    selection = view_->GetSelectionBounds();
  }
  current_match_ = AutocompleteMatch();
  // First home the cursor, so view of text is scrolled to left, then correct
  // it. |SetCaretPos()| doesn't scroll the text, so doing that first wouldn't
  // accomplish anything.
  std::u16string current_permanent_url = GetPermanentDisplayText();
  if (view_) {
    view_->SetWindowTextAndCaretPos(current_permanent_url, 0, false, true);
    view_->SetCaretPos(
        std::min(current_permanent_url.size(), selection.start()));
  }
  controller_->client()->OnRevert();
}

void OmniboxEditModel::StartAutocomplete(bool has_selected_text,
                                         bool prevent_inline_autocomplete) {
  const std::u16string input_text = MaybePrependKeyword(user_text_);

  // This method currently only works when there's a view, but ideally the
  // model should be primary for determining such state.
  CHECK(view_);
  size_t cursor_position = view_->GetSelectionBounds().end();

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
      (has_selected_text && inline_autocompletion_.empty()) ||
      paste_state_ != PasteState::kNone);
  input_.set_prefer_keyword(is_keyword_selected());
  input_.set_allow_exact_keyword_match(is_keyword_selected() ||
                                       allow_exact_keyword_match_);
  input_.set_keyword_mode_entry_method(keyword_mode_entry_method_);
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          controller_->client()->GetLensOverlaySuggestInputs()) {
    input_.set_lens_overlay_suggest_inputs(*suggest_inputs);
  }

  controller_->StartAutocomplete(input_);
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

void OmniboxEditModel::EnterKeywordMode(
    OmniboxEventProto::KeywordModeEntryMethod entry_method,
    const TemplateURL* template_url,
    const std::u16string& placeholder_text) {
  DCHECK(template_url);
  controller_->StopAutocomplete(/*clear_result=*/false);

  if (keyword_ != template_url->keyword()) {
    // Note, this is not the only place that keyword mode can be entered, but
    // it would be better to make it so than to add extra notification calls
    // elsewhere. At present, the method is only meaningfully used for exit.
    controller_->client()->OnKeywordModeChanged(true, template_url->keyword());
  }
  SetKeyword(template_url->keyword());
  SetKeywordPlaceholder(placeholder_text);
  SetIsKeywordHint(false);
  keyword_mode_entry_method_ = entry_method;
  if (view_) {
    view_->OnKeywordPlaceholderTextChange();
  }

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

  EmitEnteredKeywordModeHistogram(entry_method, template_url,
                                  !user_text_.empty());
}

void OmniboxEditModel::EnterKeywordModeForDefaultSearchProvider(
    OmniboxEventProto::KeywordModeEntryMethod entry_method) {
  if (!controller_->client()->IsDefaultSearchProviderEnabled()) {
    return;
  }
  EnterKeywordMode(entry_method,
                   controller_->client()
                       ->GetTemplateURLService()
                       ->GetDefaultSearchProvider(),
                   u"");
}

void OmniboxEditModel::OpenAiMode(bool via_keyboard, bool via_context_menu) {
  std::u16string query_text =
      AutocompleteMatch::IsSearchType(current_match_.type)
          ? current_match_.contents
          : u"";
  RecordAiModeMetrics(query_text, /*activated=*/true, via_keyboard);

  bool force_navigation_to_aim =
      !via_context_menu &&
      base::FeatureList::IsEnabled(omnibox::kAiModeEntryPointAlwaysNavigates);
  if (!force_navigation_to_aim && controller_->client()->IsAimPopupEnabled()) {
    // In general, adding a context will always open the AIM popup, while the
    // AIM button will prefer to navigate to the AI page with a query
    // prepopulated.
    bool open_aim_popup = via_context_menu;
    // When the default suggestion is selected and the text is unmodified or
    // clobbered, then there is no text to prepopulate, so resort to opening the
    // AIM popup. `kNoMatch` is used on NTP focus and some other edge cases.
    open_aim_popup |=
        (popup_selection_.line == 0 ||
         popup_selection_.line == OmniboxPopupSelection::kNoMatch) &&
        (!user_input_in_progress_ || user_text_.empty());
    // If a URL match has been selected, there are privacy concerns
    // with prepopulating the URL when navigating to the AI page, so instead
    // open the AIM popup. This also applies to when the default suggestion is
    // still selected with a user edit that defaults a URL.
    open_aim_popup |= !AutocompleteMatch::IsSearchType(current_match_.type);
    // In summary:
    // - Default suggestion selected:
    //   - The text is unmodified -> AIM popup
    //   - The text is clobbered -> AIM popup
    //   - The text is modified, not empty, and defaults a URL -> AIM popup
    //   - The text is modified, not empty, and defaults a search -> AI page
    // - A non default suggestion is selected, regardless of user input state:
    //   - if a URL suggestion is selected -> AIM popup
    //   - if a search suggestion is selected -> AI page
    if (open_aim_popup) {
      controller_->popup_state_manager()->SetPopupState(
          OmniboxPopupState::kAim);
      return;
    }
  }

  GURL ai_mode_url =
      GetUrlForAim(controller_->client()->GetTemplateURLService(),
                   omnibox::DESKTOP_CHROME_OMNIBOX_KEYWORD_ENTRY_POINT,
                   /*query_start_time=*/base::Time::Now(), query_text);
  controller_->client()->OpenUrl(ai_mode_url);
}

void OmniboxEditModel::OpenLensSearch() {
  if (auto* provider =
          autocomplete_controller()->contextual_search_provider()) {
    OpenMatch(
        OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch),
        provider->CreateLensEntrypointMatch(autocomplete_controller()->input()),
        WindowOpenDisposition::CURRENT_TAB, GURL(), std::u16string(),
        base::TimeTicks::Now());
  }
}

void OmniboxEditModel::OpenSelection(OmniboxPopupSelection selection,
                                     base::TimeTicks timestamp,
                                     WindowOpenDisposition disposition,
                                     bool via_keyboard) {
  base::UmaHistogramMicrosecondsTimes("Omnibox.InputToOpenSelection",
                                      base::TimeTicks::Now() - timestamp);

  // Check for AIM button focus state first, since it can have a line selection
  // of `kNoMatch`, which would otherwise be handled by the `AcceptInput` case
  // below.
  if (selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_AIM) {
    OpenAiMode(via_keyboard, /*via_context_menu=*/false);
    return;
  }
  // If the AIM page action was NOT activated, then make sure we still record
  // the appropriate AI Mode UMA metrics.
  RecordAiModeMetrics(/*query_text=*/u"", /*activated=*/false, via_keyboard);

  // Intentionally accept input when selection has no line.
  // This will usually reach `OpenMatch` indirectly.
  if (selection.line >= autocomplete_controller()->result().size()) {
    AcceptInput(disposition, timestamp);
    return;
  }

  // The keyword mode button doesn't commit the omnibox, it's a
  // transient UI element leading to other normal omnibox selections.
  if (selection.state == OmniboxPopupSelection::KEYWORD_MODE) {
    return;
  }

  const AutocompleteMatch& match =
      autocomplete_controller()->result().match_at(selection.line);

  if (selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_THUMBS_UP) {
    UpdateFeedbackOnMatch(selection.line, FeedbackType::kThumbsUp);
  } else if (selection.state ==
             OmniboxPopupSelection::FOCUSED_BUTTON_THUMBS_DOWN) {
    UpdateFeedbackOnMatch(selection.line, FeedbackType::kThumbsDown);
  } else if (selection.state ==
             OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION) {
    TryDeletingPopupLine(selection.line);
  } else if (selection.state == OmniboxPopupSelection::FOCUSED_IPH_LINK) {
    controller_->client()->OpenIphLink(match.iph_link_url);
  } else {
    // Open the match.
    GURL alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        input_, match,
        autocomplete_controller()->autocomplete_provider_client());
    OpenMatch(selection, match, disposition, alternate_nav_url,
              std::u16string(), timestamp);
  }
}

void OmniboxEditModel::OpenSelectionForTesting(
    base::TimeTicks timestamp,
    WindowOpenDisposition disposition,
    bool via_keyboard) {
  OpenSelection(popup_selection_, timestamp, disposition, via_keyboard);
}

void OmniboxEditModel::AcceptKeyword(
    OmniboxEventProto::KeywordModeEntryMethod entry_method) {
  TRACE_EVENT0("omnibox", "OmniboxEditModel::AcceptKeyword");

  DCHECK(!keyword_.empty()) << keyword_;

  controller_->StopAutocomplete(/*clear_result=*/false);

  SetIsKeywordHint(false);
  keyword_mode_entry_method_ = entry_method;
  if (original_user_text_with_keyword_.empty()) {
    original_user_text_with_keyword_ = user_text_;
  }
  user_text_ = MaybeStripKeyword(user_text_);

  if (controller_->IsPopupOpen()) {
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
    if (!user_text_.empty()) {
      view_->UpdatePopup();
    }
  }

  base::RecordAction(base::UserMetricsAction("AcceptedKeywordHint"));
  const TemplateURL* turl =
      controller_->client()->GetTemplateURLService()->GetTemplateURLForKeyword(
          keyword_);
  EmitEnteredKeywordModeHistogram(entry_method, turl, !user_text_.empty());
}

void OmniboxEditModel::AcceptTemporaryTextAsUserText() {
  InternalSetUserText(GetText());
  has_temporary_text_ = false;

  if (user_input_in_progress_ || !in_revert_) {
    controller_->client()->OnInputStateChanged();
  }
}

void OmniboxEditModel::ClearKeyword() {
  if (!is_keyword_selected() || !view_) {
    return;
  }

  TRACE_EVENT0("omnibox", "OmniboxEditModel::ClearKeyword");
  controller_->StopAutocomplete(/*clear_result=*/false);

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
    SetIsKeywordHint(true);
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
    if (keyword_mode_entry_method_ == OmniboxEventProto::QUESTION_MARK) {
      prefix = u"?";
    } else if (keyword_mode_entry_method_ !=
               OmniboxEventProto::KEYBOARD_SHORTCUT) {
      prefix = keyword_ + u" ";
    }

    SetKeyword(std::u16string());
    keyword_placeholder_.clear();
    SetIsKeywordHint(false);
    keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
    if (view_) {
      view_->OnKeywordPlaceholderTextChange();
    }

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
  focus_resulted_in_navigation_ = false;

  // If the omnibox lost focus while the caret was hidden and then regained
  // focus, OnSetFocus() is called and should restore visibility. Note that
  // focus can be regained without an accompanying call to
  // OmniboxView::SetFocus(), e.g. by tabbing in.
  SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  // On focusing the omnibox, if the ctrl key is pressed, we don't want to
  // trigger ctrl-enter behavior unless it is released and re-pressed. For
  // example, if the user presses ctrl-l to focus the omnibox.
  control_key_state_ =
      control_down ? ControlKeyState::kDownAndConsumed : ControlKeyState::kUp;

  if (user_input_in_progress_ || !in_revert_) {
    controller_->client()->OnInputStateChanged();
  }

  if (omnibox_feature_configs::HappinessTrackingSurveyForOmniboxOnFocusZps::
          Get()
              .enabled) {
    controller_->client()->MaybeShowOnFocusHatsSurvey(
        autocomplete_controller()->autocomplete_provider_client());
  }
}

void OmniboxEditModel::StartZeroSuggestRequest(
    bool user_clobbered_permanent_text) {
  // Early exit if a query is already in progress or the popup is already open.
  // This is what allows this method to be called multiple times in multiple
  // code locations without harm.
  if (!autocomplete_controller()->done() || controller_->IsPopupOpen()) {
    return;
  }

  // Early exit if the page has not loaded yet, so we don't annoy users.
  if (!controller_->client()->CurrentPageExists()) {
    return;
  }

  // Early exit if the user already has a navigation or search query in mind.
  if (user_input_in_progress_ && !user_clobbered_permanent_text) {
    return;
  }

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
  input_.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  // Set the lens overlay suggest inputs, if available.
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          controller_->client()->GetLensOverlaySuggestInputs()) {
    input_.set_lens_overlay_suggest_inputs(*suggest_inputs);
  }
  controller_->StartAutocomplete(input_);
}

void OmniboxEditModel::SetCaretVisibility(bool visible) {
  // Caret visibility only matters if the omnibox has focus.
  if (focus_state_ != OMNIBOX_FOCUS_NONE) {
    SetFocusState(visible ? OMNIBOX_FOCUS_VISIBLE : OMNIBOX_FOCUS_INVISIBLE,
                  OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  }
}

void OmniboxEditModel::ConsumeCtrlKey() {
  if (control_key_state_ == ControlKeyState::kDown) {
    control_key_state_ = ControlKeyState::kDownAndConsumed;
  }
}

void OmniboxEditModel::OnWillKillFocus() {
  if (user_input_in_progress_ || !in_revert_) {
    controller_->client()->OnInputStateChanged();
  }
}

void OmniboxEditModel::OnKillFocus() {
  UMA_HISTOGRAM_BOOLEAN(kOmniboxFocusResultedInNavigation,
                        focus_resulted_in_navigation_);
  SetFocusState(OMNIBOX_FOCUS_NONE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  last_omnibox_focus_ = base::TimeTicks();
  paste_state_ = PasteState::kNone;
  control_key_state_ = ControlKeyState::kUp;
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
  if (controller_->IsPopupOpen()) {
    base::UmaHistogramEnumeration(kOmniboxEscapeHistogramName,
                                  OmniboxEscapeAction::kClosePopup);
    if (auto* popup_closer = controller_->client()->GetOmniboxPopupCloser()) {
      popup_closer->CloseWithReason(
          omnibox::PopupCloseReason::kEscapeKeyPressed);
    }
    return true;
  }

  // Unconditionally revert/select all.  This ensures any popup, whether due to
  // normal editing or ZeroSuggest, is closed, and the full text is selected.
  // This in turn allows the user to use escape to quickly select all the text
  // for ease of replacement, and matches other browsers.
  bool user_input_was_in_progress = user_input_in_progress_;
  // TODO(crbug.com/40230336): If the popup was open, `user_input_in_progress_`
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
  if (pressed == (control_key_state_ == ControlKeyState::kUp)) {
    control_key_state_ =
        pressed ? ControlKeyState::kDown : ControlKeyState::kUp;
  }
}

void OmniboxEditModel::OnPaste() {
  UMA_HISTOGRAM_COUNTS_1M("Omnibox.Paste", 1);
  paste_state_ = PasteState::kPasting;
}

void OmniboxEditModel::OnUpOrDownPressed(bool down, bool page) {
  const auto direction =
      down ? OmniboxPopupSelection::kForward : OmniboxPopupSelection::kBackward;
  const auto step = page ? OmniboxPopupSelection::kAllLines
                         : OmniboxPopupSelection::kWholeLine;
  StepPopupSelection(direction, step);
}

void OmniboxEditModel::OnTabPressed(bool shift) {
  StepPopupSelection(shift ? OmniboxPopupSelection::kBackward
                           : OmniboxPopupSelection::kForward,
                     OmniboxPopupSelection::kStateOrLine);
}

bool OmniboxEditModel::OnSpacePressed() {
  if (!AllowKeywordSpaceTriggering()) {
    return false;
  }
  if (!is_keyword_hint_ && keyword_.empty() &&
      input_.cursor_position() == input_.text().length()) {
    // Keywords can now be accessed anywhere in the match list. If one is
    // found on an instant keyword match, select and accept it.
    const AutocompleteResult& result = autocomplete_controller()->result();
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
  if (autocomplete_controller()->result().empty()) {
    return;
  }

  if (line == OmniboxPopupSelection::kNoMatch) {
    return;
  }

  if (line >= autocomplete_controller()->result().size()) {
    return;
  }

  controller_->client()->OnNavigationLikely(
      line, autocomplete_controller()->result().match_at(line),
      navigation_predictor);
}

void OmniboxEditModel::OnPopupDataChanged(
    const std::u16string& temporary_text,
    bool is_temporary_text,
    const std::u16string& inline_autocompletion,
    const std::u16string& keyword,
    const std::u16string& keyword_placeholder,
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
    SetKeyword(keyword);
    SetKeywordPlaceholder(keyword_placeholder);
    SetIsKeywordHint(is_keyword_hint);
    if (!keyword_was_selected && is_keyword_selected()) {
      // Since we entered keyword mode, record the reason. Note that we
      // don't do this simply because the keyword changes, since the user
      // never left keyword mode.
      keyword_mode_entry_method_ = OmniboxEventProto::SELECT_SUGGESTION;
    } else if (!is_keyword_selected()) {
      // We've left keyword mode, so align the entry method field with that.
      keyword_mode_entry_method_ = OmniboxEventProto::INVALID;
    }
    if (view_) {
      view_->OnKeywordPlaceholderTextChange();
    }
    observers_.Notify(&Observer::OnKeywordStateChanged, is_keyword_hint);

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
  if (inline_autocompletion_.empty() && view_) {
    view_->OnInlineAutocompleteTextCleared();
  }

  const std::u16string& user_text =
      user_input_in_progress_ ? user_text_ : input_.text();
  if (keyword_state_changed && is_keyword_selected() &&
      inline_autocompletion_.empty()) {
    // If we reach here, the user most likely entered keyword mode by inserting
    // a space between a keyword name and a search string (as pressing space or
    // tab after the keyword name alone would have been be handled in
    // `ShouldAcceptKeywordAfterInsertingSpaceAtEnd()` by calling
    // `AcceptKeyword()`, which won't reach here).  In this case, we don't want
    // to call `OnInlineAutocompleteTextMaybeChanged()` as normal, because that
    // will correctly change the text (to the search string alone) but move the
    // caret to the end of the string; instead we want the caret at the start of
    // the search string since that's where it was in the original input.  So we
    // set the text and caret position directly.
    //
    // It may also be possible to reach here if we're reverting from having
    // temporary text back to a default match that's a keyword search, but in
    // that case the `RevertTemporaryTextAndPopup()` call below will reset the
    // caret or selection correctly so the caret positioning we do here won't
    // matter.
    if (view_) {
      view_->SetWindowTextAndCaretPos(user_text, 0, false, true);
    }
  } else {
    if (view_) {
      view_->OnInlineAutocompleteTextMaybeChanged(user_text,
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
  if (paste_state_ == PasteState::kPasting) {
    paste_state_ = PasteState::kPasted;
    GURL url = GURL(*state_changes.new_text);
    if (url.is_valid()) {
      controller_->client()->OnUserPastedInOmniboxResultingInValidURL();
    }
  } else if (state_changes.text_differs) {
    paste_state_ = PasteState::kNone;
  }

  if (state_changes.text_differs || state_changes.selection_differs) {
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
  // reset `just_deleted_text_`. Modifying the selection accepts any inline
  // autocompletion, which results in a user text change.
  if (!state_changes.text_differs &&
      (!state_changes.selection_differs || inline_autocompletion_.empty())) {
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

  const bool no_selection = state_changes.new_selection.is_empty();

  // Update the popup for the change, in the process changing to keyword mode
  // if the user hit space in mid-string after a keyword.
  // `allow_exact_keyword_match_` will be used by `StartAutocomplete()`, which
  // will be called by `view_->UpdatePopup()`; so after that returns we can
  // safely reset this flag.
  allow_exact_keyword_match_ =
      state_changes.text_differs && allow_keyword_ui_change &&
      !state_changes.just_deleted_text && no_selection &&
      ShouldAcceptKeywordAfterInsertingSpaceInMiddle(
          *state_changes.old_text, user_text_,
          state_changes.new_selection.start());
  if (view_) {
    view_->UpdatePopup();
  }
  if (allow_exact_keyword_match_) {
    keyword_mode_entry_method_ = OmniboxEventProto::SPACE_IN_MIDDLE;
    const TemplateURL* turl = controller_->client()
                                  ->GetTemplateURLService()
                                  ->GetTemplateURLForKeyword(keyword_);
    EmitEnteredKeywordModeHistogram(OmniboxEventProto::SPACE_IN_MIDDLE, turl,
                                    !user_text_.empty());
    allow_exact_keyword_match_ = false;
  }

  if (!state_changes.text_differs || !allow_keyword_ui_change ||
      (state_changes.just_deleted_text && no_selection) ||
      is_keyword_selected() || paste_state_ != PasteState::kNone) {
    return true;
  }

  // If the user input a "?" at the beginning of the text, put them into
  // keyword mode for their default search provider.
  if (state_changes.new_selection.start() == 1 && user_text_[0] == '?') {
    EnterKeywordModeForDefaultSearchProvider(OmniboxEventProto::QUESTION_MARK);
    return false;
  }

  if (state_changes.new_selection.start() != user_text_.size()) {
    return true;
  }

  // Change to keyword mode if the user is now pressing space after a keyword
  // name. If this is the case, then even if there was no keyword hint when we
  // entered this function (e.g. if the user has used space to replace some
  // selected text that was adjoined to this keyword), there will be one now
  // because of the call to `UpdatePopup()` above; so it's safe for
  // `ShouldAcceptKeywordAfterInsertingSpaceAtEnd()` to look at `keyword_` and
  // `is_keyword_hint_` to determine what keyword, if any, is applicable.
  //
  // If `ShouldAcceptKeywordAfterInsertingSpaceAtEnd()` accepts the keyword and
  // returns true, that will have updated our state already, so in that case we
  // don't also return true from this function.
  if (ShouldAcceptKeywordAfterInsertingSpaceAtEnd(user_text_)) {
    AcceptKeyword(OmniboxEventProto::SPACE_AT_END);
    return true;
  }
  return false;
}

// TODO(beaudoin): Merge OnPopupDataChanged with this method once the popup
// handling has completely migrated to omnibox_controller.
void OmniboxEditModel::OnCurrentMatchChanged() {
  has_temporary_text_ = false;

  DCHECK(autocomplete_controller()->result().default_match());
  const AutocompleteMatch& match =
      *autocomplete_controller()->result().default_match();

  // We store |keyword| and |is_keyword_hint| in temporary variables since
  // OnPopupDataChanged use their previous state to detect changes.
  std::u16string keyword;
  std::u16string keyword_placeholder;
  bool is_keyword_hint;
  TemplateURLService* service = controller_->client()->GetTemplateURLService();
  match.GetKeywordUIState(service,
                          controller_->client()->IsHistoryEmbeddingsEnabled(),
                          &keyword, &keyword_placeholder, &is_keyword_hint);

  if (!is_keyword_selected() && !is_keyword_hint && !keyword.empty()) {
    // We just entered keyword mode, so remove the keyword from the input.
    // We don't call MaybeStripKeyword, as we haven't yet updated our internal
    // state (keyword_ and is_keyword_hint_), and MaybeStripKeyword checks this.
    user_text_ =
        AutocompleteInput::SplitReplacementStringFromInput(user_text_, false);
    original_user_text_with_keyword_.clear();
  }

  // OnPopupDataChanged() resets OmniboxController's |current_match_| early
  // on.  Therefore, copy match.inline_autocompletion to a temp to preserve
  // its value across the entire call.
  OnPopupDataChanged(std::u16string(),
                     /*is_temporary_text=*/false, match.inline_autocompletion,
                     keyword, keyword_placeholder, is_keyword_hint,
                     match.additional_text, match);

  // Notify observers after the match has been safely copied to |current_match_|
  // in OnPopupDataChanged(). This prevents use-after-free if observers
  // invalidate the autocomplete results. See https://crbug.com/462736555.
  OnPopupResultChanged();
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
  if (view_) {
    view_->OnInlineAutocompleteTextCleared();
  }
}

std::u16string OmniboxEditModel::MaybeStripKeyword(
    const std::u16string& text) const {
  return is_keyword_selected()
             ? AutocompleteInput::SplitReplacementStringFromInput(text, false)
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
  if (!autocomplete_controller()->done() || controller_->IsPopupOpen()) {
    if (!autocomplete_controller()->done() &&
        autocomplete_controller()->result().default_match()) {
      // The user cannot have manually selected a match, or the query would have
      // stopped. So the default match must be the desired selection.
      *match = *autocomplete_controller()->result().default_match();
      found_match_for_text = true;
    } else if (controller_->IsPopupOpen() &&
               GetPopupSelection().line != OmniboxPopupSelection::kNoMatch) {
      const OmniboxPopupSelection selection = GetPopupSelection();
      // TODO(crbug.com/468047546): https://crrev.com/c/7191448 introduced a
      // change in events that makes it possible for the selection to be out of
      // bounds here. Follow up to figure out why this is and fix in a wholistic
      // way.
      if (selection.line < autocomplete_controller()->result().size()) {
        *match = autocomplete_controller()->result().match_at(selection.line);
        found_match_for_text = true;
      }
    }
    if (found_match_for_text && alternate_nav_url &&
        (!popup_view_ || IsPopupSelectionOnInitialLine())) {
      AutocompleteProviderClient* provider_client =
          autocomplete_controller()->autocomplete_provider_client();
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
  if ((!user_input_in_progress_ ||
       !autocomplete_controller()->result().default_match()) &&
      view_) {
    view_->SetWindowTextAndCaretPos(input_.text(), /*caret_pos=*/0,
                                    /*update_popup=*/false,
                                    /*notify_text_changed=*/true);
  }

  if (view_) {
    AutocompleteMatch match = CurrentMatch();
    view_->OnRevertTemporaryText(match.fill_into_edit, match);
  }
}

bool OmniboxEditModel::IsStarredMatch(const AutocompleteMatch& match) const {
  auto* bookmark_model = controller_->client()->GetBookmarkModel();
  return bookmark_model && bookmark_model->IsBookmarked(match.destination_url);
}

gfx::Image OmniboxEditModel::GetMatchIcon(const AutocompleteMatch& match,
                                          SkColor vector_icon_color,
                                          bool dark_mode) const {
  if (!match.icon_url.is_empty()) {
    const SkBitmap* bitmap = GetIconBitmap(match.icon_url);
    if (bitmap) {
      return controller_->client()->GetSizedIcon(bitmap);
    }
  }

  gfx::Image extension_icon = GetMatchIconIfExtension(match);
  if (!extension_icon.IsEmpty()) {
    return extension_icon;
  }

  const TemplateURL* turl =
      match.associated_keyword.empty()
          ? nullptr
          : controller_->client()
                ->GetTemplateURLService()
                ->GetTemplateURLForKeyword(match.associated_keyword);

  // Get the favicon for navigational suggestions.
  //
  // The starter pack suggestions are a unique case. These suggestions
  // normally use a favicon image that cannot be styled further by client
  // code. In order to apply custom styling to the icon (e.g. colors), we ignore
  // this favicon in favor of using a vector icon which has better styling
  // support.
  // Enterprise search aggregator people suggestions are another unique case.
  // These suggestions should use the Agentspace icon if available. Otherwise,
  // they should use the vector icon instead of the favicon.
  if (match.enterprise_search_aggregator_type ==
      AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE) {
    gfx::Image agentspace_icon = GetAgentspaceIcon(dark_mode);
    if (!agentspace_icon.IsEmpty()) {
      return agentspace_icon;
    }
  } else if (!AutocompleteMatch::IsSearchType(match.type) &&
             match.type != AutocompleteMatchType::DOCUMENT_SUGGESTION &&
             match.type != AutocompleteMatchType::HISTORY_CLUSTER &&
             match.type != AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER &&
             !AutocompleteMatch::IsStarterPackType(match.type)) {
    // Because the Views UI code calls GetMatchIcon in both the layout and
    // painting code, we may generate multiple `OnFaviconFetched` callbacks,
    // all run one after another. This seems to be harmless as the callback
    // just flips a flag to schedule a repaint. However, if it turns out to be
    // costly, we can optimize away the redundant extra callbacks.
    gfx::Image favicon;
    auto on_icon_fetched =
        base::BindOnce(&OmniboxEditModel::OnFaviconFetched,
                       weak_factory_.GetWeakPtr(), match.destination_url);
    favicon =
        (turl && AutocompleteMatch::IsFeaturedEnterpriseSearchType(match.type))
            ? controller_->client()->GetFaviconForKeywordSearchProvider(
                  turl, std::move(on_icon_fetched))
            : controller_->client()->GetFaviconForPageUrl(
                  match.destination_url, std::move(on_icon_fetched));

    // Extension icons are the correct size for non-touch UI but need to be
    // adjusted to be the correct size for touch mode.
    if (!favicon.IsEmpty()) {
      return controller_->client()->GetSizedIcon(favicon);
    }
  }

  bool is_starred_match = IsStarredMatch(match);
  const auto& vector_icon_type = match.GetVectorIcon(is_starred_match, turl);

  return controller_->client()->GetSizedIcon(vector_icon_type,
                                             vector_icon_color);
}

gfx::Image OmniboxEditModel::GetMatchIconIfExtension(
    const AutocompleteMatch& match) const {
  // Return an empty image if not an extension match.
  TemplateURLService* service = controller_->client()->GetTemplateURLService();
  const TemplateURL* template_url = match.GetTemplateURL(service);
  if (!template_url ||
      template_url->type() != TemplateURL::OMNIBOX_API_EXTENSION) {
    return gfx::Image();
  }

  // Return the image specified in the suggestion match if set by looking it up
  // in the rich suggestions bitmaps. Fall back to the extension icon if empty
  // or not found.
  if (match.provider &&
      match.provider->type() == AutocompleteProvider::TYPE_UNSCOPED_EXTENSION &&
      !match.ImageUrl().is_empty()) {
    const SkBitmap* bitmap = GetPopupRichSuggestionBitmap(match.image_url);
    if (bitmap) {
      return controller_->client()->GetSizedIcon(bitmap);
    }
  }

  gfx::Image extension_icon =
      controller_->client()->GetExtensionIcon(template_url);
  // Extension icons are the correct size for non-touch UI but need to be
  // adjusted to be the correct size for touch mode
  return extension_icon.IsEmpty()
             ? extension_icon
             : controller_->client()->GetSizedIcon(extension_icon);
}

std::u16string OmniboxEditModel::GetSuggestionGroupHeaderText(
    const std::optional<omnibox::GroupId>& suggestion_group_id) const {
  if (suggestion_group_id.has_value()) {
    const auto& input = autocomplete_controller()->input();
    bool force_hide_row_header =
        OmniboxFieldTrial::IsHideSuggestionGroupHeadersEnabledInContext(
            input.current_page_classification());
    auto header_text =
        autocomplete_controller()->result().GetHeaderForSuggestionGroup(
            suggestion_group_id.value());

    bool has_toolbelt_lens_action =
        autocomplete_controller()->contextual_search_provider() &&
        autocomplete_controller()
            ->contextual_search_provider()
            ->HasToolbeltLensAction();
    const auto* client =
        autocomplete_controller()->autocomplete_provider_client();
    bool has_lens_search_chip =
        client->IsOmniboxNextLensSearchChipEnabled() &&
        ContextualSearchProvider::LensEntrypointEligible(input, client);
    // Show contextual search suggestion group header if the Lens action has
    // been moved to the Omnibox toolbelt OR the "Omnibox Next" Lens search chip
    // is currently active.
    if (suggestion_group_id.value() == omnibox::GROUP_CONTEXTUAL_SEARCH &&
        (has_toolbelt_lens_action || has_lens_search_chip)) {
      // TODO(khalidpeer): Make direct use of `header_text` once we start
      //     receiving a non-empty contextual search header from the server.
      return header_text.empty()
                 ? l10n_util::GetStringUTF16(
                       IDS_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_LABEL)
                 : header_text;
    }
    return force_hide_row_header ? u"" : header_text;
  }
  return u"";
}

void OmniboxEditModel::ResetPopupToInitialState() {
  if (!popup_view_) {
    return;
  }
  size_t new_line = autocomplete_controller()->result().default_match()
                        ? 0
                        : OmniboxPopupSelection::kNoMatch;
  SetPopupSelection(
      OmniboxPopupSelection(new_line, OmniboxPopupSelection::NORMAL),
      /*reset_to_default=*/true);
  popup_view_->OnDragCanceled();
}

OmniboxPopupSelection OmniboxEditModel::GetPopupSelection() const {
  DCHECK(popup_view_);
  return popup_selection_;
}

void OmniboxEditModel::SetPopupSelection(OmniboxPopupSelection new_selection,
                                         bool reset_to_default,
                                         bool force_update_ui) {
  DCHECK(popup_view_);

  if (autocomplete_controller()->result().empty()) {
    return;
  }

  // Cancel the query so the matches don't change on the user.
  controller_->StopAutocomplete(/*clear_result=*/false);

  if (new_selection == popup_selection_ && !force_update_ui) {
    // This occurs when e.g. pressing tab to select an action chip or the x
    // delete icon. Nothing else to do.
    return;
  }

  // We need to update selection before notifying any views, as they will query
  // `popup_selection_` to update themselves.
  const OmniboxPopupSelection old_selection = popup_selection_;
  popup_selection_ = new_selection;
  observers_.Notify(&Observer::OnSelectionChanged, old_selection,
                    popup_selection_);

  // Special case for updating the focus ring around the AIM button.
  bool apply_focus_ring_to_aim_button =
      popup_selection_.state == OmniboxPopupSelection::FOCUSED_BUTTON_AIM;
  bool remove_focus_ring_from_aim_button =
      old_selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_AIM;
  if (apply_focus_ring_to_aim_button || remove_focus_ring_from_aim_button) {
    view_->ApplyFocusRingToAimButton(apply_focus_ring_to_aim_button);
  }

  const AutocompleteMatch& match =
      popup_selection_.line == OmniboxPopupSelection::kNoMatch
          ? AutocompleteMatch()
          : autocomplete_controller()->result().match_at(popup_selection_.line);

  // Can't select keyword chip if the match shouldn't show a keyword chip.
  DCHECK(popup_selection_.state != OmniboxPopupSelection::KEYWORD_MODE ||
         !match.associated_keyword.empty());

  if (popup_selection_.IsButtonFocused()) {
    old_focused_url_ = match.destination_url;
    SetAccessibilityLabel(match);
    // TODO(tommycli): Fold the focus hint into
    // popup_view_->OnSelectionChanged(). Caveat: We must update the
    // accessibility label before notifying the View.
    if (popup_selection_.line != OmniboxPopupSelection::kNoMatch) {
      popup_view_->ProvideButtonFocusHint(GetPopupSelection().line);
    }
  }

  std::u16string keyword;
  std::u16string keyword_placeholder;
  bool is_keyword_hint;
  TemplateURLService* service = controller_->client()->GetTemplateURLService();
  match.GetKeywordUIState(service,
                          controller_->client()->IsHistoryEmbeddingsEnabled(),
                          &keyword, &keyword_placeholder, &is_keyword_hint);

  // Don't update the edit model if entering or leaving keyword mode; doing so
  // breaks keyword mode. Updating when there is no line change is necessary
  // because omnibox text changes when:
  // a) Moving down from a header row.
  // b) Focusing other states; e.g. the switch-to-tab chip.
  if (old_selection.line != popup_selection_.line ||
      (old_selection.state != OmniboxPopupSelection::KEYWORD_MODE &&
       new_selection.state != OmniboxPopupSelection::KEYWORD_MODE)) {
    if (reset_to_default) {
      OnPopupDataChanged(
          std::u16string(),
          /*is_temporary_text=*/false, match.inline_autocompletion, keyword,
          keyword_placeholder, is_keyword_hint, match.additional_text, match);
    } else {
      OnPopupDataChanged(match.fill_into_edit,
                         /*is_temporary_text=*/true, std::u16string(), keyword,
                         keyword_placeholder, is_keyword_hint, std::u16string(),
                         match);
    }
  }
}

bool OmniboxEditModel::IsPopupSelectionOnInitialLine() const {
  DCHECK(popup_view_);
  size_t initial_line = autocomplete_controller()->result().default_match()
                            ? 0
                            : OmniboxPopupSelection::kNoMatch;
  return GetPopupSelection().line == initial_line;
}

bool OmniboxEditModel::IsPopupControlPresentOnMatch(
    OmniboxPopupSelection selection) const {
  DCHECK(popup_view_);
  return selection.IsControlPresentOnMatch(autocomplete_controller()->result());
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
  controller_->StopAutocomplete(/*clear_result=*/false);

  const AutocompleteMatch& match =
      autocomplete_controller()->result().match_at(line);
  if (match.SupportsDeletion()) {
    // Try to preserve the selection even after match deletion.
    size_t old_selected_line = GetPopupSelection().line;

    // This will synchronously notify both the edit and us that the results
    // have changed, causing both to revert to the default match.
    autocomplete_controller()->DeleteMatch(match);

    // Clamp the old selection to the new size of
    // autocomplete_controller()->result(), since there may be fewer results
    // now.
    if (old_selected_line != OmniboxPopupSelection::kNoMatch) {
      old_selected_line =
          std::min(line, autocomplete_controller()->result().size() - 1);
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

  const AutocompleteMatch& match =
      autocomplete_controller()->result().match_at(line);

  if (match.type == AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER) {
    // This match type is a special case that puts its primary meaningful
    // content (the answer) into the `description` and repurposes other fields.
    // So using `fill_into_edit` or `inline_autocompletion` or even just match
    // `contents` doesn't make sense in this case. Instead, we provide the
    // screen reader with the header ("Summary") and then the answer in
    // `description`, and finally the URL details in `contents` (includes date).
    return AutocompleteMatchType::ToAccessibilityLabel(
        match, GetSuggestionGroupHeaderText(match.suggestion_group_id),
        base::StrCat({
            match.history_embeddings_answer_header_text,
            match.description,
            match.contents,
        }),
        line, autocomplete_controller()->result().size(), u"",
        label_prefix_length);
  }

  int additional_message_id = 0;
  std::u16string additional_message;
  // This switch statement should be updated when new selection types are added.
  static_assert(OmniboxPopupSelection::LINE_STATE_MAX_VALUE == 8);
  switch (popup_selection_.state) {
    case OmniboxPopupSelection::NORMAL: {
      int available_actions_count = 0;
      if (line + 1 < autocomplete_controller()->result().size() &&
          autocomplete_controller()->result().match_at(line + 1).IsToolbelt()) {
        additional_message_id = IDS_ACC_OMNIBOX_TOOLBELT_NEXT_SUFFIX;
      }
      if (OmniboxPopupSelection(line, OmniboxPopupSelection::KEYWORD_MODE)
              .IsControlPresentOnMatch(autocomplete_controller()->result())) {
        additional_message_id = IDS_ACC_KEYWORD_SUFFIX;
        available_actions_count++;
      }
      if (OmniboxPopupSelection(line,
                                OmniboxPopupSelection::FOCUSED_BUTTON_ACTION)
              .IsControlPresentOnMatch(autocomplete_controller()->result())) {
        additional_message =
            match.GetActionAt(0u)->GetLabelStrings().accessibility_suffix;
        available_actions_count++;
      }
      if (OmniboxPopupSelection(line,
                                OmniboxPopupSelection::FOCUSED_BUTTON_THUMBS_UP)
              .IsControlPresentOnMatch(autocomplete_controller()->result())) {
        // No need to set `additional_message_id`. Thumbs up and thumbs down
        // button are always present together; `additional_message_id` is set to
        // `IDS_ACC_MULTIPLE_ACTIONS_SUFFIX` further down.
        available_actions_count++;
      }
      if (OmniboxPopupSelection(
              line, OmniboxPopupSelection::FOCUSED_BUTTON_THUMBS_DOWN)
              .IsControlPresentOnMatch(autocomplete_controller()->result())) {
        // No need to set `additional_message_id`. Thumbs up and thumbs down
        // button are always present together; `additional_message_id` is set to
        // `IDS_ACC_MULTIPLE_ACTIONS_SUFFIX` further down.
        available_actions_count++;
      }
      if (OmniboxPopupSelection(
              line, OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION)
              .IsControlPresentOnMatch(autocomplete_controller()->result())) {
        additional_message_id = IDS_ACC_REMOVE_SUGGESTION_SUFFIX;
        available_actions_count++;
      }
      if (available_actions_count > 1) {
        additional_message_id = IDS_ACC_MULTIPLE_ACTIONS_SUFFIX;
      }

      break;
    }
    case OmniboxPopupSelection::KEYWORD_MODE: {
      CHECK(!match.associated_keyword.empty());
      const TemplateURL* turl = AutocompleteMatch::GetTemplateURLWithKeyword(
          controller_->client()->GetTemplateURLService(),
          match.associated_keyword, "");
      std::u16string replacement_string =
          turl ? turl->short_name() : match.contents;
      bool ask_keyword = turl && turl->is_ask_starter_pack();
      // For featured search engines, we also want to add the shortcut name.
      if (AutocompleteMatch::IsFeaturedSearchType(match.type)) {
        int message_id = ask_keyword ? IDS_ACC_ASK_KEYWORD_MODE_WITH_SHORTCUT
                                     : IDS_ACC_KEYWORD_MODE_WITH_SHORTCUT;
        return l10n_util::GetStringFUTF16(message_id, match.keyword,
                                          replacement_string);
      }
      int message_id =
          ask_keyword ? IDS_ACC_ASK_KEYWORD_MODE : IDS_ACC_KEYWORD_MODE;
      return l10n_util::GetStringFUTF16(message_id, replacement_string);
    }
    case OmniboxPopupSelection::FOCUSED_BUTTON_ACTION: {
      // When pedal button is focused, the autocomplete suggestion isn't
      // read because it's not relevant to the button's action.
      // When dealing with toolbelt actions, we need to ensure that the proper
      // action a11y label is announced based on the action index.
      DCHECK(match.GetActionAt(popup_selection_.action_index));
      return match.GetActionAt(popup_selection_.action_index)
          ->GetLabelStrings()
          .accessibility_hint;
    }
    case OmniboxPopupSelection::FOCUSED_BUTTON_THUMBS_UP:
      additional_message_id = IDS_ACC_THUMBS_UP_SUGGESTION_FOCUSED_PREFIX;
      break;
    case OmniboxPopupSelection::FOCUSED_BUTTON_THUMBS_DOWN:
      additional_message_id = IDS_ACC_THUMBS_DOWN_SUGGESTION_FOCUSED_PREFIX;
      break;
    case OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      additional_message_id = match.IsIphSuggestion()
                                  ? IDS_ACC_DISMISS_CHROME_TIP_FOCUSED_PREFIX
                                  : IDS_ACC_REMOVE_SUGGESTION_FOCUSED_PREFIX;
      break;
    case OmniboxPopupSelection::FOCUSED_IPH_LINK:
      return base::StrCat(
          {match_text, u" ",
           AutocompleteMatchType::ToAccessibilityLabel(
               match, GetSuggestionGroupHeaderText(match.suggestion_group_id),
               match.iph_link_text, line, 0,
               l10n_util::GetStringUTF16(IDS_ACC_OMNIBOX_IPH_LINK_SELECTED),
               label_prefix_length)});
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
      include_positional_info ? autocomplete_controller()->result().size() : 0;

  // If there's a button focused, we don't want the "n of m" message announced.
  return AutocompleteMatchType::ToAccessibilityLabel(
      match, GetSuggestionGroupHeaderText(match.suggestion_group_id),
      match_text, line, total_matches, additional_message, label_prefix_length);
}

std::u16string OmniboxEditModel::GetPopupAccessibilityLabelForAimButton() {
  DCHECK(popup_selection_.state == OmniboxPopupSelection::FOCUSED_BUTTON_AIM);
  return l10n_util::GetStringUTF16(IDS_ACC_AI_MODE_BUTTON_FOCUSED);
}

std::u16string
OmniboxEditModel::MaybeGetPopupAccessibilityLabelForIPHSuggestion() {
  DCHECK(popup_view_);
  DCHECK_NE(popup_selection_.line, OmniboxPopupSelection::kNoMatch)
      << "MaybeGetPopupAccessibilityLabelForIPHSuggestion should never be "
         "called "
         "if the current selection is kNoMatch.";

  std::u16string label = u"";
  size_t next_line = popup_selection_.line + 1;
  if (next_line < autocomplete_controller()->result().size()) {
    const AutocompleteMatch& next_match =
        autocomplete_controller()->result().match_at(next_line);
    if (next_match.IsIphSuggestion()) {
      label =
          l10n_util::GetStringFUTF16(IDS_ACC_CHROME_TIP, next_match.contents);

      // Iff the next selection (the next time the user presses tab) is the
      // remove suggestion button for the IPH row, also append its a11y label.
      auto next_selection = popup_selection_.GetNextSelection(
          autocomplete_controller()->input(),
          autocomplete_controller()->result(),
          controller_->client()->GetTemplateURLService(),
          view_->AimButtonVisible(), OmniboxPopupSelection::kForward,
          OmniboxPopupSelection::kStateOrLine);
      if (next_selection.line == next_line &&
          next_selection.state ==
              OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION) {
        label = l10n_util::GetStringFUTF16(IDS_ACC_DISMISS_CHROME_TIP_SUFFIX,
                                           label);
      }
    }
  }

  return label;
}

void OmniboxEditModel::OnPopupResultChanged() {
  if (!popup_view_) {
    return;
  }
  rich_suggestion_bitmaps_.clear();
  const AutocompleteResult& result = autocomplete_controller()->result();

  // Reset selection.
  const OmniboxPopupSelection old_selection = popup_selection_;
  popup_selection_ = OmniboxPopupSelection(
      result.default_match() ? 0 : OmniboxPopupSelection::kNoMatch,
      OmniboxPopupSelection::NORMAL);

  // If the AI button was previously focused and the selection state changed,
  // remove the focus ring from the AI mode button.
  if (old_selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_AIM &&
      popup_selection_.state != OmniboxPopupSelection::FOCUSED_BUTTON_AIM) {
    view_->ApplyFocusRingToAimButton(false);
  }
  observers_.Notify(&Observer::OnContentsChanged);
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

const SkBitmap* OmniboxEditModel::GetPopupRichSuggestionBitmap(
    const GURL& image_url) const {
  DCHECK(popup_view_);
  auto iter =
      std::ranges::find_if(autocomplete_controller()->result(),
                           [&image_url](const AutocompleteMatch& result_match) {
                             return (!result_match.ImageUrl().is_empty() &&
                                     result_match.ImageUrl() == image_url);
                           });
  return iter == autocomplete_controller()->result().end()
             ? nullptr
             : GetPopupRichSuggestionBitmap(std::distance(
                   autocomplete_controller()->result().begin(), iter));
}

const SkBitmap* OmniboxEditModel::GetIconBitmap(const GURL& icon_url) const {
  DCHECK(popup_view_);
  auto iter = icon_bitmaps_.find(icon_url);
  if (iter == icon_bitmaps_.end()) {
    return nullptr;
  }
  return &iter->second;
}

void OmniboxEditModel::SetPopupRichSuggestionBitmap(int result_index,
                                                    const SkBitmap& bitmap) {
  DCHECK(popup_view_);
  rich_suggestion_bitmaps_[result_index] = bitmap;
  observers_.Notify(&Observer::OnContentsChanged);
}

void OmniboxEditModel::SetIconBitmap(const GURL& icon_url,
                                     const SkBitmap& bitmap) {
  DCHECK(popup_view_ && !icon_url.is_empty());
  icon_bitmaps_[icon_url] = bitmap;
  observers_.Notify(&Observer::OnContentsChanged);
}

void OmniboxEditModel::SetAutocompleteInput(AutocompleteInput input) {
  input_ = std::move(input);
}

PrefService* OmniboxEditModel::GetPrefService() {
  return controller_->client()->GetPrefs();
}

const PrefService* OmniboxEditModel::GetPrefService() const {
  return controller_->client()->GetPrefs();
}

AutocompleteController* OmniboxEditModel::autocomplete_controller() const {
  return controller_->autocomplete_controller();
}

bool OmniboxEditModel::MaybeStartQueryForPopup() {
  if (controller_->IsPopupOpen() || !autocomplete_controller()->done()) {
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
  if (!user_input_in_progress_) {
    InternalSetUserText(url_for_editing_);
  }
  if (view_) {
    view_->UpdatePopup();
  }
  return true;
}

void OmniboxEditModel::StepPopupSelection(
    OmniboxPopupSelection::Direction direction,
    OmniboxPopupSelection::Step step) {
  // NOTE: This purposefully doesn't trigger any code that resets
  // `paste_state_`.

  // The popup could be working on a query but is not open. In that case, force
  // it to open immediately.
  if (MaybeStartQueryForPopup() || !controller_->IsPopupOpen()) {
    return;
  }

  // The popup is open, so the user should be able to interact with it normally.

  // This block steps the popup selection, with special consideration for
  // existing keyword logic in the edit model, where `ClearKeyword()` must be
  // called before changing the selected line. `AcceptKeyword()` should be
  // called after changing the selected line so we don't accept keyword on the
  // wrong suggestion when stepping backwards.
  const OmniboxPopupSelection old_selection = GetPopupSelection();
  OmniboxPopupSelection new_selection = old_selection.GetNextSelection(
      autocomplete_controller()->input(), autocomplete_controller()->result(),
      controller_->client()->GetTemplateURLService(), view_->AimButtonVisible(),
      direction, step);
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
    // If, as a result of the key press, we would select the first result,
    // then we should revert the temporary text same as what pressing escape
    // would have done.
    //
    // Reverting, however, does not make sense for on-focus suggestions
    // (`user_input_in_progress_` is false) unless the first result is a
    // verbatim match of the omnibox input (on-focus query refinements on
    // SERP).
    if (autocomplete_controller()->result().default_match() &&
        has_temporary_text_ && new_selection == OmniboxPopupSelection(0) &&
        (user_input_in_progress_ || autocomplete_controller()
                                        ->result()
                                        .default_match()
                                        ->IsVerbatimType())) {
      RevertTemporaryTextAndPopup();
    } else {
      SetPopupSelection(new_selection);
    }
  }

  // `AcceptKeyword()` above updates the popup for the case where we step into
  // keyword mode with extra user text, e.g. user input is "youtube.com query"
  // then press tab to enter keyword mode. This is to ensure that the user text
  // is populated accordingly and the new default suggestion is immediately
  // clickable. In this case, the current selection (the clickable keyword
  // suggestion) will not match the expected "next" suggestion (the keyword
  // button focused).
  if (new_selection.state != OmniboxPopupSelection::KEYWORD_MODE ||
      user_text_.empty()) {
    DCHECK(popup_selection_ == new_selection);
  }

  // Inform the client that a new row is now selected.
  OnNavigationLikely(popup_selection_.line,
                     NavigationPredictor::kUpOrDownArrowButton);
}

void OmniboxEditModel::AcceptInput(WindowOpenDisposition disposition,
                                   base::TimeTicks match_selection_timestamp) {
  // Get the URL and transition type for the selected entry.
  GURL alternate_nav_url;
  AutocompleteMatch match = CurrentMatchAndAlternateNavUrl(&alternate_nav_url);

  // If CTRL is down it means the user wants to append ".com" to the text they
  // typed. If we can successfully generate a URL_WHAT_YOU_TYPED match doing
  // that, then we use this. These matches are marked as generated by the
  // HistoryURLProvider so we only generate them if this provider is present.
  bool accept_via_control_enter =
      control_key_state_ == ControlKeyState::kDown && !is_keyword_selected() &&
      autocomplete_controller()->history_url_provider();
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
        autocomplete_controller()->history_url_provider(),
        autocomplete_controller()->autocomplete_provider_client(), input_,
        input_.canonicalized_url(), false));

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

  if (paste_state_ != PasteState::kNone &&
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
  // When the toolbelt is enabled, a non-selectable match may still have
  // selectable actions on it, and these can still be executed.
  if (match.type == AutocompleteMatchType::NULL_RESULT_MESSAGE &&
      (!omnibox_feature_configs::Toolbelt::Get().enabled || !action)) {
    return;
  }

  // Switch the window disposition to SWITCH_TO_TAB for open tab matches that
  // originated while in keyword mode.  This is to support the keyword mode
  // starter pack's tab search (@tabs) feature, which should open all
  // suggestions in the existing open tab.
  bool is_open_tab_match =
      match.from_keyword &&
      match.provider->type() == AutocompleteProvider::TYPE_OPEN_TAB;
  // Also switch the window disposition for tab switch actions. The action
  // itself will already open with SWITCH_TO_TAB disposition, but the change
  // is needed earlier for metrics.
  bool is_tab_switch_action =
      action && action->ActionId() == OmniboxActionId::TAB_SWITCH;
  if (is_open_tab_match || is_tab_switch_action) {
    disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  }

  TRACE_EVENT("omnibox", "OmniboxEditModel::OpenMatch", "match", match,
              "disposition", disposition, "altenate_nav_url", alternate_nav_url,
              "pasted_text", pasted_text);
  const base::TimeTicks& now(base::TimeTicks::Now());
  base::TimeDelta elapsed_time_since_user_first_modified_omnibox(
      now - time_user_first_modified_omnibox_);
  autocomplete_controller()
      ->UpdateMatchDestinationURLWithAdditionalSearchboxStats(
          elapsed_time_since_user_first_modified_omnibox, &match);

  GURL destination_url = action ? action->getUrl() : match.destination_url;

  // Save the result of the interaction, but do not record the histogram yet.
  focus_resulted_in_navigation_ = true;

  omnibox::RecordActionShownForAllActions(autocomplete_controller()->result(),
                                          selection);
  HistoryFuzzyProvider::RecordOpenMatchMetrics(
      autocomplete_controller()->result(), match);

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
      now - autocomplete_controller()->last_time_default_match_changed());
  DCHECK(match.provider);
  // These elapsed times don't really make sense for matches that come from
  // omnibox focus (because the user did not modify the omnibox), so for those
  // we set the elapsed times to something that will be ignored by
  // metrics_log.cc.  They also don't necessarily make sense if the omnibox
  // dropdown is closed or the user used paste-and-go.  (In most
  // cases when this happens, the user never modified the omnibox.)
  const bool popup_open = controller_->IsPopupOpen();
  const base::TimeDelta default_time_delta = base::Milliseconds(-1);
  if (input_.IsZeroSuggest() || !pasted_text.empty()) {
    elapsed_time_since_user_first_modified_omnibox = default_time_delta;
    elapsed_time_since_last_change_to_default_match = default_time_delta;
  }

  base::TimeDelta elapsed_time_since_user_focused_omnibox = default_time_delta;
  if (!last_omnibox_focus_.is_null()) {
    elapsed_time_since_user_focused_omnibox = now - last_omnibox_focus_;
    // Only record focus to open time when a focus actually happened (as
    // opposed to, say, dragging a link onto the omnibox).
    omnibox::LogFocusToOpenTime(
        elapsed_time_since_user_focused_omnibox, input_.IsZeroSuggest(),
        GetPageClassification(), match,
        selection.IsAction() ? selection.action_index : -1);
  }

  // In some unusual cases, we ignore autocomplete_controller()->result() and
  // instead log a fake result set with a single element (|match|) and
  // selected_index of 0. For these cases:
  //  1. If the popup is closed (there is no result set). This doesn't apply
  //  for WebUI searchboxes since they don't have an associated popup.
  //  2. If the index is out of bounds. This should only happen if
  //  |selection.line| is
  //     kNoMatch, which can happen if the default search provider is disabled.
  //  3. If this is paste-and-go (meaning the contents of the dropdown
  //     are ignored regardless).
  const bool dropdown_ignored =
      (!popup_open && !omnibox::IsWebUISearchbox(GetPageClassification())) ||
      selection.line >= autocomplete_controller()->result().size() ||
      !pasted_text.empty();
  ACMatches fake_single_entry_matches;
  fake_single_entry_matches.push_back(match);
  AutocompleteResult fake_single_entry_result;
  fake_single_entry_result.AppendMatches(fake_single_entry_matches);

  std::u16string user_text =
      input_.IsZeroSuggest() ? std::u16string() : input_text;
  size_t completed_length = match.allowed_to_be_default_match
                                ? match.inline_autocompletion.length()
                                : std::u16string::npos;
  bool is_incognito = autocomplete_controller()
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
      dropdown_ignored ? fake_single_entry_result
                       : autocomplete_controller()->result(),
      destination_url, is_incognito, input_.IsZeroSuggest(), match.session);
  DCHECK(dropdown_ignored ||
         (log.elapsed_time_since_user_first_modified_omnibox >=
          log.elapsed_time_since_last_change_to_default_match))
      << "We should've got the notification that the user modified the "
      << "omnibox text at same time or before the most recent time the "
      << "default match changed.";
  log.elapsed_time_since_user_focused_omnibox =
      elapsed_time_since_user_focused_omnibox;
  log.ukm_source_id = controller_->client()->GetUKMSourceId();

  if ((disposition == WindowOpenDisposition::CURRENT_TAB) &&
      controller_->client()->CurrentPageExists()) {
    // If we know the destination is being opened in the current tab,
    // we can easily get the tab ID.  (If it's being opened in a new
    // tab, we don't know the tab ID yet.)
    log.tab_id = controller_->client()->GetSessionID();
  }
  autocomplete_controller()->AddProviderAndTriggeringLogs(&log);

  base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.RichAutocompletion",
                                match.rich_autocompletion_triggered);

  omnibox::LogIPv4PartsCount(user_text, destination_url, completed_length);

  controller_->client()->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);
  omnibox::answer_data_parser::LogAnswerUsed(match.answer_type);

  TemplateURLService* service = controller_->client()->GetTemplateURLService();
  TemplateURL* template_url = match.GetTemplateURL(service);
  if (template_url) {
    // |match| is a Search navigation or a URL navigation in keyword mode; log
    // search engine usage metrics.
    AutocompleteMatch::LogSearchEngineUsed(match, service);

    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_KEYWORD) ||
        match.provider->type() ==
            AutocompleteProvider::TYPE_UNSCOPED_EXTENSION) {
      // User is in keyword mode or accepted an unscoped extension suggestion,
      // increment usage count for the keyword.
      base::RecordAction(base::UserMetricsAction("AcceptedKeyword"));
      EmitAcceptedKeywordSuggestionHistogram(keyword_mode_entry_method_,
                                             template_url);
      controller_->client()->GetTemplateURLService()->IncrementUsageCount(
          template_url);

      // Notify the extension of the selected input, but ignore if the selection
      // corresponds to an action created by an extension in unscoped mode.
      if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION &&
          !action) {
        controller_->client()->ProcessExtensionMatch(input_text, template_url,
                                                     match, disposition);
        if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB && view_) {
          base::AutoReset<bool> tmp(&in_revert_, true);
          view_->RevertAll();
        }
        // Avoid calling `OmniboxClient::OnAutocompleteAccept()`. The extension
        // was notfied of the accepted input and will handle the navigation.
        return;
      }
    } else {
      DCHECK(ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_GENERATED) ||
             ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_RELOAD));
      // NOTE: We purposefully don't increment the usage count of the default
      // search engine here like we do for explicit keywords above; see comments
      // in template_url.h.
    }
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
    }
  }

  TemplateURLService* template_url_service =
      controller_->client()->GetTemplateURLService();
  if (action) {
    OmniboxAction::ExecutionContext context(
        *(autocomplete_controller()->autocomplete_provider_client()),
        base::BindOnce(&OmniboxClient::OnAutocompleteAccept,
                       controller_->client()->AsWeakPtr()),
        match_selection_timestamp, disposition);
    base::UmaHistogramMicrosecondsTimes(
        "Omnibox.InputToExecuteAction",
        base::TimeTicks::Now() - match_selection_timestamp);
    action->Execute(context);
    if (context.enter_starter_pack_id_ != 0 && template_url_service) {
      if (const TemplateURL* starter_pack_turl =
              template_url_service->FindStarterPackTemplateURL(
                  context.enter_starter_pack_id_)) {
        EnterKeywordMode(
            OmniboxEventProto::TOOLBELT, starter_pack_turl,
            AutocompleteMatch::GetKeywordPlaceholder(
                starter_pack_turl,
                controller_->client()->IsHistoryEmbeddingsEnabled()));
        return;
      }
    }
  }

  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB && view_) {
    base::AutoReset<bool> tmp(&in_revert_, true);
    view_->RevertAll();  // Revert the box to its unedited state.
  }

  if (!action) {
    // Track whether the destination URL sends us to a search results page
    // using the default search provider.
    if (template_url_service &&
        template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
            match.destination_url)) {
      base::RecordAction(
          base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
      base::UmaHistogramBoolean("Omnibox.Search.OffTheRecord", is_incognito);
    }

    BookmarkModel* bookmark_model = controller_->client()->GetBookmarkModel();
    if (bookmark_model && bookmark_model->IsBookmarked(destination_url)) {
      controller_->client()->OnBookmarkLaunched();
    }

    // This block should be the last call in OpenMatch, because controller_ is
    // not guaranteed to exist after client()->OnAutocompleteAccept is called.
    if (destination_url.is_valid()) {
      // This calls RevertAll again.
      base::AutoReset<bool> tmp(&in_revert_, true);

      base::UmaHistogramMicrosecondsTimes(
          "Omnibox.InputToAcceptNonAction",
          base::TimeTicks::Now() - match_selection_timestamp);
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
              autocomplete_controller()->history_url_provider(),
              autocomplete_controller()->autocomplete_provider_client(),
              alternate_input, alternate_nav_url, false));
    }
  }
}

void OmniboxEditModel::UpdateFeedbackOnMatch(size_t match_index,
                                             FeedbackType feedback_type) {
  if (match_index == OmniboxPopupSelection::kNoMatch ||
      match_index >= autocomplete_controller()->result().size()) {
    return;
  }
  auto& match = const_cast<AutocompleteMatch&>(
      autocomplete_controller()->result().match_at(match_index));

  // Set the feedback or reset it if the same feedback is given.
  match.feedback_type = match.feedback_type == feedback_type
                            ? FeedbackType::kNone
                            : feedback_type;
  // Update the suggestion appearance.
  observers_.Notify(&Observer::OnContentsChanged);
  // Show the feedback form on negative feedback.
  if (match.feedback_type == FeedbackType::kThumbsDown) {
    controller_->client()->ShowFeedbackPage(
        autocomplete_controller()->input().text(), match.destination_url);
  }
}

bool OmniboxEditModel::AllowKeywordSpaceTriggering() const {
  return GetPrefService()->GetBoolean(omnibox::kKeywordSpaceTriggeringEnabled);
}

bool OmniboxEditModel::ShouldAcceptKeywordAfterInsertingSpaceAtEnd(
    const std::u16string& new_text) {
  // Check if the user has disabled space triggering.
  if (!AllowKeywordSpaceTriggering()) {
    return false;
  }

  // Check a keyword hint was being shown. If the text matches a keyword, a hint
  // would have been shown. Even if this weren't the case, and the input matched
  // a keyword without showing a hint, entering keyword mode in this case would
  // be surprising.
  if (!is_keyword_hint_) {
    return false;
  }

  // Pasting a space shouldn't enter keyword mode. This isn't strictly necessary
  // because `OnAfterPossibleChange()`, the only caller of
  // `ShouldAcceptKeywordAfterInsertingSpaceAtEnd()`, doesn't make the call if
  // there was a paste. But it'd be fragile to rely on that logic.
  if (paste_state_ != PasteState::kNone) {
    return false;
  }

  // `input` must end with space. Typing 'youtube' shouldn't enter
  // keyword mode until the user types a final space.
  if (!IsSpaceCharForAcceptingKeyword(new_text[new_text.length() - 1])) {
    return false;
  }

  // Check the rest of the input matches `keyword`. This isn't necessary,
  // `AutocompleteController` shouldn't show keyword hints on the default match
  // for inputs like 'yout '. But that's not a guarantee.
  if (new_text.substr(0, new_text.length() - 1) != keyword_) {
    return false;
  }

  return true;
}

bool OmniboxEditModel::ShouldAcceptKeywordAfterInsertingSpaceInMiddle(
    std::u16string_view old_text,
    std::u16string_view new_text,
    size_t caret_position) const {
  DCHECK_GE(new_text.length(), caret_position);
  // Check if the user has disabled space triggering.
  if (!AllowKeywordSpaceTriggering()) {
    return false;
  }

  // Unlike `ShouldAcceptKeywordAfterInsertingSpaceAtEnd()`, don't check a
  // keyword hint was being shown. The input may have been 'youtube|query',
  // which won't show a keyword hint, but space should still enter keyword mode.

  // Pasting a space shouldn't enter keyword mode.
  if (paste_state_ != PasteState::kNone) {
    return false;
  }

  // Check a space was inserted in the middle of the input text. E.g.
  // - 'youtube |query'  -> valid
  // - ' |youtube'       -> invalid
  // - 'youtube |'       -> invalid
  // - 'youtube  |query' -> invalid
  // Some of these are redundant with other checks below.
  size_t space_position = caret_position - 1;
  if (caret_position < 2 || old_text.length() < caret_position ||
      new_text.length() == caret_position ||
      !IsSpaceCharForAcceptingKeyword(new_text[space_position]) ||
      base::IsUnicodeWhitespace(new_text[space_position - 1])) {
    return false;
  }

  // If the text preceding the space changed, then it's not a simple space
  // insertion.
  if (old_text.substr(0, space_position) !=
      new_text.substr(0, space_position)) {
    return false;
  }

  // Check if  the text was unchanged. E.g. old text was 'youtube[ ]query' and
  // the user replaced the selected space with another space.
  if (old_text == new_text) {
    return false;
  }

  // Check there aren't multiple words preceding the space. E.g.
  // 'youtube google |query' shouldn't accept the 'youtube' keyword.
  if (new_text.substr(0, space_position)
          .find_first_of(base::kWhitespaceUTF16) != std::u16string_view::npos) {
    return false;
  }

  return true;
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
  if (user_input_in_progress_ == in_progress) {
    return false;
  }

  user_input_in_progress_ = in_progress;
  if (user_input_in_progress_) {
    time_user_first_modified_omnibox_ = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction("OmniboxInputInProgress"));
    autocomplete_controller()->ResetSession();
  }
  return true;
}

void OmniboxEditModel::NotifyObserversInputInProgress(bool in_progress) {
  controller_->client()->OnInputInProgress(in_progress);

  if (user_input_in_progress_ || !in_revert_) {
    controller_->client()->OnInputStateChanged();
  }
}

void OmniboxEditModel::SetFocusState(OmniboxFocusState state,
                                     OmniboxFocusChangeReason reason) {
  if (state == focus_state_) {
    return;
  }

  // Update state and notify view if the omnibox caret visibility changed.
  const bool was_caret_visible = is_caret_visible();
  focus_state_ = state;
  if (is_caret_visible() != was_caret_visible && view_) {
    view_->ApplyCaretVisibility();
  }

  controller_->client()->OnFocusChanged(focus_state_, reason);
}

void OmniboxEditModel::OnFaviconFetched(const GURL& page_url,
                                        const gfx::Image& icon) const {
  if (icon.IsEmpty() || !controller_->IsPopupOpen()) {
    return;
  }

  // Notify all affected matches.
  for (size_t i = 0; i < autocomplete_controller()->result().size(); ++i) {
    auto& match = autocomplete_controller()->result().match_at(i);
    if (!AutocompleteMatch::IsSearchType(match.type) &&
        match.destination_url == page_url) {
      observers_.Notify(&Observer::OnMatchIconUpdated, i);
    }
  }
}

std::u16string OmniboxEditModel::GetText() const {
  // Once the model owns primary text, the check for `view_` won't be needed.
  if (view_) {
    return view_->GetText();
  } else {
    NOTREACHED();
  }
}

void OmniboxEditModel::SetKeyword(const std::u16string& keyword) {
  keyword_ = keyword;
}

void OmniboxEditModel::SetKeywordPlaceholder(
    const std::u16string& keyword_placeholder) {
  keyword_placeholder_ = keyword_placeholder;
}

void OmniboxEditModel::SetIsKeywordHint(bool is_keyword_hint) {
  is_keyword_hint_ = is_keyword_hint;
}

void OmniboxEditModel::RecordAiModeMetrics(const std::u16string& query_text,
                                           bool activated,
                                           bool via_keyboard) {
  const auto* triggered_feature_service =
      autocomplete_controller()
          ->autocomplete_provider_client()
          ->GetOmniboxTriggeredFeatureService();
  const std::string page_context =
      ::metrics::OmniboxEventProto::PageClassification_Name(
          GetPageClassification());

  // Record whether or not the AIM page action was shown during the session.
  const bool shown_in_session =
      triggered_feature_service->GetFeatureTriggeredInSession(
          metrics::OmniboxEventProto_Feature::
              OmniboxEventProto_Feature_AIM_PAGE_ACTION_OMNIBOX_ENTRYPOINT);
  base::UmaHistogramBoolean(kOmniboxAimEntrypointShown, shown_in_session);
  base::UmaHistogramBoolean(base::StrCat({kOmniboxAimEntrypointShown,
                                          ".ByPageContext.", page_context}),
                            shown_in_session);

  if (!activated) {
    return;
  }

  // Record whether or not the AIM page action was activated with non-empty
  // query text.
  base::UmaHistogramBoolean(kOmniboxAimEntrypointActivatedUserTextPresent,
                            !query_text.empty());
  base::UmaHistogramBoolean(
      base::StrCat({kOmniboxAimEntrypointActivatedUserTextPresent,
                    ".ByPageContext.", page_context}),
      !query_text.empty());

  // Record the entry method used to activate the AIM page action.
  base::UmaHistogramBoolean(kOmniboxAimEntrypointActivatedViaKeyboard,
                            via_keyboard);
  base::UmaHistogramBoolean(
      base::StrCat({kOmniboxAimEntrypointActivatedViaKeyboard,
                    ".ByPageContext.", page_context}),
      via_keyboard);
}
