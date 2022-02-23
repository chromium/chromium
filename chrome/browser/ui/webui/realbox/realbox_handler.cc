// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/realbox/realbox_handler.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/webui/realbox/realbox.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/cookies/cookie_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/resources/grit/webui_generated_resources.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/app/vector_icons/vector_icons.h"
#endif

namespace {

constexpr char kSearchIconResourceName[] = "search.svg";

constexpr char kAnswerCurrencyIconResourceName[] = "realbox/icons/currency.svg";
constexpr char kAnswerDefaultIconResourceName[] = "realbox/icons/default.svg";
constexpr char kAnswerDictionaryIconResourceName[] =
    "realbox/icons/definition.svg";
constexpr char kAnswerFinanceIconResourceName[] = "realbox/icons/finance.svg";
constexpr char kAnswerSunriseIconResourceName[] = "realbox/icons/sunrise.svg";
constexpr char kAnswerTranslationIconResourceName[] =
    "realbox/icons/translation.svg";
constexpr char kAnswerWhenIsIconResourceName[] = "realbox/icons/when_is.svg";
constexpr char kBookmarkIconResourceName[] =
    "chrome://resources/images/icon_bookmark.svg";
constexpr char kCalculatorIconResourceName[] = "realbox/icons/calculator.svg";
constexpr char kClockIconResourceName[] =
    "chrome://resources/images/icon_clock.svg";
constexpr char kDinoIconResourceName[] = "realbox/icons/dino.svg";
constexpr char kDriveDocsIconResourceName[] = "realbox/icons/drive_docs.svg";
constexpr char kDriveFolderIconResourceName[] =
    "realbox/icons/drive_folder.svg";
constexpr char kDriveFormIconResourceName[] = "realbox/icons/drive_form.svg";
constexpr char kDriveImageIconResourceName[] = "realbox/icons/drive_image.svg";
constexpr char kDriveLogoIconResourceName[] = "icons/drive_logo.svg";
constexpr char kDrivePdfIconResourceName[] = "realbox/icons/drive_pdf.svg";
constexpr char kDriveSheetsIconResourceName[] =
    "realbox/icons/drive_sheets.svg";
constexpr char kDriveSlidesIconResourceName[] =
    "realbox/icons/drive_slides.svg";
constexpr char kDriveVideoIconResourceName[] = "realbox/icons/drive_video.svg";
constexpr char kExtensionAppIconResourceName[] =
    "realbox/icons/extension_app.svg";
constexpr char kGoogleGIconResourceName[] = "realbox/icons/google_g.svg";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kGoogleCalendarIconResourceName[] = "realbox/icons/calendar.svg";
constexpr char kGoogleKeepNoteIconResourceName[] = "realbox/icons/note.svg";
constexpr char kGoogleSitesIconResourceName[] = "realbox/icons/sites.svg";
#endif
constexpr char kIncognitoIconResourceName[] = "realbox/icons/incognito.svg";
constexpr char kPageIconResourceName[] = "realbox/icons/page.svg";
constexpr char kPedalsIconResourceName[] =
    "chrome://theme/current-channel-logo";
constexpr char kTrendingUpIconResourceName[] = "realbox/icons/trending_up.svg";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kCrosShareIconResourceName[] = "realbox/icons/cros_share.svg";
#elif BUILDFLAG(IS_MAC)
constexpr char kMacShareIconResourceName[] = "realbox/icons/mac_share.svg";
#elif BUILDFLAG(IS_WIN)
constexpr char kWinShareIconResourceName[] = "realbox/icons/win_share.svg";
#else
constexpr char kLinuxShareIconResourceName[] = "realbox/icons/linux_share.svg";
#endif

base::flat_map<int32_t, realbox::mojom::SuggestionGroupPtr>
CreateSuggestionGroupsMap(
    const AutocompleteResult& result,
    PrefService* prefs,
    const SearchSuggestionParser::HeadersMap& headers_map) {
  base::flat_map<int32_t, realbox::mojom::SuggestionGroupPtr> result_map;
  for (const auto& pair : headers_map) {
    realbox::mojom::SuggestionGroupPtr suggestion_group =
        realbox::mojom::SuggestionGroup::New();
    suggestion_group->header = pair.second;
    suggestion_group->hidden =
        result.IsSuggestionGroupIdHidden(prefs, pair.first);
    suggestion_group->show_group_a11y_label =
        l10n_util::GetStringFUTF16(IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON,
                                   result.GetHeaderForGroupId(pair.first));
    suggestion_group->hide_group_a11y_label =
        l10n_util::GetStringFUTF16(IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON,
                                   result.GetHeaderForGroupId(pair.first));

    result_map.emplace(pair.first, std::move(suggestion_group));
  }
  return result_map;
}

absl::optional<std::u16string> GetAdditionalText(
    const SuggestionAnswer::ImageLine& line) {
  if (line.additional_text()) {
    const auto additional_text = line.additional_text()->text();
    if (!additional_text.empty())
      return additional_text;
  }
  return absl::nullopt;
}

std::u16string ImageLineToString16(const SuggestionAnswer::ImageLine& line) {
  std::vector<std::u16string> text;
  for (const auto& text_field : line.text_fields()) {
    text.push_back(text_field.text());
  }
  const auto& additional_text = GetAdditionalText(line);
  if (additional_text) {
    text.push_back(additional_text.value());
  }
  // TODO(crbug.com/1130372): Use placeholders or a l10n-friendly way to
  // construct this string instead of concatenation. This currently only happens
  // for stock ticker symbols.
  return base::JoinString(text, u" ");
}

std::u16string GetAdditionalA11yMessage(const AutocompleteMatch& match,
                                        RealboxHandler::FocusState state) {
  switch (state) {
    case RealboxHandler::FocusState::kFocusedMatch: {
      if (match.action) {
        return match.action->GetLabelStrings().accessibility_suffix;
      }
      if (match.SupportsDeletion()) {
        return l10n_util::GetStringUTF16(IDS_ACC_REMOVE_SUGGESTION_SUFFIX);
      }
      break;
    }
    case RealboxHandler::FocusState::kFocusedButtonRemoveSuggestion:
      return l10n_util::GetStringUTF16(
          IDS_ACC_REMOVE_SUGGESTION_FOCUSED_PREFIX);
    default:
      NOTREACHED();
      break;
  }
  return std::u16string();
}

std::vector<realbox::mojom::AutocompleteMatchPtr> CreateAutocompleteMatches(
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model) {
  std::vector<realbox::mojom::AutocompleteMatchPtr> matches;
  int line = 0;
  for (const AutocompleteMatch& match : result) {
    realbox::mojom::AutocompleteMatchPtr mojom_match =
        realbox::mojom::AutocompleteMatch::New();
    mojom_match->allowed_to_be_default_match =
        match.allowed_to_be_default_match;
    mojom_match->contents = match.contents;
    for (const auto& contents_class : match.contents_class) {
      mojom_match->contents_class.push_back(
          realbox::mojom::ACMatchClassification::New(contents_class.offset,
                                                     contents_class.style));
    }
    mojom_match->description = match.description;
    for (const auto& description_class : match.description_class) {
      mojom_match->description_class.push_back(
          realbox::mojom::ACMatchClassification::New(description_class.offset,
                                                     description_class.style));
    }
    mojom_match->destination_url = match.destination_url;
    mojom_match->suggestion_group_id = match.suggestion_group_id.value_or(
        SearchSuggestionParser::kNoSuggestionGroupId);
    const bool is_bookmarked =
        bookmark_model->IsBookmarked(match.destination_url);
    mojom_match->icon_url =
        RealboxHandler::AutocompleteMatchVectorIconToResourceName(
            match.GetVectorIcon(is_bookmarked));
    mojom_match->image_dominant_color = match.image_dominant_color;
    mojom_match->image_url = match.image_url.spec();
    mojom_match->fill_into_edit = match.fill_into_edit;
    mojom_match->inline_autocompletion = match.inline_autocompletion;
    mojom_match->is_search_type = AutocompleteMatch::IsSearchType(match.type);
    mojom_match->swap_contents_and_description =
        match.swap_contents_and_description;
    mojom_match->type = AutocompleteMatchType::ToString(match.type);
    mojom_match->supports_deletion = match.SupportsDeletion();
    if (match.answer.has_value() &&
        base::FeatureList::IsEnabled(omnibox::kNtpRealboxSuggestionAnswers)) {
      const auto& additional_text =
          GetAdditionalText(match.answer->first_line());
      mojom_match->answer = realbox::mojom::SuggestionAnswer::New(
          additional_text ? base::JoinString(
                                {match.contents, additional_text.value()}, u" ")
                          : match.contents,
          ImageLineToString16(match.answer->second_line()));
      mojom_match->image_url = match.ImageUrl().spec();
    }
    mojom_match->is_rich_suggestion =
        !mojom_match->image_url.empty() ||
        match.type == AutocompleteMatchType::CALCULATOR ||
        (match.answer.has_value() &&
         base::FeatureList::IsEnabled(omnibox::kNtpRealboxSuggestionAnswers));
    const bool has_action = match.action && base::FeatureList::IsEnabled(
                                                omnibox::kNtpRealboxPedals);
    if (has_action) {
      mojom_match->action = realbox::mojom::Action::New(
          match.action->GetLabelStrings().accessibility_hint,
          match.action->GetLabelStrings().hint,
          match.action->GetLabelStrings().suggestion_contents,
          RealboxHandler::PedalVectorIconToResourceName(
              match.action->GetVectorIcon()));
    }
    mojom_match->a11y_label = AutocompleteMatchType::ToAccessibilityLabel(
        match, match.contents, line, 0,
        GetAdditionalA11yMessage(match,
                                 RealboxHandler::FocusState::kFocusedMatch));

    mojom_match->remove_button_a11y_label =
        AutocompleteMatchType::ToAccessibilityLabel(
            match, match.contents, line, 0,
            GetAdditionalA11yMessage(
                match,
                RealboxHandler::FocusState::kFocusedButtonRemoveSuggestion));

    mojom_match->tail_suggest_common_prefix = match.tail_suggest_common_prefix;

    matches.push_back(std::move(mojom_match));
    line++;
  }
  return matches;
}

realbox::mojom::AutocompleteResultPtr CreateAutocompleteResult(
    const std::u16string& input,
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model,
    PrefService* prefs) {
  return realbox::mojom::AutocompleteResult::New(
      input, CreateSuggestionGroupsMap(result, prefs, result.headers_map()),
      CreateAutocompleteMatches(result, bookmark_model));
}

}  // namespace

