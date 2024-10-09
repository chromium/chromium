// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/clipboard_provider.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/base/clipboard/clipboard.h"  // nogncheck
#endif                                    // !BUILDFLAG(IS_IOS)

namespace {
constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);

const size_t kMaxClipboardSuggestionShownNumTimesSimpleSize = 20;

// Clipboard suggestion is placed either in a dedicated
// SECTION_MOBILE_CLIPBOARD, or SECTION_PERSONALIZED_ZERO_SUGGEST.
// The score for the former is irrelevant, but for the latter we need to be
// confident the suggestion shows up on top.
const int kClipboardMatchRelevanceScore = 1600;

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
  } else if (match_type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
    UMA_HISTOGRAM_LONG_TIMES_100("Omnibox.ClipboardSuggestionRemovedAge.IMAGE",
                                 clipboard_contents_age);
  }
}
}  // namespace

ClipboardProvider::ClipboardProvider(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener,
                                     ClipboardRecentContent* clipboard_content)
    : AutocompleteProvider(AutocompleteProvider::TYPE_CLIPBOARD),
      client_(client),
      clipboard_content_(clipboard_content),
      current_url_suggested_times_(0) {
  DCHECK(clipboard_content_);
  AddListener(listener);
}

ClipboardProvider::~ClipboardProvider() = default;

void ClipboardProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  using OEP = ::metrics::OmniboxEventProto;

  matches_.clear();

  // If the user started typing, do not offer clipboard based match.
  if (!input.IsZeroSuggest()) {
    return;
  }

  auto page_class = input.current_page_classification();
  if (page_class == OEP::OTHER_ON_CCT ||
      page_class == OEP::SEARCH_RESULT_PAGE_ON_CCT) {
    return;
  }

  // Image matched was kicked off asynchronously, so proceed when that ends.
  if (!input.omit_asynchronous_matches() && CreateImageMatch(input))
    return;

  bool read_clipboard_content = false;
  bool read_clipboard_url;
  std::optional<AutocompleteMatch> optional_match =
      CreateURLMatch(input, &read_clipboard_url);
  read_clipboard_content |= read_clipboard_url;
  if (!optional_match) {
    bool read_clipboard_text;
    optional_match = CreateTextMatch(input, &read_clipboard_text);
    read_clipboard_content |= read_clipboard_text;
  }

  if (optional_match) {
    AddCreatedMatchWithTracking(input, std::move(optional_match).value(),
                                clipboard_content_->GetClipboardContentAge());
    return;
  }

  // If there was clipboard content, but no match, don't proceed. There was
  // some other reason for not creating a match (e.g. copied URL but the URL was
  // the same as the current URL).
  if (read_clipboard_content) {
    return;
  }

  done_ = true;

  // On iOS and Android, accessing the clipboard contents shows a notification
  // to the user. To avoid this, all the methods above will not check the
  // contents and will return false/std::nullopt. Instead, check the existence
  // of content without accessing the actual content and create blank matches.
  if (!input.omit_asynchronous_matches()) {
    // Image matched was kicked off asynchronously, so proceed when that ends.
    CheckClipboardContent(input);
  }
}

void ClipboardProvider::Stop(bool clear_cached_results,
                             bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);

  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void ClipboardProvider::DeleteMatch(const AutocompleteMatch& match) {
  RecordDeletingClipboardSuggestionMetrics(
      match.type, clipboard_content_->GetClipboardContentAge());
  clipboard_content_->ClearClipboardContent();

  const auto pred = [&match](const AutocompleteMatch& i) {
    return i.contents == match.contents && i.type == match.type;
  };
  std::erase_if(matches_, pred);
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
}

void ClipboardProvider::AddCreatedMatchWithTracking(
    const AutocompleteInput& input,
    AutocompleteMatch match,
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

  RecordCreatingClipboardSuggestionMetrics(current_url_suggested_times_,
                                           matches_.empty(), match.type,
                                           clipboard_contents_age);

  if (is_android &&
      omnibox::IsNTPPage(input.current_page_classification())) {
    // Assign the Clipboard to the PZPS group on NTP pages to improve the use
    // of the suggest space.
    match.suggestion_group_id = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
  } else {
    // Leave the clipboard in its dedicated section otherwise.
    match.suggestion_group_id = omnibox::GROUP_MOBILE_CLIPBOARD;
  }

  matches_.push_back(match);
}

