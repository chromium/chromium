// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/realbox/realbox_handler.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_client.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/cookies/cookie_util.h"
#include "realbox_handler.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/resources/grit/webui_resources.h"

namespace {

constexpr char kAnswerCurrencyIconResourceName[] =
    "//resources/cr_components/omnibox/icons/currency.svg";
constexpr char kAnswerDefaultIconResourceName[] =
    "//resources/cr_components/omnibox/icons/default.svg";
constexpr char kAnswerDictionaryIconResourceName[] =
    "//resources/cr_components/omnibox/icons/definition.svg";
constexpr char kAnswerFinanceIconResourceName[] =
    "//resources/cr_components/omnibox/icons/finance.svg";
constexpr char kAnswerSunriseIconResourceName[] =
    "//resources/cr_components/omnibox/icons/sunrise.svg";
constexpr char kAnswerTranslationIconResourceName[] =
    "//resources/cr_components/omnibox/icons/translation.svg";
constexpr char kAnswerWhenIsIconResourceName[] =
    "//resources/cr_components/omnibox/icons/when_is.svg";
constexpr char kBookmarkIconResourceName[] =
    "//resources/images/icon_bookmark.svg";
constexpr char kCalculatorIconResourceName[] =
    "//resources/cr_components/omnibox/icons/calculator.svg";
constexpr char kChromeProductIconResourceName[] =
    "//resources/cr_components/omnibox/icons/chrome_product.svg";
constexpr char kClockIconResourceName[] = "//resources/images/icon_clock.svg";
constexpr char kDinoIconResourceName[] =
    "//resources/cr_components/omnibox/icons/dino.svg";
constexpr char kDriveDocsIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_docs.svg";
constexpr char kDriveFolderIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_folder.svg";
constexpr char kDriveFormIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_form.svg";
constexpr char kDriveImageIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_image.svg";
constexpr char kDriveLogoIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_logo.svg";
constexpr char kDrivePdfIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_pdf.svg";
constexpr char kDriveSheetsIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_sheets.svg";
constexpr char kDriveSlidesIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_slides.svg";
constexpr char kDriveVideoIconResourceName[] =
    "//resources/cr_components/omnibox/icons/drive_video.svg";
constexpr char kExtensionAppIconResourceName[] =
    "//resources/cr_components/omnibox/icons/extension_app.svg";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kGoogleCalendarIconResourceName[] =
    "//resources/cr_components/omnibox/icons/calendar.svg";
constexpr char kGoogleGIconResourceName[] =
    "//resources/cr_components/omnibox/icons/google_g.svg";
constexpr char kGoogleKeepNoteIconResourceName[] =
    "//resources/cr_components/omnibox/icons/note.svg";
constexpr char kGoogleSitesIconResourceName[] =
    "//resources/cr_components/omnibox/icons/sites.svg";
#endif
constexpr char kIncognitoIconResourceName[] =
    "//resources/cr_components/omnibox/icons/incognito.svg";
constexpr char kJourneysIconResourceName[] =
    "//resources/cr_components/omnibox/icons/journeys.svg";
constexpr char kPageIconResourceName[] =
    "//resources/cr_components/omnibox/icons/page.svg";
constexpr char kPedalsIconResourceName[] = "//theme/current-channel-logo";
constexpr char kSearchIconResourceName[] = "//resources/images/icon_search.svg";
constexpr char kTabIconResourceName[] =
    "//resources/cr_components/omnibox/icons/tab.svg";
constexpr char kTrendingUpIconResourceName[] =
    "//resources/cr_components/omnibox/icons/trending_up.svg";

#if BUILDFLAG(IS_MAC)
constexpr char kMacShareIconResourceName[] =
    "//resources/cr_components/omnibox/icons/mac_share.svg";
#elif BUILDFLAG(IS_WIN)
constexpr char kWinShareIconResourceName[] =
    "//resources/cr_components/omnibox/icons/win_share.svg";
#elif BUILDFLAG(IS_LINUX)
constexpr char kLinuxShareIconResourceName[] =
    "//resources/cr_components/omnibox/icons/share.svg";
#else
constexpr char kShareIconResourceName[] =
    "//resources/cr_components/omnibox/icons/share.svg";
#endif

base::flat_map<int32_t, omnibox::mojom::SuggestionGroupPtr>
CreateSuggestionGroupsMap(
    const AutocompleteResult& result,
    PrefService* prefs,
    const omnibox::GroupConfigMap& suggestion_groups_map) {
  base::flat_map<int32_t, omnibox::mojom::SuggestionGroupPtr> result_map;
  for (const auto& pair : suggestion_groups_map) {
    omnibox::mojom::SuggestionGroupPtr suggestion_group =
        omnibox::mojom::SuggestionGroup::New();
    suggestion_group->header = base::UTF8ToUTF16(pair.second.header_text());
    suggestion_group->side_type =
        static_cast<omnibox::mojom::SideType>(pair.second.side_type());
    suggestion_group->hidden =
        result.IsSuggestionGroupHidden(prefs, pair.first);
    suggestion_group->show_group_a11y_label = l10n_util::GetStringFUTF16(
        IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON, suggestion_group->header);
    suggestion_group->hide_group_a11y_label = l10n_util::GetStringFUTF16(
        IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON, suggestion_group->header);

    result_map.emplace(static_cast<int>(pair.first),
                       std::move(suggestion_group));
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

bool MatchHasSideTypeAndRenderType(
    const AutocompleteMatch& match,
    omnibox::GroupConfig_SideType side_type,
    omnibox::GroupConfig_RenderType render_type,
    const omnibox::GroupConfigMap& suggestion_groups_map) {
  omnibox::GroupId group_id =
      match.suggestion_group_id.value_or(omnibox::GROUP_INVALID);
  return base::Contains(suggestion_groups_map, group_id) &&
         suggestion_groups_map.at(group_id).side_type() == side_type &&
         suggestion_groups_map.at(group_id).render_type() == render_type;
}

std::u16string GetAdditionalA11yMessage(const AutocompleteMatch& match,
                                        RealboxHandler::FocusState state) {
  switch (state) {
    case RealboxHandler::FocusState::kFocusedMatch: {
      if (match.has_tab_match.value_or(false) &&
          base::FeatureList::IsEnabled(omnibox::kNtpRealboxPedals)) {
        return l10n_util::GetStringUTF16(IDS_ACC_TAB_SWITCH_SUFFIX);
      }
      const OmniboxAction* action = match.GetActionAt(0u);
      if (action) {
        return action->GetLabelStrings().accessibility_suffix;
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

std::vector<omnibox::mojom::AutocompleteMatchPtr> CreateAutocompleteMatches(
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model,
    const omnibox::GroupConfigMap& suggestion_groups_map) {
  std::vector<omnibox::mojom::AutocompleteMatchPtr> matches;
  int line = 0;
  for (const AutocompleteMatch& match : result) {
    // Skip the primary column horizontal matches. This check guards against
    // this unexpected scenario as the UI expects the primary column matches to
    // be vertical ones.
    if (MatchHasSideTypeAndRenderType(
            match, omnibox::GroupConfig_SideType_DEFAULT_PRIMARY,
            omnibox::GroupConfig_RenderType_HORIZONTAL,
            suggestion_groups_map)) {
      continue;
    }

    // Skip the secondary column horizontal matches that are not entities or do
    // not have images. This check guards against this unexpected scenario as
    // the UI expects the secondary column horizontal matches to be entity
    // suggestions with images.
    if (MatchHasSideTypeAndRenderType(
            match, omnibox::GroupConfig_SideType_SECONDARY,
            omnibox::GroupConfig_RenderType_HORIZONTAL,
            suggestion_groups_map) &&
        (match.type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         !match.image_url.is_valid())) {
      continue;
    }

    omnibox::mojom::AutocompleteMatchPtr mojom_match =
        omnibox::mojom::AutocompleteMatch::New();
    mojom_match->allowed_to_be_default_match =
        match.allowed_to_be_default_match;
    mojom_match->contents = match.contents;
    for (const auto& contents_class : match.contents_class) {
      mojom_match->contents_class.push_back(
          omnibox::mojom::ACMatchClassification::New(contents_class.offset,
                                                     contents_class.style));
    }
    mojom_match->description = match.description;
    for (const auto& description_class : match.description_class) {
      mojom_match->description_class.push_back(
          omnibox::mojom::ACMatchClassification::New(description_class.offset,
                                                     description_class.style));
    }
    mojom_match->destination_url = match.destination_url;
    mojom_match->suggestion_group_id =
        match.suggestion_group_id.value_or(omnibox::GROUP_INVALID);
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
    if (match.answer.has_value()) {
      const auto& additional_text =
          GetAdditionalText(match.answer->first_line());
      mojom_match->answer = omnibox::mojom::SuggestionAnswer::New(
          additional_text ? base::JoinString(
                                {match.contents, additional_text.value()}, u" ")
                          : match.contents,
          ImageLineToString16(match.answer->second_line()));
      mojom_match->image_url = match.ImageUrl().spec();
    }
    mojom_match->is_rich_suggestion =
        !mojom_match->image_url.empty() ||
        match.type == AutocompleteMatchType::CALCULATOR ||
        (match.answer.has_value());
    if (base::FeatureList::IsEnabled(omnibox::kNtpRealboxPedals)) {
      for (const auto& action : match.actions) {
        const OmniboxAction::LabelStrings& label_strings =
            action->GetLabelStrings();
        mojom_match->actions.emplace_back(omnibox::mojom::Action::New(
            label_strings.accessibility_hint, label_strings.hint,
            label_strings.suggestion_contents,
            RealboxHandler::PedalVectorIconToResourceName(
                action->GetVectorIcon())));
      }
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

omnibox::mojom::AutocompleteResultPtr CreateAutocompleteResult(
    const std::u16string& input,
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model,
    PrefService* prefs) {
  return omnibox::mojom::AutocompleteResult::New(
      input,
      CreateSuggestionGroupsMap(result, prefs, result.suggestion_groups_map()),
      CreateAutocompleteMatches(result, bookmark_model,
                                result.suggestion_groups_map()));
}

std::string GetBase64UrlVariations(Profile* profile) {
  variations::VariationsClient* provider = profile->GetVariationsClient();

  variations::mojom::VariationsHeadersPtr headers =
      provider->GetVariationsHeaders();
  if (headers.is_null()) {
    return std::string();
  }
  const std::string variations_base64 = headers->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY);

  // Variations headers are base64 encoded, however, we're attaching the value
  // to a URL query parameter so they need to be base64url encoded.
  std::string variations_decoded;
  base::Base64Decode(variations_base64, &variations_decoded);

  std::string variations_base64url;
  base::Base64UrlEncode(variations_decoded,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &variations_base64url);

  return variations_base64url;
}

// TODO(crbug.com/1431513): Consider inheriting from `ChromeOmniboxClient`
//  to avoid reimplementation of methods like `OnBookmarkLaunched`.
class RealboxOmniboxClient : public OmniboxClient {
 public:
  explicit RealboxOmniboxClient(Profile* profile,
                                content::WebContents* web_contents);
  ~RealboxOmniboxClient() override;

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  bool IsPasteAndGoEnabled() const override;
  SessionID GetSessionID() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                          SkColor vector_icon_color) const override;
  gfx::Image GetFaviconForPageUrl(
      const GURL& page_url,
      FaviconFetchedCallback on_favicon_fetched) override;
  void OnBookmarkLaunched() override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  ChromeAutocompleteSchemeClassifier scheme_classifier_;
};

RealboxOmniboxClient::RealboxOmniboxClient(Profile* profile,
                                           content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      scheme_classifier_(ChromeAutocompleteSchemeClassifier(profile)) {}

RealboxOmniboxClient::~RealboxOmniboxClient() = default;

std::unique_ptr<AutocompleteProviderClient>
RealboxOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<ChromeAutocompleteProviderClient>(profile_);
}

bool RealboxOmniboxClient::IsPasteAndGoEnabled() const {
  return false;
}

SessionID RealboxOmniboxClient::GetSessionID() const {
  return sessions::SessionTabHelper::IdForTab(web_contents_);
}

bookmarks::BookmarkModel* RealboxOmniboxClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

AutocompleteControllerEmitter*
RealboxOmniboxClient::GetAutocompleteControllerEmitter() {
  return AutocompleteControllerEmitter::GetForBrowserContext(profile_);
}

TemplateURLService* RealboxOmniboxClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier& RealboxOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* RealboxOmniboxClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool RealboxOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return false;
}

int RealboxOmniboxClient::GetHttpsPortForTesting() const {
  return 0;
}

bool RealboxOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return false;
}

gfx::Image RealboxOmniboxClient::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  return gfx::Image();
}

gfx::Image RealboxOmniboxClient::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

void RealboxOmniboxClient::OnBookmarkLaunched() {
  RecordBookmarkLaunch(BookmarkLaunchLocation::kOmnibox,
                       profile_metrics::GetBrowserProfileType(profile_));
}

void RealboxOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  if (auto* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    search_prefetch_service->OnURLOpenedFromOmnibox(log);
  }
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(*log);
}

}  // namespace

// static
void RealboxHandler::SetupWebUIDataSource(content::WebUIDataSource* source,
                                          Profile* profile) {
  RealboxHandler::SetupDropdownWebUIDataSource(source, profile);

  static constexpr webui::LocalizedString kStrings[] = {
      {"searchBoxHint", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD}};
  source->AddLocalizedStrings(kStrings);

  source->AddBoolean(
      "realboxMatchSearchboxTheme",
      base::FeatureList::IsEnabled(ntp_features::kRealboxMatchSearchboxTheme));

  source->AddString("realboxWidthBehavior",
                    base::GetFieldTrialParamValueByFeature(
                        ntp_features::kRealboxWidthBehavior,
                        ntp_features::kNtpRealboxWidthBehaviorParam));
  source->AddBoolean("realboxIsTall", base::FeatureList::IsEnabled(
                                          ntp_features::kRealboxIsTall));

  source->AddString(
      "realboxDefaultIcon",
      base::FeatureList::IsEnabled(ntp_features::kRealboxUseGoogleGIcon)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          ? kGoogleGIconResourceName
#else
          ? kSearchIconResourceName
#endif
          : kSearchIconResourceName);

  source->AddBoolean(
      "realboxLensSearch",
      base::FeatureList::IsEnabled(ntp_features::kNtpRealboxLensSearch) &&
          profile->GetPrefs()->GetBoolean(prefs::kLensDesktopNTPSearchEnabled));
  source->AddString("realboxLensVariations", GetBase64UrlVariations(profile));
}

// static
void RealboxHandler::SetupDropdownWebUIDataSource(
    content::WebUIDataSource* source,
    Profile* profile) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"realboxSeparator", IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR},
      {"removeSuggestion", IDS_OMNIBOX_REMOVE_SUGGESTION},
      {"hideSuggestions", IDS_TOOLTIP_HEADER_HIDE_SUGGESTIONS_BUTTON},
      {"showSuggestions", IDS_TOOLTIP_HEADER_SHOW_SUGGESTIONS_BUTTON}};
  source->AddLocalizedStrings(kStrings);