// static
void RealboxHandler::SetupWebUIDataSource(content::WebUIDataSource* source) {
  static constexpr webui::ResourcePath kImages[] = {
      {kSearchIconResourceName, IDR_WEBUI_IMAGES_ICON_SEARCH_SVG}};
  source->AddResourcePaths(kImages);

  static constexpr webui::LocalizedString kStrings[] = {
      {"searchBoxHint", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD},
      {"realboxSeparator", IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR},
      {"removeSuggestion", IDS_OMNIBOX_REMOVE_SUGGESTION},
      {"hideSuggestions", IDS_TOOLTIP_HEADER_HIDE_SUGGESTIONS_BUTTON},
      {"showSuggestions", IDS_TOOLTIP_HEADER_SHOW_SUGGESTIONS_BUTTON}};
  source->AddLocalizedStrings(kStrings);

  source->AddInteger(
      "realboxMatchOmniboxThemeVariant",
      base::GetFieldTrialParamByFeatureAsInt(
          ntp_features::kRealboxMatchOmniboxTheme,
          ntp_features::kRealboxMatchOmniboxThemeVariantParam, 0));
  source->AddBoolean(
      "realboxMatchOmniboxTheme",
      base::FeatureList::IsEnabled(ntp_features::kRealboxMatchOmniboxTheme));
  source->AddBoolean(
      "roundCorners",
      base::GetFieldTrialParamByFeatureAsInt(
          ntp_features::kRealboxMatchSearchboxTheme,
          ntp_features::kRealboxMatchSearchboxThemeParam, 0) == 1);
  source->AddBoolean(
      "realboxMatchSearchboxTheme",
      base::FeatureList::IsEnabled(ntp_features::kRealboxMatchSearchboxTheme));
  source->AddString(
      "realboxDefaultIcon",
      base::FeatureList::IsEnabled(ntp_features::kRealboxUseGoogleGIcon)
          ? kGoogleGIconResourceName
          : kSearchIconResourceName);
  source->AddString("realboxHint", l10n_util::GetStringUTF8(
                                       IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD));
}