bool ClipboardProvider::TemplateURLSupportsTextSearch() {
  TemplateURLService* url_service = client_->GetTemplateURLService();
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  if (!default_url)
    return false;

  DCHECK(!default_url->url().empty());
  DCHECK(default_url->url_ref().IsValid(url_service->search_terms_data()));
  return true;
}

bool ClipboardProvider::TemplateURLSupportsImageSearch() {
  TemplateURLService* url_service = client_->GetTemplateURLService();
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();

  return default_url && !default_url->image_url().empty() &&
         default_url->image_url_ref().IsValid(url_service->search_terms_data());
}

void ClipboardProvider::CheckClipboardContent(const AutocompleteInput& input) {
  std::set<ClipboardContentType> desired_types;
  desired_types.insert(ClipboardContentType::URL);

  if (TemplateURLSupportsTextSearch()) {
    desired_types.insert(ClipboardContentType::Text);
  }

  if (TemplateURLSupportsImageSearch()) {
    desired_types.insert(ClipboardContentType::Image);
  }

  done_ = false;

  // We want to get the age here because the contents of the clipboard could
  // change after this point. We want the age of the contents we actually use,
  // not the age of whatever's on the clipboard when the histogram is created
  // (i.e when the match is created).
  base::TimeDelta clipboard_contents_age =
      clipboard_content_->GetClipboardContentAge();
  clipboard_content_->HasRecentContentFromClipboard(
      desired_types,
      base::BindOnce(&ClipboardProvider::OnReceiveClipboardContent,
                     callback_weak_ptr_factory_.GetWeakPtr(), input,
                     clipboard_contents_age));
}

void ClipboardProvider::OnReceiveClipboardContent(
    const AutocompleteInput& input,
    base::TimeDelta clipboard_contents_age,
    std::set<ClipboardContentType> matched_types) {
  if (TemplateURLSupportsImageSearch() &&
      matched_types.find(ClipboardContentType::Image) != matched_types.end()) {
    // The image content will be added in later. If the image is large, encoding
    // the image may take some time, so just be wary whenever that step happens
    // (e.g OmniboxView::OpenMatch).
    AutocompleteMatch match = NewBlankImageMatch();
    AddCreatedMatchWithTracking(input, std::move(match),
                                clipboard_contents_age);
    NotifyListeners(true);
  } else if (matched_types.find(ClipboardContentType::URL) !=
             matched_types.end()) {
    AutocompleteMatch match = NewBlankURLMatch();
    AddCreatedMatchWithTracking(input, std::move(match),
                                clipboard_contents_age);
    NotifyListeners(true);
  } else if (TemplateURLSupportsTextSearch() &&
             matched_types.find(ClipboardContentType::Text) !=
                 matched_types.end()) {
    AutocompleteMatch match = NewBlankTextMatch();
    AddCreatedMatchWithTracking(input, std::move(match),
                                clipboard_contents_age);
    NotifyListeners(true);
  }
  done_ = true;
}

std::optional<AutocompleteMatch> ClipboardProvider::CreateURLMatch(
    const AutocompleteInput& input,
    bool* read_clipboard_content) {
  *read_clipboard_content = false;
  if (base::FeatureList::IsEnabled(
          omnibox::kClipboardSuggestionContentHidden)) {
    return std::nullopt;
  }
  // The clipboard does not contain a URL worth suggesting.
  std::optional<GURL> optional_gurl =
      clipboard_content_->GetRecentURLFromClipboard();
  if (!optional_gurl)
    return std::nullopt;

  *read_clipboard_content = true;
  GURL url = std::move(optional_gurl).value();

  // The URL on the page is the same as the URL in the clipboard.  Don't
  // bother suggesting it.
  if (url == input.current_url())
    return std::nullopt;

  return NewClipboardURLMatch(url);
}

