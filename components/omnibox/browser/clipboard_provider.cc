// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/clipboard_provider.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_util.h"

namespace {

const size_t kMaxClipboardSuggestionShownNumTimesSimpleSize = 20;

bool IsMatchDeletionEnabled() {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxRemoveSuggestionsFromClipboard);
}

void RecordCreatingClipboardSuggestionMetrics(
    size_t current_url_suggested_times,
    bool matches_is_empty,
    AutocompleteMatchType::Type match_type,
    const base::TimeDelta clipboard_contents_age) {
  DCHECK(match_type == AutocompleteMatchType::CLIPBOARD_URL ||
         match_type == AutocompleteMatchType::CLIPBOARD_TEXT ||
         match_type == AutocompleteMatchType::CLIPBOARD_IMAGE);

  base::UmaHistogramSparse(
      "Omnibox.ClipboardSuggestionShownNumTimes",
      std::min(current_url_suggested_times,
               kMaxClipboardSuggestionShownNumTimesSimpleSize));
  UMA_HISTOGRAM_BOOLEAN("Omnibox.ClipboardSuggestionShownWithCurrentURL",
                        !matches_is_empty);
  UMA_HISTOGRAM_LONG_TIMES_100("Omnibox.ClipboardSuggestionShownAge",
                               clipboard_contents_age);
  if (match_type == AutocompleteMatchType::CLIPBOARD_URL) {
    base::UmaHistogramSparse(
        "Omnibox.ClipboardSuggestionShownNumTimes.URL",
        std::min(current_url_suggested_times,
                 kMaxClipboardSuggestionShownNumTimesSimpleSize));
    UMA_HISTOGRAM_BOOLEAN("Omnibox.ClipboardSuggestionShownWithCurrentURL.URL",
                          !matches_is_empty);
    UMA_HISTOGRAM_LONG_TIMES_100("Omnibox.ClipboardSuggestionShownAge.URL",
                                 clipboard_contents_age);
  } else if (match_type == AutocompleteMatchType::CLIPBOARD_TEXT) {
    base::UmaHistogramSparse(
        "Omnibox.ClipboardSuggestionShownNumTimes.TEXT",
        std::min(current_url_suggested_times,
                 kMaxClipboardSuggestionShownNumTimesSimpleSize));
    UMA_HISTOGRAM_BOOLEAN("Omnibox.ClipboardSuggestionShownWithCurrentURL.TEXT",
                          !matches_is_empty);
    UMA_HISTOGRAM_LONG_TIMES_100("Omnibox.ClipboardSuggestionShownAge.TEXT",
                                 clipboard_contents_age);
  } else if (match_type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
    base::UmaHistogramSparse(
        "Omnibox.ClipboardSuggestionShownNumTimes.IMAGE",
        std::min(current_url_suggested_times,
                 kMaxClipboardSuggestionShownNumTimesSimpleSize));
    UMA_HISTOGRAM_BOOLEAN(
        "Omnibox.ClipboardSuggestionShownWithCurrentURL.IMAGE",
        !matches_is_empty);
    UMA_HISTOGRAM_LONG_TIMES_100("Omnibox.ClipboardSuggestionShownAge.IMAGE",
                                 clipboard_contents_age);
  }
}

void RecordDeletingClipboardSuggestionMetrics(
    AutocompleteMatchType::Type match_type,
    const base::TimeDelta clipboard_contents_age) {
  base::RecordAction(
      base::UserMetricsAction("Omnibox.ClipboardSuggestionRemoved"));

  UMA_HISTOGRAM_LONG_TIMES_100("Omnibox.ClipboardSuggestionRemovedAge",
                               clipboard_contents_age);
  if (match_type == AutocompleteMatchType::CLIPBOARD_URL) {
    UMA_HISTOGRAM_LONG_TIMES_100("Omnibox.ClipboardSuggestionRemovedAge.URL",
                                 clipboard_contents_age);
  } else if (match_type == AutocompleteMatchType::CLIPBOARD_TEXT) {
    UMA_HISTOGRAM_LONG_TIMES_100("Omnibox.ClipboardSuggestionRemovedAge.TEXT",
                                 clipboard_contents_age);
  }
}

}  // namespace