// static
std::string RealboxHandler::AutocompleteMatchVectorIconToResourceName(
    const gfx::VectorIcon& icon) {
  std::string answerNames[] = {
      omnibox::kAnswerCurrencyIcon.name,   omnibox::kAnswerDefaultIcon.name,
      omnibox::kAnswerDictionaryIcon.name, omnibox::kAnswerFinanceIcon.name,
      omnibox::kAnswerSunriseIcon.name,    omnibox::kAnswerTranslationIcon.name,
      omnibox::kAnswerWhenIsIcon.name,     omnibox::kAnswerWhenIsIcon.name};

  if (!base::FeatureList::IsEnabled(omnibox::kNtpRealboxSuggestionAnswers) &&
      base::Contains(answerNames, icon.name)) {
    return kSearchIconResourceName;
  } else if (icon.name == omnibox::kAnswerCurrencyIcon.name) {
    return kAnswerCurrencyIconResourceName;
  } else if (icon.name == omnibox::kAnswerDefaultIcon.name) {
    return kAnswerDefaultIconResourceName;
  } else if (icon.name == omnibox::kAnswerDictionaryIcon.name) {
    return kAnswerDictionaryIconResourceName;
  } else if (icon.name == omnibox::kAnswerFinanceIcon.name) {
    return kAnswerFinanceIconResourceName;
  } else if (icon.name == omnibox::kAnswerSunriseIcon.name) {
    return kAnswerSunriseIconResourceName;
  } else if (icon.name == omnibox::kAnswerTranslationIcon.name) {
    return kAnswerTranslationIconResourceName;
  } else if (icon.name == omnibox::kAnswerWhenIsIcon.name) {
    return kAnswerWhenIsIconResourceName;
  } else if (icon.name == omnibox::kBlankIcon.name) {
    return "";  // An empty resource name is effectively a blank icon.
  } else if (icon.name == omnibox::kBookmarkIcon.name) {
    return kBookmarkIconResourceName;
  } else if (icon.name == omnibox::kCalculatorIcon.name) {
    return kCalculatorIconResourceName;
  } else if (icon.name == omnibox::kClockIcon.name) {
    return kClockIconResourceName;
  } else if (icon.name == omnibox::kDriveDocsIcon.name) {
    return kDriveDocsIconResourceName;
  } else if (icon.name == omnibox::kDriveFolderIcon.name) {
    return kDriveFolderIconResourceName;
  } else if (icon.name == omnibox::kDriveFormsIcon.name) {
    return kDriveFormIconResourceName;
  } else if (icon.name == omnibox::kDriveImageIcon.name) {
    return kDriveImageIconResourceName;
  } else if (icon.name == omnibox::kDriveLogoIcon.name) {
    return kDriveLogoIconResourceName;
  } else if (icon.name == omnibox::kDrivePdfIcon.name) {
    return kDrivePdfIconResourceName;
  } else if (icon.name == omnibox::kDriveSheetsIcon.name) {
    return kDriveSheetsIconResourceName;
  } else if (icon.name == omnibox::kDriveSlidesIcon.name) {
    return kDriveSlidesIconResourceName;
  } else if (icon.name == omnibox::kDriveVideoIcon.name) {
    return kDriveVideoIconResourceName;
  } else if (icon.name == omnibox::kExtensionAppIcon.name) {
    return kExtensionAppIconResourceName;
  } else if (icon.name == omnibox::kPageIcon.name) {
    return kPageIconResourceName;
  } else if (icon.name == omnibox::kPedalIcon.name) {
    return kPedalsIconResourceName;
  } else if (icon.name == vector_icons::kSearchIcon.name) {
    return kSearchIconResourceName;
  } else if (icon.name == omnibox::kTrendingUpIcon.name) {
    return kTrendingUpIconResourceName;
  } else {
    NOTREACHED()
        << "Every vector icon returned by AutocompleteMatch::GetVectorIcon "
           "must have an equivalent SVG resource for the NTP Realbox.";
    return "";
  }
}