  source->AddBoolean("roundCorners", base::FeatureList::IsEnabled(
                                         ntp_features::kRealboxRoundedCorners));
}

// static
std::string RealboxHandler::AutocompleteMatchVectorIconToResourceName(
    const gfx::VectorIcon& icon) {
  if (icon.name == omnibox::kAnswerCurrencyIcon.name ||
      icon.name == omnibox::kAnswerCurrencyChromeRefreshIcon.name) {
    return kAnswerCurrencyIconResourceName;
  } else if (icon.name == omnibox::kAnswerDefaultIcon.name) {
    return kAnswerDefaultIconResourceName;
  } else if (icon.name == omnibox::kAnswerDictionaryIcon.name ||
             icon.name == omnibox::kAnswerDictionaryChromeRefreshIcon.name) {
    return kAnswerDictionaryIconResourceName;
  } else if (icon.name == omnibox::kAnswerFinanceIcon.name ||
             icon.name == omnibox::kAnswerFinanceChromeRefreshIcon.name) {
    return kAnswerFinanceIconResourceName;
  } else if (icon.name == omnibox::kAnswerSunriseIcon.name ||
             icon.name == omnibox::kAnswerSunriseChromeRefreshIcon.name) {
    return kAnswerSunriseIconResourceName;
  } else if (icon.name == omnibox::kAnswerTranslationIcon.name ||
             icon.name == omnibox::kAnswerTranslationChromeRefreshIcon.name) {
    return kAnswerTranslationIconResourceName;
  } else if (icon.name == omnibox::kAnswerWhenIsIcon.name ||
             icon.name == omnibox::kAnswerWhenIsChromeRefreshIcon.name) {
    return kAnswerWhenIsIconResourceName;
  } else if (icon.name == omnibox::kBookmarkIcon.name ||
             icon.name == omnibox::kBookmarkChromeRefreshIcon.name) {
    return kBookmarkIconResourceName;
  } else if (icon.name == omnibox::kCalculatorIcon.name ||
             icon.name == omnibox::kCalculatorChromeRefreshIcon.name) {
    return kCalculatorIconResourceName;
  } else if (icon.name == omnibox::kClockIcon.name ||
             icon.name == omnibox::kClockChromeRefreshIcon.name) {
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
  } else if (icon.name == omnibox::kJourneysIcon.name ||
             icon.name == omnibox::kJourneysChromeRefreshIcon.name) {
    return kJourneysIconResourceName;
  } else if (icon.name == omnibox::kPageIcon.name ||
             icon.name == omnibox::kPageChromeRefreshIcon.name) {
    return kPageIconResourceName;
  } else if (icon.name == omnibox::kPedalIcon.name) {
    return kPedalsIconResourceName;
  } else if (icon.name == omnibox::kProductIcon.name ||
             icon.name == omnibox::kProductChromeRefreshIcon.name) {
    return kChromeProductIconResourceName;
  } else if (icon.name == vector_icons::kSearchIcon.name ||
             icon.name == vector_icons::kSearchChromeRefreshIcon.name) {
    return kSearchIconResourceName;
  } else if (icon.name == omnibox::kTrendingUpIcon.name ||
             icon.name == omnibox::kTrendingUpChromeRefreshIcon.name) {
    return kTrendingUpIconResourceName;
  } else if (icon.is_empty()) {
    return "";  // An empty resource name is effectively a blank icon.
  } else {
    NOTREACHED()
        << "Every vector icon returned by AutocompleteMatch::GetVectorIcon "
           "must have an equivalent SVG resource for the NTP Realbox. "
           "icon.name: '"
        << icon.name << "'";
  }
  return "";
}