std::optional<AutocompleteMatch> ClipboardProvider::CreateTextMatch(
    const AutocompleteInput& input,
    bool* read_clipboard_content) {
  *read_clipboard_content = false;
  if (base::FeatureList::IsEnabled(
          omnibox::kClipboardSuggestionContentHidden)) {
    return std::nullopt;
  }

  if (!TemplateURLSupportsTextSearch()) {
    return std::nullopt;
  }

  std::optional<std::u16string> optional_text =
      clipboard_content_->GetRecentTextFromClipboard();
  if (!optional_text)
    return std::nullopt;

  *read_clipboard_content = true;
  std::u16string text = std::move(optional_text).value();

  // The clipboard can contain the empty string, which shouldn't be suggested.
  if (text.empty())
    return std::nullopt;

  // The text in the clipboard is a url. We don't want to prompt the user to
  // search for a url.
  if (GURL(text).is_valid())
    return std::nullopt;

  return NewClipboardTextMatch(text);
}

bool ClipboardProvider::CreateImageMatch(const AutocompleteInput& input) {
  if (base::FeatureList::IsEnabled(
          omnibox::kClipboardSuggestionContentHidden)) {
    return false;
  }

  if (!clipboard_content_->HasRecentImageFromClipboard()) {
    return false;
  }

  if (!TemplateURLSupportsImageSearch()) {
    return false;
  }

  done_ = false;

  // We want to get the age here because the contents of the clipboard could
  // change after this point. We want the age of the image we actually use, not
  // the age of whatever's on the clipboard when the histogram is created (i.e
  // when the match is created).
  base::TimeDelta clipboard_contents_age =
      clipboard_content_->GetClipboardContentAge();
  clipboard_content_->GetRecentImageFromClipboard(base::BindOnce(
      &ClipboardProvider::CreateImageMatchCallback,
      callback_weak_ptr_factory_.GetWeakPtr(), input, clipboard_contents_age));
  return true;
}

void ClipboardProvider::CreateImageMatchCallback(
    const AutocompleteInput& input,
    const base::TimeDelta clipboard_contents_age,
    std::optional<gfx::Image> optional_image) {
  NewClipboardImageMatch(
      optional_image, base::BindOnce(&ClipboardProvider::AddImageMatchCallback,
                                     callback_weak_ptr_factory_.GetWeakPtr(),
                                     input, clipboard_contents_age));
}

void ClipboardProvider::AddImageMatchCallback(
    const AutocompleteInput& input,
    const base::TimeDelta clipboard_contents_age,
    std::optional<AutocompleteMatch> match) {
  if (!match) {
    return;
  }
  AddCreatedMatchWithTracking(input, std::move(match).value(),
                              clipboard_contents_age);
  NotifyListeners(true);
  done_ = true;
}

AutocompleteMatch ClipboardProvider::NewBlankURLMatch() {
  AutocompleteMatch match(this, kClipboardMatchRelevanceScore,
                          IsMatchDeletionEnabled(),
                          AutocompleteMatchType::CLIPBOARD_URL);

  match.description.assign(l10n_util::GetStringUTF16(IDS_LINK_FROM_CLIPBOARD));
  if (!match.description.empty())
    match.description_class.push_back({0, ACMatchClassification::NONE});
  return match;
}

AutocompleteMatch ClipboardProvider::NewClipboardURLMatch(GURL url) {
  DCHECK(url.is_valid());

  AutocompleteMatch match = NewBlankURLMatch();

  UpdateClipboardURLContent(url, &match);

  return match;
}

AutocompleteMatch ClipboardProvider::NewBlankTextMatch() {
  AutocompleteMatch match(this, kClipboardMatchRelevanceScore,
                          IsMatchDeletionEnabled(),
                          AutocompleteMatchType::CLIPBOARD_TEXT);
  // Any path leading here should first verify whether
  // TemplateUrlSupportsTextSearch().
  TemplateURLService* url_service = client_->GetTemplateURLService();
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  DCHECK(!!default_url);
  match.keyword = default_url->keyword();

  match.description.assign(l10n_util::GetStringUTF16(IDS_TEXT_FROM_CLIPBOARD));
  if (!match.description.empty())
    match.description_class.push_back({0, ACMatchClassification::NONE});

  match.transition = ui::PAGE_TRANSITION_GENERATED;
  return match;
}

std::optional<AutocompleteMatch> ClipboardProvider::NewClipboardTextMatch(
    std::u16string text) {
  AutocompleteMatch match = NewBlankTextMatch();

  if (!UpdateClipboardTextContent(text, &match))
    return std::nullopt;

  return match;
}