// static
std::string RealboxHandler::PedalVectorIconToResourceName(
    const gfx::VectorIcon& icon) {
  if (icon.name == omnibox::kDinoIcon.name) {
    return kDinoIconResourceName;
  }
  if (icon.name == omnibox::kDriveFormsIcon.name) {
    return kDriveFormIconResourceName;
  }
  if (icon.name == omnibox::kDriveDocsIcon.name) {
    return kDriveDocsIconResourceName;
  }
  if (icon.name == omnibox::kDriveSheetsIcon.name) {
    return kDriveSheetsIconResourceName;
  }
  if (icon.name == omnibox::kDriveSlidesIcon.name) {
    return kDriveSlidesIconResourceName;
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (icon.name == kGoogleCalendarIcon.name) {
    return kGoogleCalendarIconResourceName;
  }
  if (icon.name == kGoogleKeepNoteIcon.name) {
    return kGoogleKeepNoteIconResourceName;
  }
  if (icon.name == kGoogleSitesIcon.name) {
    return kGoogleSitesIconResourceName;
  }
  if (icon.name == kGoogleSuperGIcon.name) {
    return kGoogleGIconResourceName;
  }
#endif
  if (icon.name == omnibox::kIncognitoIcon.name) {
    return kIncognitoIconResourceName;
  }
  if (icon.name == omnibox::kPedalIcon.name) {
    return kPedalsIconResourceName;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (icon.name == omnibox::kShareIcon.name) {
    return kCrosShareIconResourceName;
  }
#elif BUILDFLAG(IS_MAC)
  if (icon.name == omnibox::kShareMacIcon.name) {
    return kMacShareIconResourceName;
  }
#elif BUILDFLAG(IS_WIN)
  if (icon.name == omnibox::kShareWinIcon.name) {
    return kWinShareIconResourceName;
  }
#else
  if (icon.name == omnibox::kSendIcon.name) {
    return kLinuxShareIconResourceName;
  }
#endif
  NOTREACHED() << "Every vector icon returned by OmniboxAction::GetVectorIcon "
                  "must have an equivalent SVG resource for the NTP Realbox.";
  return "";
}

RealboxHandler::RealboxHandler(
    mojo::PendingReceiver<realbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      bitmap_fetcher_service_(
          BitmapFetcherServiceFactory::GetForBrowserContext(profile)),
      favicon_cache_(FaviconServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS),
                     HistoryServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS)),
      page_handler_(this, std::move(pending_page_handler)) {}

RealboxHandler::~RealboxHandler() {
  // Clear pending bitmap requests.
  for (auto bitmap_request_id : bitmap_request_ids_) {
    bitmap_fetcher_service_->CancelRequest(bitmap_request_id);
  }
}

void RealboxHandler::SetPage(
    mojo::PendingRemote<realbox::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void RealboxHandler::QueryAutocomplete(const std::u16string& input,
                                       bool prevent_inline_autocomplete) {
  if (!autocomplete_controller_) {
    autocomplete_controller_ = std::make_unique<AutocompleteController>(
        std::make_unique<ChromeAutocompleteProviderClient>(profile_),
        AutocompleteClassifier::DefaultOmniboxProviders());
    autocomplete_controller_->AddObserver(this);

    OmniboxControllerEmitter* emitter =
        OmniboxControllerEmitter::GetForBrowserContext(profile_);
    if (emitter)
      autocomplete_controller_->AddObserver(emitter);
  }

  // TODO(tommycli): We use the input being empty as a signal we are requesting
  // on-focus suggestions. It would be nice if we had a more explicit signal.
  bool is_on_focus = input.empty();

  // Early exit if a query is already in progress for on focus inputs.
  if (!autocomplete_controller_->done() && is_on_focus)
    return;

  if (time_user_first_modified_realbox_.is_null() && !is_on_focus)
    time_user_first_modified_realbox_ = base::TimeTicks::Now();

  AutocompleteInput autocomplete_input(
      input, metrics::OmniboxEventProto::NTP_REALBOX,
      ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_input.set_focus_type(is_on_focus ? OmniboxFocusType::ON_FOCUS
                                                : OmniboxFocusType::DEFAULT);
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);

  // We do not want keyword matches for the NTP realbox, which has no UI
  // facilities to support them.
  autocomplete_input.set_prefer_keyword(false);
  autocomplete_input.set_allow_exact_keyword_match(false);

  autocomplete_controller_->Start(autocomplete_input);
}

void RealboxHandler::StopAutocomplete(bool clear_result) {
  if (!autocomplete_controller_)
    return;

  autocomplete_controller_->Stop(clear_result);

  if (clear_result)
    time_user_first_modified_realbox_ = base::TimeTicks();
}

void RealboxHandler::OpenAutocompleteMatch(
    uint8_t line,
    const GURL& url,
    bool are_matches_showing,
    base::TimeDelta time_elapsed_since_last_focus,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  if (autocomplete_controller_->result().size() <= line) {
    return;
  }

  AutocompleteMatch match(autocomplete_controller_->result().match_at(line));
  if (match.destination_url != url) {
    // TODO(https://crbug.com/1020025): this could be malice or staleness.
    // Either way: don't navigate.
    return;
  }

  // TODO(crbug.com/1041129): The following logic for recording Omnibox metrics
  // is largely copied from SearchTabHelper::OpenAutocompleteMatch(). Make sure
  // any changes here is reflected there until one code path is obsolete.

  const auto now = base::TimeTicks::Now();
  base::TimeDelta elapsed_time_since_first_autocomplete_query =
      now - time_user_first_modified_realbox_;
  autocomplete_controller_
      ->UpdateMatchDestinationURLWithAdditionalAssistedQueryStats(
          elapsed_time_since_first_autocomplete_query, &match);

  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);

  UMA_HISTOGRAM_MEDIUM_TIMES("Omnibox.FocusToOpenTimeAnyPopupState3",
                             time_elapsed_since_last_focus);

  if (ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_TYPED)) {
    navigation_metrics::RecordOmniboxURLNavigation(match.destination_url);
  }
  // The following histogram should be recorded for both TYPED and pasted
  // URLs, but should still exclude reloads.
  if (ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_TYPED) ||
      ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_LINK)) {
    net::cookie_util::RecordCookiePortOmniboxHistograms(match.destination_url);
  }

  SuggestionAnswer::LogAnswerUsed(match.answer);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          match.destination_url)) {
    // Note: will always be false for the realbox.
    UMA_HISTOGRAM_BOOLEAN("Omnibox.Search.OffTheRecord",
                          profile_->IsOffTheRecord());
    base::RecordAction(
        base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
  }

  AutocompleteMatch::LogSearchEngineUsed(match, template_url_service);

  auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile_);
  if (bookmark_model->IsBookmarked(match.destination_url)) {
    RecordBookmarkLaunch(BOOKMARK_LAUNCH_LOCATION_OMNIBOX,
                         profile_metrics::GetBrowserProfileType(profile_));
  }

  const AutocompleteInput& input = autocomplete_controller_->input();
  WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);

  base::TimeDelta default_time_delta = base::Milliseconds(-1);

  if (time_user_first_modified_realbox_.is_null())
    elapsed_time_since_first_autocomplete_query = default_time_delta;

  base::TimeDelta elapsed_time_since_last_change_to_default_match =
      !autocomplete_controller_->last_time_default_match_changed().is_null()
          ? now - autocomplete_controller_->last_time_default_match_changed()
          : default_time_delta;

  OmniboxLog log(
      /*text=*/input.focus_type() != OmniboxFocusType::DEFAULT
          ? std::u16string()
          : input.text(),
      /*just_deleted_text=*/input.prevent_inline_autocomplete(),
      /*input_type=*/input.type(),
      /*in_keyword_mode=*/false,
      /*entry_method=*/metrics::OmniboxEventProto::INVALID,
      /*is_popup_open=*/are_matches_showing,
      /*selected_index=*/line,
      /*disposition=*/disposition,
      /*is_paste_and_go=*/false,
      /*tab_id=*/sessions::SessionTabHelper::IdForTab(web_contents_),
      /*current_page_classification=*/metrics::OmniboxEventProto::NTP_REALBOX,
      /*elapsed_time_since_user_first_modified_omnibox=*/
      elapsed_time_since_first_autocomplete_query,
      /*completed_length=*/match.allowed_to_be_default_match
          ? match.inline_autocompletion.length()
          : std::u16string::npos,
      /*elapsed_time_since_last_change_to_default_match=*/
      elapsed_time_since_last_change_to_default_match,
      /*result=*/autocomplete_controller_->result());
  autocomplete_controller_->AddProviderAndTriggeringLogs(&log);

  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(log);

  web_contents_->OpenURL(
      content::OpenURLParams(match.destination_url, content::Referrer(),
                             disposition, match.transition, false));
}