ClipboardProvider::ClipboardProvider(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener,
                                     HistoryURLProvider* history_url_provider,
                                     ClipboardRecentContent* clipboard_content)
    : AutocompleteProvider(AutocompleteProvider::TYPE_CLIPBOARD),
      client_(client),
      listener_(listener),
      clipboard_content_(clipboard_content),
      history_url_provider_(history_url_provider),
      current_url_suggested_times_(0),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false) {
  DCHECK(clipboard_content_);
}

ClipboardProvider::~ClipboardProvider() {}

void ClipboardProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  matches_.clear();
  field_trial_triggered_ = false;

  // If the user started typing, do not offer clipboard based match.
  if (!input.from_omnibox_focus())
    return;

  // Image matched was kicked off asynchronously, so proceed when that ends.
  if (CreateImageMatch(input))
    return;

  base::Optional<AutocompleteMatch> optional_match = CreateURLMatch(input);
  if (!optional_match)
    optional_match = CreateTextMatch(input);

  // The clipboard does not contain any suggestions
  if (!optional_match)
    return;

  AddCreatedMatchWithTracking(input, std::move(optional_match).value(),
                              clipboard_content_->GetClipboardContentAge());
}

void ClipboardProvider::Stop(bool clear_cached_results,
                             bool due_to_user_inactivity) {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);
}

void ClipboardProvider::DeleteMatch(const AutocompleteMatch& match) {
  RecordDeletingClipboardSuggestionMetrics(
      match.type, clipboard_content_->GetClipboardContentAge());
  clipboard_content_->ClearClipboardContent();

  const auto pred = [&match](const AutocompleteMatch& i) {
    return i.contents == match.contents && i.type == match.type;
  };
  base::EraseIf(matches_, pred);
}

void ClipboardProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  // If a URL wasn't suggested on this most recent focus event, don't bother
  // setting |times_returned_results_in_session|, as in effect this URL has
  // never been suggested during the current session.  (For the purpose of
  // this provider, we define a session as intervals between when a URL
  // clipboard suggestion changes.)
  if (current_url_suggested_times_ == 0)
    return;
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(AsOmniboxEventProviderType());
  new_entry.set_provider_done(done_);
  new_entry.set_times_returned_results_in_session(current_url_suggested_times_);

  if (field_trial_triggered_ || field_trial_triggered_in_session_) {
    std::vector<uint32_t> field_trial_hashes;
    OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
    for (uint32_t trial : field_trial_hashes) {
      if (field_trial_triggered_) {
        new_entry.mutable_field_trial_triggered()->Add(trial);
      }
      if (field_trial_triggered_in_session_) {
        new_entry.mutable_field_trial_triggered_in_session()->Add(trial);
      }
    }
  }
}

void ClipboardProvider::ResetSession() {
  field_trial_triggered_ = false;
  field_trial_triggered_in_session_ = false;
}

void ClipboardProvider::AddCreatedMatchWithTracking(
    const AutocompleteInput& input,
    const AutocompleteMatch& match,
    const base::TimeDelta clipboard_contents_age) {
  // Record the number of times the currently-offered URL has been suggested.
  // This only works over this run of Chrome; if the URL was in the clipboard
  // on a previous run, those offerings will not be counted.
  if (match.destination_url == current_url_suggested_) {
    current_url_suggested_times_++;
  } else {
    current_url_suggested_ = match.destination_url;
    current_url_suggested_times_ = 1;
  }


  // If the omnibox is not empty, add a default match.
  // This match will be opened when the user presses "Enter".
  if (!input.text().empty()) {
    const base::string16 description =
        (base::FeatureList::IsEnabled(omnibox::kDisplayTitleForCurrentUrl))
            ? input.current_title()
            : base::string16();
    AutocompleteMatch verbatim_match =
        VerbatimMatchForURL(client_, input, input.current_url(), description,
                            history_url_provider_, -1);
    matches_.push_back(verbatim_match);
  }

  RecordCreatingClipboardSuggestionMetrics(current_url_suggested_times_,
                                           matches_.empty(), match.type,
                                           clipboard_contents_age);

  matches_.push_back(match);
}