AutocompleteMatch ClipboardProvider::NewBlankImageMatch() {
  AutocompleteMatch match(this, kClipboardMatchRelevanceScore,
                          IsMatchDeletionEnabled(),
                          AutocompleteMatchType::CLIPBOARD_IMAGE);
  // Any path leading here should first verify whether
  // TemplateUrlSupportsImageSearch().
  TemplateURLService* url_service = client_->GetTemplateURLService();
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  DCHECK(!!default_url);
  match.keyword = default_url->keyword();

  match.description.assign(l10n_util::GetStringUTF16(IDS_IMAGE_FROM_CLIPBOARD));
  if (!match.description.empty())
    match.description_class.push_back({0, ACMatchClassification::NONE});

  // This will end up being something like "Search for Copied Image." This may
  // seem strange to use for |fill_into_edit|, but it is because iOS requires
  // some text in the text field for the Enter key to work when using keyboard
  // navigation.
  match.fill_into_edit = match.description;
  match.transition = ui::PAGE_TRANSITION_GENERATED;

  return match;
}

void ClipboardProvider::NewClipboardImageMatch(
    std::optional<gfx::Image> optional_image,
    ClipboardImageMatchCallback callback) {
  // ImageSkia::ToImageSkia should only be called if the gfx::Image is
  // non-empty. It is unclear when the clipboard returns a non-optional but
  // empty image. See crbug.com/1136759 for more details.
  if (!optional_image || optional_image.value().IsEmpty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  gfx::ImageSkia image_skia = *optional_image.value().ToImageSkia();
  image_skia.MakeThreadSafe();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ClipboardProvider::EncodeClipboardImage, image_skia),
      base::BindOnce(&ClipboardProvider::ConstructImageMatchCallback,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ClipboardProvider::UpdateClipboardMatchWithContent(
    AutocompleteMatch* match,
    ClipboardMatchCallback callback) {
  DCHECK(match);
  if (match->type == AutocompleteMatchType::CLIPBOARD_URL) {
    clipboard_content_->GetRecentURLFromClipboard(base::BindOnce(
        &ClipboardProvider::OnReceiveURLForMatchWithContent,
        callback_weak_ptr_factory_.GetWeakPtr(), std::move(callback), match));
    return;
  } else if (match->type == AutocompleteMatchType::CLIPBOARD_TEXT) {
    clipboard_content_->GetRecentTextFromClipboard(base::BindOnce(
        &ClipboardProvider::OnReceiveTextForMatchWithContent,
        callback_weak_ptr_factory_.GetWeakPtr(), std::move(callback), match));
    return;
  } else if (match->type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
    clipboard_content_->GetRecentImageFromClipboard(base::BindOnce(
        &ClipboardProvider::OnReceiveImageForMatchWithContent,
        callback_weak_ptr_factory_.GetWeakPtr(), std::move(callback), match));
    return;
  }
}

scoped_refptr<base::RefCountedMemory> ClipboardProvider::EncodeClipboardImage(
    gfx::ImageSkia image_skia) {
  gfx::Image resized_image =
      gfx::ResizedImageForSearchByImage(gfx::Image(image_skia));
  return resized_image.As1xPNGBytes();
}

void ClipboardProvider::ConstructImageMatchCallback(
    ClipboardImageMatchCallback callback,
    scoped_refptr<base::RefCountedMemory> image_bytes) {
  TemplateURLService* url_service = client_->GetTemplateURLService();
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  DCHECK(default_url);

  AutocompleteMatch match = NewBlankImageMatch();

  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(u"");
  match.search_terms_args->image_thumbnail_content.assign(
      base::as_string_view(*image_bytes));
  TemplateURLRef::PostContent post_content;
  GURL result(default_url->image_url_ref().ReplaceSearchTerms(
      *match.search_terms_args.get(), url_service->search_terms_data(),
      &post_content));

  if (!base::FeatureList::IsEnabled(omnibox::kImageSearchSuggestionThumbnail)) {
    // If Omnibox image suggestion do not need thumbnail, release memory.
    match.search_terms_args.reset();
  }
  match.destination_url = result;
  match.post_content =
      std::make_unique<TemplateURLRef::PostContent>(post_content);

  std::move(callback).Run(match);
}

void ClipboardProvider::OnReceiveURLForMatchWithContent(
    ClipboardMatchCallback callback,
    AutocompleteMatch* match,
    std::optional<GURL> optional_gurl) {
  if (!optional_gurl)
    return;

  GURL url = std::move(optional_gurl).value();
  UpdateClipboardURLContent(url, match);

  std::move(callback).Run();
}

void ClipboardProvider::OnReceiveTextForMatchWithContent(
    ClipboardMatchCallback callback,
    AutocompleteMatch* match,
    std::optional<std::u16string> optional_text) {
  if (!optional_text)
    return;

  std::u16string text = std::move(optional_text).value();
  if (!UpdateClipboardTextContent(text, match))
    return;

  std::move(callback).Run();
}

void ClipboardProvider::OnReceiveImageForMatchWithContent(
    ClipboardMatchCallback callback,
    AutocompleteMatch* match,
    std::optional<gfx::Image> optional_image) {
  if (!optional_image)
    return;

  gfx::Image image = std::move(optional_image).value();
  NewClipboardImageMatch(
      image,
      base::BindOnce(&ClipboardProvider::OnReceiveImageMatchForMatchWithContent,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), match));
}

void ClipboardProvider::OnReceiveImageMatchForMatchWithContent(
    ClipboardMatchCallback callback,
    AutocompleteMatch* match,
    std::optional<AutocompleteMatch> optional_match) {
  DCHECK(match);
  if (!optional_match)
    return;

  match->destination_url = std::move(optional_match->destination_url);
  match->post_content = std::move(optional_match->post_content);
  match->search_terms_args = std::move(optional_match->search_terms_args);

  std::move(callback).Run();
}

void ClipboardProvider::UpdateClipboardURLContent(const GURL& url,
                                                  AutocompleteMatch* match) {
  DCHECK(url.is_valid());
  DCHECK(match);

  match->destination_url = url;

  // Because the user did not type a related input to get this clipboard
  // suggestion, preserve the subdomain so the user has extra context.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, true);
  match->contents.assign(url_formatter::FormatUrl(url, format_types,
                                                  base::UnescapeRule::SPACES,
                                                  nullptr, nullptr, nullptr));
  if (!match->contents.empty())
    match->contents_class.push_back({0, ACMatchClassification::URL});
  match->fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, match->contents, client_->GetSchemeClassifier(), nullptr);
}