void RealboxHandler::OpenURL(const GURL& destination_url,
                             TemplateURLRef::PostContent* post_content,
                             WindowOpenDisposition disposition,
                             ui::PageTransition transition,
                             AutocompleteMatchType::Type type,
                             base::TimeTicks match_selection_timestamp,
                             bool destination_url_entered_without_scheme,
                             const std::u16string&,
                             const AutocompleteMatch&,
                             const AutocompleteMatch&) {
  web_contents_->OpenURL(content::OpenURLParams(
      destination_url, content::Referrer(), disposition, transition, false));
}

void RealboxHandler::DeleteAutocompleteMatch(uint8_t line) {
  if (autocomplete_controller_->result().size() <= line ||
      !autocomplete_controller_->result().match_at(line).SupportsDeletion()) {
    return;
  }

  const auto& match = autocomplete_controller_->result().match_at(line);
  if (match.SupportsDeletion()) {
    autocomplete_controller_->Stop(false);
    autocomplete_controller_->DeleteMatch(match);
  }
}

void RealboxHandler::ToggleSuggestionGroupIdVisibility(
    int32_t suggestion_group_id) {
  if (!autocomplete_controller_)
    return;

  omnibox::SuggestionGroupVisibility new_value =
      autocomplete_controller_->result().IsSuggestionGroupIdHidden(
          profile_->GetPrefs(), suggestion_group_id)
          ? omnibox::SuggestionGroupVisibility::SHOWN
          : omnibox::SuggestionGroupVisibility::HIDDEN;
  omnibox::SetSuggestionGroupVisibility(profile_->GetPrefs(),
                                        suggestion_group_id, new_value);
}