base::Optional<AutocompleteMatch> ClipboardProvider::CreateURLMatch(
    const AutocompleteInput& input) {
  // The clipboard does not contain a URL worth suggesting.
  base::Optional<GURL> optional_gurl =
      clipboard_content_->GetRecentURLFromClipboard();
  if (!optional_gurl)
    return base::nullopt;

  GURL url = std::move(optional_gurl).value();

  // The URL on the page is the same as the URL in the clipboard.  Don't
  // bother suggesting it.
  if (url == input.current_url())
    return base::nullopt;

  DCHECK(url.is_valid());

  // Add the clipboard match. The relevance is 800 to beat ZeroSuggest results.
  AutocompleteMatch match(this, 800, IsMatchDeletionEnabled(),
                          AutocompleteMatchType::CLIPBOARD_URL);
  match.destination_url = url;
  // Because the user did not type a related input to get this clipboard
  // suggestion, preserve the subdomain so the user has extra context.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, true);
  match.contents.assign(url_formatter::FormatUrl(
      url, format_types, net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
  if (!match.contents.empty())
    match.contents_class.push_back({0, ACMatchClassification::URL});

  match.description.assign(l10n_util::GetStringUTF16(IDS_LINK_FROM_CLIPBOARD));
  if (!match.description.empty())
    match.description_class.push_back({0, ACMatchClassification::NONE});

  return match;
}

base::Optional<AutocompleteMatch> ClipboardProvider::CreateTextMatch(
    const AutocompleteInput& input) {
  // Only try text match if feature is enabled
  if (!base::FeatureList::IsEnabled(
          omnibox::kEnableClipboardProviderTextSuggestions)) {
    return base::nullopt;
  }

  base::Optional<base::string16> optional_text =
      clipboard_content_->GetRecentTextFromClipboard();
  if (!optional_text)
    return base::nullopt;

  base::string16 text = std::move(optional_text).value();

  // The clipboard can contain the empty string, which shouldn't be suggested.
  if (text.empty())
    return base::nullopt;

  // The text in the clipboard is a url. We don't want to prompt the user to
  // search for a url.
  if (GURL(text).is_valid())
    return base::nullopt;

  // Add the clipboard match. The relevance is 800 to beat ZeroSuggest results.
  AutocompleteMatch match(this, 800, IsMatchDeletionEnabled(),
                          AutocompleteMatchType::CLIPBOARD_TEXT);
  TemplateURLService* url_service = client_->GetTemplateURLService();
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  if (!default_url)
    return base::nullopt;

  DCHECK(!default_url->url().empty());
  DCHECK(default_url->url_ref().IsValid(url_service->search_terms_data()));
  TemplateURLRef::SearchTermsArgs search_args(text);
  GURL result(default_url->url_ref().ReplaceSearchTerms(
      search_args, url_service->search_terms_data()));

  match.destination_url = result;
  match.contents.assign(l10n_util::GetStringFUTF16(
      IDS_COPIED_TEXT_FROM_CLIPBOARD, AutocompleteMatch::SanitizeString(text)));
  if (!match.contents.empty())
    match.contents_class.push_back({0, ACMatchClassification::NONE});

  match.description.assign(l10n_util::GetStringUTF16(IDS_TEXT_FROM_CLIPBOARD));
  if (!match.description.empty())
    match.description_class.push_back({0, ACMatchClassification::NONE});

  match.keyword = default_url->keyword();
  match.transition = ui::PAGE_TRANSITION_GENERATED;

  // Some users may be in a counterfactual study arm in which we perform all
  // necessary work but do not forward the autocomplete matches.
  bool in_counterfactual_group = base::GetFieldTrialParamByFeatureAsBool(
      omnibox::kEnableClipboardProviderTextSuggestions,
      "ClipboardProviderTextSuggestionsCounterfactualArm", false);
  field_trial_triggered_ = true;
  field_trial_triggered_in_session_ = true;
  if (in_counterfactual_group)
    return base::nullopt;

  return match;
}

bool ClipboardProvider::CreateImageMatch(const AutocompleteInput& input) {
  // Only try image match if feature is enabled
  if (!base::FeatureList::IsEnabled(
          omnibox::kEnableClipboardProviderImageSuggestions)) {
    return false;
  }

  base::Optional<gfx::Image> optional_image =
      clipboard_content_->GetRecentImageFromClipboard();
  if (!optional_image)
    return false;

  // Make sure current provider supports image search
  TemplateURLService* url_service = client_->GetTemplateURLService();
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();

  if (!default_url || default_url->image_url().empty() ||
      !default_url->image_url_ref().IsValid(url_service->search_terms_data())) {
    return false;
  }

  // We want to get the age here because the contents of the clipboard could
  // change after this point. We want the age of the image we actually use, not
  // the age of whatever's on the clipboard when the histogram is created (i.e
  // when the match is created).
  base::TimeDelta clipboard_contents_age =
      clipboard_content_->GetClipboardContentAge();
  done_ = false;
  PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ClipboardProvider::EncodeClipboardImage,
                     optional_image.value()),
      base::BindOnce(&ClipboardProvider::ConstructImageMatchCallback,
                     callback_weak_ptr_factory_.GetWeakPtr(), input,
                     url_service, clipboard_contents_age));
  return true;
}