// static
std::string RealboxHandler::PedalVectorIconToResourceName(
    const gfx::VectorIcon& icon) {
  if (icon.name == omnibox::kSwitchIcon.name) {
    return kTabIconResourceName;
  }
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
  if (icon.name == vector_icons::kGoogleCalendarIcon.name) {
    return kGoogleCalendarIconResourceName;
  }
  if (icon.name == vector_icons::kGoogleKeepNoteIcon.name) {
    return kGoogleKeepNoteIconResourceName;
  }
  if (icon.name == vector_icons::kGoogleSitesIcon.name) {
    return kGoogleSitesIconResourceName;
  }
  if (icon.name == vector_icons::kGoogleSuperGIcon.name) {
    return kGoogleGIconResourceName;
  }
#endif
  if (icon.name == omnibox::kIncognitoIcon.name) {
    return kIncognitoIconResourceName;
  }
  if (icon.name == omnibox::kJourneysIcon.name ||
      icon.name == omnibox::kJourneysChromeRefreshIcon.name) {
    return kJourneysIconResourceName;
  }
  if (icon.name == omnibox::kPedalIcon.name) {
    return kPedalsIconResourceName;
  }
#if BUILDFLAG(IS_MAC)
  if (icon.name == omnibox::kShareMacIcon.name ||
      icon.name == omnibox::kShareMacChromeRefreshIcon.name) {
    return kMacShareIconResourceName;
  }
#elif BUILDFLAG(IS_WIN)
  if (icon.name == omnibox::kShareWinIcon.name ||
      icon.name == omnibox::kShareWinChromeRefreshIcon.name) {
    return kWinShareIconResourceName;
  }
#elif BUILDFLAG(IS_LINUX)
  if (icon.name == omnibox::kShareIcon.name ||
      icon.name == omnibox::kShareLinuxChromeRefreshIcon.name) {
    return kLinuxShareIconResourceName;
  }
#else
  if (icon.name == omnibox::kShareIcon.name ||
      icon.name == omnibox::kShareChromeRefreshIcon.name) {
    return kShareIconResourceName;
  }
#endif
  NOTREACHED() << "Every vector icon returned by OmniboxAction::GetVectorIcon "
                  "must have an equivalent SVG resource for the NTP Realbox. "
                  "icon.name: '"
               << icon.name << "'";
  return "";
}