void RealboxHandler::LogCharTypedToRepaintLatency(base::TimeDelta latency) {
  UMA_HISTOGRAM_TIMES("NewTabPage.Realbox.CharTypedToRepaintLatency.ToPaint",
                      latency);
}

void RealboxHandler::ExecuteAction(uint8_t line,
                                   base::TimeTicks match_selection_timestamp,
                                   uint8_t mouse_button,
                                   bool alt_key,
                                   bool ctrl_key,
                                   bool meta_key,
                                   bool shift_key) {
  if (!autocomplete_controller_ ||
      autocomplete_controller_->result().size() <= line) {
    return;
  }

  const auto& match = autocomplete_controller_->result().match_at(line);
  if (!match.action) {
    return;
  }
  WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);

  match.action->RecordActionExecuted(line);
  OmniboxAction::ExecutionContext context(
      *(autocomplete_controller_->autocomplete_provider_client()),
      base::BindOnce(&RealboxHandler::OpenURL, weak_ptr_factory_.GetWeakPtr()),
      match_selection_timestamp, disposition);
  match.action->Execute(context);
}

void RealboxHandler::OnResultChanged(AutocompleteController* controller,
                                     bool default_match_changed) {
  DCHECK(controller == autocomplete_controller_.get());

  // Prepend missing tail suggestion prefixes in results, if present.
  if (base::FeatureList::IsEnabled(omnibox::kNtpRealboxTailSuggest)) {
    autocomplete_controller_->SetTailSuggestCommonPrefixes();
  }

  page_->AutocompleteResultChanged(CreateAutocompleteResult(
      autocomplete_controller_->input().text(),
      autocomplete_controller_->result(),
      BookmarkModelFactory::GetForBrowserContext(profile_),
      profile_->GetPrefs()));

  // Clear pending bitmap requests before requesting new ones.
  for (auto bitmap_request_id : bitmap_request_ids_) {
    bitmap_fetcher_service_->CancelRequest(bitmap_request_id);
  }
  bitmap_request_ids_.clear();

  int match_index = -1;
  for (const auto& match : autocomplete_controller_->result()) {
    match_index++;

    // Request bitmaps for matche images.
    if (!match.ImageUrl().is_empty()) {
      bitmap_request_ids_.push_back(bitmap_fetcher_service_->RequestImage(
          match.ImageUrl(),
          base::BindOnce(&RealboxHandler::OnRealboxBitmapFetched,
                         weak_ptr_factory_.GetWeakPtr(), match_index,
                         match.ImageUrl())));
    }

    // Request favicons for navigational matches.
    // TODO(crbug.com/1075848): Investigate using chrome://favicon2.
    if (!AutocompleteMatch::IsSearchType(match.type) &&
        match.type != AutocompleteMatchType::DOCUMENT_SUGGESTION) {
      gfx::Image favicon = favicon_cache_.GetLargestFaviconForPageUrl(
          match.destination_url,
          base::BindOnce(&RealboxHandler::OnRealboxFaviconFetched,
                         weak_ptr_factory_.GetWeakPtr(), match_index,
                         match.destination_url));
      if (!favicon.IsEmpty()) {
        OnRealboxFaviconFetched(match_index, match.destination_url, favicon);
      }
    }
  }
}

void RealboxHandler::OnRealboxBitmapFetched(int match_index,
                                            const GURL& image_url,
                                            const SkBitmap& bitmap) {
  auto data = gfx::Image::CreateFrom1xBitmap(bitmap).As1xPNGBytes();
  std::string data_url =
      webui::GetPngDataUrl(data->front_as<unsigned char>(), data->size());

  page_->AutocompleteMatchImageAvailable(match_index, image_url, data_url);
}

void RealboxHandler::OnRealboxFaviconFetched(int match_index,
                                             const GURL& page_url,
                                             const gfx::Image& favicon) {
  DCHECK(!favicon.IsEmpty());
  auto data = favicon.As1xPNGBytes();
  std::string data_url =
      webui::GetPngDataUrl(data->front_as<unsigned char>(), data->size());

  page_->AutocompleteMatchImageAvailable(match_index, page_url, data_url);
}