scoped_refptr<base::RefCountedMemory> ClipboardProvider::EncodeClipboardImage(
    gfx::Image image) {
  gfx::Image resized_image = gfx::ResizedImageForSearchByImage(image);
  return resized_image.As1xPNGBytes();
}

void ClipboardProvider::ConstructImageMatchCallback(
    const AutocompleteInput& input,
    TemplateURLService* url_service,
    base::TimeDelta clipboard_contents_age,
    scoped_refptr<base::RefCountedMemory> image_bytes) {
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  DCHECK(default_url);
  // Add the clipboard match. The relevance is 800 to beat ZeroSuggest results.
  AutocompleteMatch match(this, 800, false,
                          AutocompleteMatchType::CLIPBOARD_IMAGE);

  match.description.assign(l10n_util::GetStringUTF16(IDS_IMAGE_FROM_CLIPBOARD));
  if (!match.description.empty())
    match.description_class.push_back({0, ACMatchClassification::NONE});

  TemplateURLRef::SearchTermsArgs search_args(base::ASCIIToUTF16(""));
  search_args.image_thumbnail_content.assign(image_bytes->front_as<char>(),
                                             image_bytes->size());
  TemplateURLRef::PostContent post_content;
  GURL result(default_url->image_url_ref().ReplaceSearchTerms(
      search_args, url_service->search_terms_data(), &post_content));
  match.destination_url = result;
  match.post_content =
      std::make_unique<TemplateURLRef::PostContent>(post_content);

  match.transition = ui::PAGE_TRANSITION_GENERATED;

  field_trial_triggered_ = true;
  field_trial_triggered_in_session_ = true;
  done_ = true;

  // Some users may be in a counterfactual study arm in which we perform all
  // necessary work but do not forward the autocomplete matches.
  bool in_counterfactual_group = base::GetFieldTrialParamByFeatureAsBool(
      omnibox::kEnableClipboardProviderImageSuggestions,
      "ClipboardProviderImageSuggestionsCounterfactualArm", false);
  if (!in_counterfactual_group) {
    AddCreatedMatchWithTracking(input, match, clipboard_contents_age);
    listener_->OnProviderUpdate(true);
  }
}