RealboxHandler::RealboxHandler(
    mojo::PendingReceiver<omnibox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter)
    : profile_(profile),
      web_contents_(web_contents),
      metrics_reporter_(metrics_reporter),
      page_handler_(this, std::move(pending_page_handler)) {
  // Indirectly observe all the `AutocompleteController` instances registered
  // with the `AutocompleteControllerEmitter`. In addition to observing the
  // instance associated with this handler, `RealboxHandler` also observes
  // omnibox's instance for when it is used in the context of the WebUI omnibox.
  controller_emitter_observation_.Observe(
      AutocompleteControllerEmitter::GetForBrowserContext(profile_));

  controller_ = std::make_unique<OmniboxController>(
      /*view=*/nullptr,
      /*edit_model_delegate=*/this,
      std::make_unique<RealboxOmniboxClient>(profile_, web_contents_));
}

RealboxHandler::~RealboxHandler() = default;

void RealboxHandler::SetPage(
    mojo::PendingRemote<omnibox::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void RealboxHandler::QueryAutocomplete(const std::u16string& input,
                                       bool prevent_inline_autocomplete) {
  // TODO(tommycli): We use the input being empty as a signal we are requesting
  // on-focus suggestions. It would be nice if we had a more explicit signal.
  bool is_on_focus = input.empty();

  // Early exit if a query is already in progress for on focus inputs.
  if (!autocomplete_controller()->done() && is_on_focus) {
    return;
  }

  if (time_user_first_modified_realbox_.is_null() && !is_on_focus)
    time_user_first_modified_realbox_ = base::TimeTicks::Now();

  AutocompleteInput autocomplete_input(
      input, metrics::OmniboxEventProto::NTP_REALBOX,
      ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_input.set_focus_type(
      is_on_focus ? metrics::OmniboxFocusType::INTERACTION_FOCUS
                  : metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);

  // We do not want keyword matches for the NTP realbox, which has no UI
  // facilities to support them.
  autocomplete_input.set_prefer_keyword(false);
  autocomplete_input.set_allow_exact_keyword_match(false);

  autocomplete_controller()->Start(autocomplete_input);
}

void RealboxHandler::StopAutocomplete(bool clear_result) {
  autocomplete_controller()->Stop(clear_result);

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
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  edit_model()->OpenSelection(OmniboxPopupSelection(line), timestamp,
                              disposition);
}

void RealboxHandler::OnNavigationLikely(
    uint8_t line,
    const GURL& url,
    omnibox::mojom::NavigationPredictor navigation_predictor) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  if (auto* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    search_prefetch_service->OnNavigationLikely(
        line, *match, navigation_predictor, web_contents_);
  }
}

void RealboxHandler::DeleteAutocompleteMatch(uint8_t line, const GURL& url) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match || !match->SupportsDeletion()) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  autocomplete_controller()->Stop(false);
  autocomplete_controller()->DeleteMatch(*match);
}