bool ClipboardProvider::UpdateClipboardTextContent(const std::u16string& text,
                                                   AutocompleteMatch* match) {
  DCHECK(match);

  // The text in the clipboard is a url. We don't want to prompt the user to
  // search for a url.
  if (GURL(text).is_valid()) {
    // Note: on Android, the clipboard content is evaluated by Android
    // Framework. The Framework is familiar with only a handful of URL schemes,
    // and any non-explicitly annotated URL with scheme not recognized by the
    // Android is immediately annotated as Text. Additionally, any application
    // setting clipboard content may supply its own annotation, which may be
    // inaccurate.
    // we do not have the control over all sources from where such URLs can come
    // from. The change below allows us to still open these URLs. Without this
    // change Clipboard suggestions may be non interactable, if the clipboard
    // contains an unannotated or mis-classified URL not recognized by Android.
    if constexpr (is_android) {
      UpdateClipboardURLContent(GURL(text), match);
      return true;
    }
    return false;
  }

  match->fill_into_edit = text;

  TemplateURLService* url_service = client_->GetTemplateURLService();
  const TemplateURL* default_url = url_service->GetDefaultSearchProvider();
  if (!default_url)
    return false;

  DCHECK(!default_url->url().empty());
  DCHECK(default_url->url_ref().IsValid(url_service->search_terms_data()));
  TemplateURLRef::SearchTermsArgs search_args(text);
  GURL result(default_url->url_ref().ReplaceSearchTerms(
      search_args, url_service->search_terms_data()));

  match->destination_url = result;
  match->contents.assign(AutocompleteMatch::SanitizeString(text));
  if (!match->contents.empty())
    match->contents_class.push_back({0, ACMatchClassification::NONE});

  match->keyword = default_url->keyword();

  return true;
}