void RealboxHandler::ToggleSuggestionGroupIdVisibility(
    int32_t suggestion_group_id) {
  const auto& group_id = omnibox::GroupIdForNumber(suggestion_group_id);
  DCHECK_NE(omnibox::GROUP_INVALID, group_id);
  const bool current_visibility =
      autocomplete_controller()->result().IsSuggestionGroupHidden(
          profile_->GetPrefs(), group_id);
  autocomplete_controller()->result().SetSuggestionGroupHidden(
      profile_->GetPrefs(), group_id, !current_visibility);
}

void RealboxHandler::ExecuteAction(uint8_t line,
                                   uint8_t action_index,
                                   const GURL& url,
                                   base::TimeTicks match_selection_timestamp,
                                   uint8_t mouse_button,
                                   bool alt_key,
                                   bool ctrl_key,
                                   bool meta_key,
                                   bool shift_key) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  if (action_index >= match->actions.size()) {
    return;
  }
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  OmniboxPopupSelection selection(
      line, OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION,
      action_index);
  edit_model()->OpenSelection(selection, match_selection_timestamp,
                              disposition);
}

void RealboxHandler::OnResultChanged(AutocompleteController* controller,
                                     bool default_match_changed) {
  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("ResultChanged")) {
    metrics_reporter_->Mark("ResultChanged");
  }

  // Update the omnibox if the controller does not belong to the realbox.
  if (controller != autocomplete_controller()) {
    if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
      page_->OmniboxAutocompleteResultChanged(CreateAutocompleteResult(
          controller->input().text(), controller->result(),
          BookmarkModelFactory::GetForBrowserContext(profile_),
          profile_->GetPrefs()));
    }
    return;
  }

  // Update the realbox only if the controller belongs to the realbox.
  page_->AutocompleteResultChanged(CreateAutocompleteResult(
      autocomplete_controller()->input().text(),
      autocomplete_controller()->result(),
      BookmarkModelFactory::GetForBrowserContext(profile_),
      profile_->GetPrefs()));

  if (autocomplete_controller()->done()) {
    if (SearchPrefetchService* search_prefetch_service =
            SearchPrefetchServiceFactory::GetForProfile(profile_)) {
      search_prefetch_service->OnResultChanged(
          web_contents_, autocomplete_controller()->result());
    }
  }
}

void RealboxHandler::SelectMatchAtLine(size_t old_line, size_t new_line) {
  page_->SelectMatchAtLine(new_line);
}

// OmniboxEditModelDelegate:
void RealboxHandler::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    IDNA2008DeviationCharacter deviation_char_in_hostname) {
  destination_url_ = destination_url;
  post_content_ = post_content;
  disposition_ = disposition;
  transition_ = transition;
  match_selection_timestamp_ = match_selection_timestamp;
  destination_url_entered_without_scheme_ =
      destination_url_entered_without_scheme;
  destination_url_entered_with_http_scheme_ =
      destination_url_entered_with_http_scheme;

  web_contents_->OpenURL(content::OpenURLParams(
      destination_url_, content::Referrer(), disposition_, transition_, false));
}

void RealboxHandler::OnInputInProgress(bool in_progress) {}

void RealboxHandler::OnChanged() {}

void RealboxHandler::OnPopupVisibilityChanged() {}

LocationBarModel* RealboxHandler::GetLocationBarModel() {
  return this;
}

const LocationBarModel* RealboxHandler::GetLocationBarModel() const {
  return this;
}

// LocationBarModel:
// Note, the implementation here is mostly not needed but the
// `OmniboxEditModelDelegate` implementation currently needs to
// provide a full working instance and some parts are used.
std::u16string RealboxHandler::GetFormattedFullURL() const {
  return u"";
}

std::u16string RealboxHandler::GetURLForDisplay() const {
  return u"";
}

GURL RealboxHandler::GetURL() const {
  return GURL();
}

security_state::SecurityLevel RealboxHandler::GetSecurityLevel() const {
  return security_state::SecurityLevel::NONE;
}

net::CertStatus RealboxHandler::GetCertStatus() const {
  return 0;
}

metrics::OmniboxEventProto::PageClassification
RealboxHandler::GetPageClassification(OmniboxFocusSource focus_source,
                                      bool is_prefetch) {
  return metrics::OmniboxEventProto::NTP_REALBOX;
}

const gfx::VectorIcon& RealboxHandler::GetVectorIcon() const {
  return vector_icon_;
}

std::u16string RealboxHandler::GetSecureDisplayText() const {
  return u"";
}

std::u16string RealboxHandler::GetSecureAccessibilityText() const {
  return u"";
}

bool RealboxHandler::ShouldDisplayURL() const {
  return false;
}

bool RealboxHandler::IsOfflinePage() const {
  return false;
}

bool RealboxHandler::ShouldPreventElision() const {
  return false;
}

bool RealboxHandler::ShouldUseUpdatedConnectionSecurityIndicators() const {
  return false;
}

OmniboxEditModel* RealboxHandler::edit_model() const {
  return controller_->edit_model();
}

AutocompleteController* RealboxHandler::autocomplete_controller() const {
  return controller_->autocomplete_controller();
}

const AutocompleteMatch* RealboxHandler::GetMatchWithUrl(size_t index,
                                                         const GURL& url) {
  const AutocompleteResult& result = autocomplete_controller()->result();
  if (index >= result.size()) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return nullptr;
  }
  const AutocompleteMatch& match = result.match_at(index);
  if (match.destination_url != url) {
    // This can happen also, for the same reason. We could search the result
    // for the match with this URL, but there would be no guarantee that it's
    // the same match, so for this edge case we treat result mismatch as none.
    return nullptr;
  }
  return &match;
}
