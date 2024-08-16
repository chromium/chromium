// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/contextual_search_delegate_impl.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/contextual_search/core/browser/contextual_search_field_trial.h"
#include "components/contextual_search/core/browser/public.h"
#include "components/contextual_search/core/browser/resolved_search_term.h"
#include "components/contextual_search/core/proto/client_discourse_context.pb.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "url/gurl.h"

using content::RenderFrameHost;

namespace {

const char kContextualSearchResponseDisplayTextParam[] = "display_text";
const char kContextualSearchResponseSelectedTextParam[] = "selected_text";
const char kContextualSearchResponseSearchTermParam[] = "search_term";
const char kContextualSearchResponseLanguageParam[] = "lang";
const char kContextualSearchResponseMidParam[] = "mid";
const char kContextualSearchResponseResolvedTermParam[] = "resolved_term";
const char kContextualSearchPreventPreload[] = "prevent_preload";
const char kContextualSearchMentionsKey[] = "mentions";
const char kContextualSearchCaption[] = "caption";
const char kContextualSearchThumbnail[] = "thumbnail";
const char kContextualSearchAction[] = "action";
const char kContextualSearchCategory[] = "category";
const char kContextualSearchCardTag[] = "card_tag";
const char kContextualSearchSearchUrlFull[] = "search_url_full";
const char kContextualSearchSearchUrlPreload[] = "search_url_preload";
const char kRelatedSearchesSuggestions[] = "suggestions";

const char kActionCategoryAddress[] = "ADDRESS";
const char kActionCategoryEmail[] = "EMAIL";
const char kActionCategoryEvent[] = "EVENT";
const char kActionCategoryPhone[] = "PHONE";
const char kActionCategoryWebsite[] = "WEBSITE";

const char kContextualSearchServerEndpoint[] = "_/contextualsearch?";
const int kContextualSearchRequestCtxsVersion = 2;
const int kDesktopPartialTranslateCtxsVersion = 3;
const int kRelatedSearchesCtxsVersion = 4;

const int kContextualSearchMaxSelection = 1000;
const char kXssiEscape[] = ")]}'\n";
const char kDiscourseContextHeaderName[] = "X-Additional-Discourse-Context";
const char kDoPreventPreloadValue[] = "1";

// A commandline switch to enable debugging information to be sent and returned.
const char kContextualSearchDebugCommandlineSwitch[] =
    "contextual-search-debug";

// Populates and returns the discourse context.
const net::HttpRequestHeaders GetDiscourseContext(
    const ContextualSearchContext& context) {
  discourse_context::ClientDiscourseContext proto;
  discourse_context::Display* display = proto.add_display();
  display->set_uri(context.GetBasePageUrl().spec());

  discourse_context::Media* media = display->mutable_media();
  media->set_mime_type(context.GetBasePageEncoding());

  discourse_context::Selection* selection = display->mutable_selection();
  selection->set_content(base::UTF16ToUTF8(context.GetSurroundingText()));
  selection->set_start(context.GetStartOffset());
  selection->set_end(context.GetEndOffset());
  selection->set_is_uri_encoded(false);

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string encoded_context = base::Base64Encode(serialized);
  // The server memoizer expects a web-safe encoding.
  std::replace(encoded_context.begin(), encoded_context.end(), '+', '-');
  std::replace(encoded_context.begin(), encoded_context.end(), '/', '_');

  net::HttpRequestHeaders headers;
  headers.SetHeader(kDiscourseContextHeaderName, encoded_context);
  return headers;
}

}  // namespace

// Handles tasks for the ContextualSearchManager in a separable, testable way.
ContextualSearchDelegateImpl::ContextualSearchDelegateImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TemplateURLService* template_url_service)
    : url_loader_factory_(std::move(url_loader_factory)),
      template_url_service_(template_url_service),
      field_trial_(std::make_unique<ContextualSearchFieldTrial>()) {}

ContextualSearchDelegateImpl::~ContextualSearchDelegateImpl() = default;

void ContextualSearchDelegateImpl::GatherAndSaveSurroundingText(
    base::WeakPtr<ContextualSearchContext> context,
    content::WebContents* web_contents,
    SurroundingTextCallback callback) {
  DCHECK(web_contents);
  blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback
      get_text_callback = base::BindOnce(
          &ContextualSearchDelegateImpl::OnTextSurroundingSelectionAvailable,
          weak_ptr_factory_.GetWeakPtr(), context, callback);
  if (!context)
    return;

  context->SetBasePageEncoding(web_contents->GetEncoding());
  int surroundingTextSize = context->CanResolve()
                                ? field_trial_->GetResolveSurroundingSize()
                                : field_trial_->GetSampleSurroundingSize();
  RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
  if (focused_frame) {
    focused_frame->RequestTextSurroundingSelection(std::move(get_text_callback),
                                                   surroundingTextSize);
  } else {
    std::move(get_text_callback).Run(std::u16string(), 0, 0);
  }
}

void ContextualSearchDelegateImpl::StartSearchTermResolutionRequest(
    base::WeakPtr<ContextualSearchContext> context,
    content::WebContents* web_contents,
    SearchTermResolutionCallback callback) {
  DCHECK(web_contents);
  if (!context)
    return;

  DCHECK(context->CanResolve());

  // Immediately cancel any request that's in flight, since we're building a new
  // context (and the response disposes of any existing context).
  url_loader_.reset();

  // Decide if the URL should be sent with the context.
  if (context->CanSendBasePageUrl())
    context->SetBasePageUrl(web_contents->GetLastCommittedURL());

  // Issue the resolve request.
  ResolveSearchTermFromContext(context, std::move(callback));
}

void ContextualSearchDelegateImpl::ResolveSearchTermFromContext(
    base::WeakPtr<ContextualSearchContext> context,
    SearchTermResolutionCallback callback) {
  DCHECK(context);
  GURL request_url(BuildRequestUrl(context.get()));

  SCOPED_CRASH_KEY_STRING1024("contextual_search", "url",
                              request_url.possibly_invalid_spec());
  DCHECK(request_url.is_valid()) << request_url.possibly_invalid_spec();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;

  // Populates the discourse context and adds it to the HTTP header of the
  // search term resolution request.
  resource_request->headers = GetDiscourseContext(*context);

  // Disable cookies for this request. The credentials mode should be omit by
  // default, only change to include for debug purpose.
  resource_request->credentials_mode =
      base::FeatureList::IsEnabled(kContextualSearchWithCredentialsForDebug)
          ? network::mojom::CredentialsMode::kInclude
          : network::mojom::CredentialsMode::kOmit;

  // Semantic details for this "Resolve" request:
  net::NetworkTrafficAnnotationTag traffic_annotation =
      base::FeatureList::IsEnabled(kContextualSearchWithCredentialsForDebug)
          ? net::DefineNetworkTrafficAnnotation(
                "contextual_search_resolve_debug",
                R"(
          semantics {
            sender: "Contextual Search"
            description:
              "Chromium can determine the best search term to apply for any "
               "section of plain text for almost any page.  This sends page "
               "data to Google and the response identifies what to search for "
               "plus additional actionable information."
            trigger:
              "Triggered by an unhandled tap or touch and hold gesture on "
              "plain text on most pages."
            data:
              "The URL and some page content from the current tab."
            destination: GOOGLE_OWNED_SERVICE
            user_data {
              type: SENSITIVE_URL
              type: WEB_CONTENT
            }
            internal {
              contacts {
                email: "gangwu@chromium.org"
              }
              contacts {
                email: "contextual-search-dev@chromium.org"
              }
            }
            last_reviewed: "2024-08-12"
          }
          policy {
            cookies_allowed: YES
            cookies_store: "user"
            setting:
              "This feature can be disabled by turning off 'Touch to Search' "
              "in Chrome for Android settings."
            chrome_policy {
              ContextualSearchEnabled {
                  policy_options {mode: MANDATORY}
                  ContextualSearchEnabled: false
              }
            }
          })")
          : net::DefineNetworkTrafficAnnotation("contextual_search_resolve",
                                                R"(
          semantics {
            sender: "Contextual Search"
            description:
              "Chromium can determine the best search term to apply for any "
               "section of plain text for almost any page.  This sends page "
               "data to Google and the response identifies what to search for "
               "plus additional actionable information."
            trigger:
              "Triggered by an unhandled tap or touch and hold gesture on "
              "plain text on most pages."
            data:
              "The URL and some page content from the current tab."
            destination: GOOGLE_OWNED_SERVICE
            user_data {
              type: SENSITIVE_URL
              type: WEB_CONTENT
            }
            internal {
              contacts {
                email: "gangwu@chromium.org"
              }
              contacts {
                email: "contextual-search-dev@chromium.org"
              }
            }
            last_reviewed: "2024-08-12"
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature can be disabled by turning off 'Touch to Search' "
              "in Chrome for Android settings."
            chrome_policy {
              ContextualSearchEnabled {
                  policy_options {mode: MANDATORY}
                  ContextualSearchEnabled: false
              }
            }
          })");

  // Add Chrome experiment state to the request headers.
  // Reset will delete any previous loader, and we won't get any callback.
  url_loader_ =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request),
          variations::InIncognito::kNo,  // Impossible to be incognito at this
                                         // point.
          traffic_annotation);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ContextualSearchDelegateImpl::OnUrlLoadComplete,
                     base::Unretained(this), context, std::move(callback)));
}

void ContextualSearchDelegateImpl::OnUrlLoadComplete(
    base::WeakPtr<ContextualSearchContext> context,
    SearchTermResolutionCallback callback,
    std::unique_ptr<std::string> response_body) {
  if (!context)
    return;

  // Network error codes are negative. See: src/net/base/net_error_list.h.
  base::UmaHistogramSparse("Search.ContextualSearch.NetError",
                           std::abs(url_loader_->NetError()));

  int response_code = ResolvedSearchTerm::kResponseCodeUninitialized;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  std::unique_ptr<ResolvedSearchTerm> resolved_search_term =
      std::make_unique<ResolvedSearchTerm>(response_code);
  if (response_body && response_code == net::HTTP_OK) {
    resolved_search_term =
        GetResolvedSearchTermFromJson(*context, response_code, *response_body);
  }
  callback.Run(*resolved_search_term);
}

std::unique_ptr<ResolvedSearchTerm>
ContextualSearchDelegateImpl::GetResolvedSearchTermFromJson(
    const ContextualSearchContext& context,
    int response_code,
    const std::string& json_string) {
  std::string search_term;
  std::string display_text;
  std::string alternate_term;
  std::string mid;
  std::string prevent_preload;
  int mention_start = 0;
  int mention_end = 0;
  int start_adjust = 0;
  int end_adjust = 0;
  std::string context_language;
  std::string thumbnail_url;
  std::string caption;
  std::string quick_action_uri;
  QuickActionCategory quick_action_category = QUICK_ACTION_CATEGORY_NONE;
  std::string search_url_full;
  std::string search_url_preload;
  int coca_card_tag = 0;
  std::string related_searches_json;

  DecodeSearchTermFromJsonResponse(
      json_string, &search_term, &display_text, &alternate_term, &mid,
      &prevent_preload, &mention_start, &mention_end, &context_language,
      &thumbnail_url, &caption, &quick_action_uri, &quick_action_category,
      &search_url_full, &search_url_preload, &coca_card_tag,
      &related_searches_json);
  if (mention_start != 0 || mention_end != 0) {
    // Sanity check that our selection is non-zero and it is less than
    // 100 characters as that would make contextual search bar hide.
    // We also check that there is at least one character overlap between
    // the new and old selection.
    if (mention_start >= mention_end ||
        (mention_end - mention_start) > kContextualSearchMaxSelection ||
        mention_end <= context.GetStartOffset() ||
        mention_start >= context.GetEndOffset()) {
      start_adjust = 0;
      end_adjust = 0;
    } else {
      start_adjust = mention_start - context.GetStartOffset();
      end_adjust = mention_end - context.GetEndOffset();
    }
  }
  bool is_invalid =
      response_code == ResolvedSearchTerm::kResponseCodeUninitialized;
  return std::make_unique<ResolvedSearchTerm>(
      is_invalid, response_code, search_term, display_text, alternate_term, mid,
      prevent_preload == kDoPreventPreloadValue, start_adjust, end_adjust,
      context_language, thumbnail_url, caption, quick_action_uri,
      quick_action_category, search_url_full, search_url_preload, coca_card_tag,
      related_searches_json);
}

std::string ContextualSearchDelegateImpl::BuildRequestUrl(
    ContextualSearchContext* context) {
  if (!template_url_service_ ||
      !template_url_service_->GetDefaultSearchProvider()) {
    return std::string();
  }

  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();

  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  // Set the Coca-integration version.
  // This is based on our current active feature.
  int contextual_cards_version =
      contextual_search::kContextualCardsTranslationsIntegration;
  // Mixin the debug setting if a commandline switch has been set.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kContextualSearchDebugCommandlineSwitch)) {
    contextual_cards_version +=
        contextual_search::kContextualCardsServerDebugMixin;
  }
  // Let the field-trial override.
  if (field_trial_->GetContextualCardsVersion() != 0) {
    contextual_cards_version = field_trial_->GetContextualCardsVersion();
  }

  int mainFunctionVersion;
  switch (context->GetRequestType()) {
    case ContextualSearchContext::RequestType::CONTEXTUAL_SEARCH:
      mainFunctionVersion = kContextualSearchRequestCtxsVersion;
      break;
    case ContextualSearchContext::RequestType::RELATED_SEARCHES:
      mainFunctionVersion = kRelatedSearchesCtxsVersion;
      break;
    case ContextualSearchContext::RequestType::PARTIAL_TRANSLATE:
      mainFunctionVersion = kDesktopPartialTranslateCtxsVersion;
      break;
  }

  TemplateURLRef::SearchTermsArgs::ContextualSearchParams params(
      mainFunctionVersion, contextual_cards_version, context->GetHomeCountry(),
      context->GetPreviousEventId(), context->GetPreviousEventResults(),
      context->GetExactResolve(),
      context->GetTranslationLanguages().detected_language,
      context->GetTranslationLanguages().target_language,
      context->GetTranslationLanguages().fluent_languages,
      context->GetRelatedSearchesStamp(), context->GetApplyLangHint());

  search_terms_args.contextual_search_params = params;

  std::string request(
      template_url->contextual_search_url_ref().ReplaceSearchTerms(
          search_terms_args, template_url_service_->search_terms_data(), NULL));

  // The switch/param should be the URL up to and including the endpoint.
  std::string replacement_url = field_trial_->GetResolverURLPrefix();

  // If a replacement URL was specified above, do the substitution.
  if (!replacement_url.empty()) {
    size_t pos = request.find(kContextualSearchServerEndpoint);
    if (pos != std::string::npos) {
      request.replace(0, pos + strlen(kContextualSearchServerEndpoint),
                      replacement_url);
    }
  }
  return request;
}

void ContextualSearchDelegateImpl::OnTextSurroundingSelectionAvailable(
    base::WeakPtr<ContextualSearchContext> context,
    SurroundingTextCallback callback,
    const std::u16string& surrounding_text,
    uint32_t start_offset,
    uint32_t end_offset) {
  if (!context)
    return;

  // Sometimes the surroundings are 0, 0, '', so run the callback with empty
  // data in that case. See https://crbug.com/393100.
  if (start_offset == 0 && end_offset == 0 && surrounding_text.length() == 0) {
    callback.Run(std::string(), std::u16string(), 0, 0);
    return;
  }

  // Pin the start and end offsets to ensure they point within the string.
  uint32_t surrounding_length = surrounding_text.length();
  start_offset = std::min(surrounding_length, start_offset);
  end_offset = std::min(surrounding_length, end_offset);
  if (end_offset < start_offset) {
    return;
  }

  context->SetSelectionSurroundings(start_offset, end_offset, surrounding_text);

  // Call the Java surrounding callback with a shortened copy of the
  // surroundings to use as a sample of the surrounding text.
  int sample_surrounding_size = field_trial_->GetSampleSurroundingSize();
  DCHECK(sample_surrounding_size >= 0);
  size_t selection_start = start_offset;
  size_t selection_end = end_offset;
  int sample_padding_each_side = sample_surrounding_size / 2;
  std::u16string sample_surrounding_text =
      SampleSurroundingText(surrounding_text, sample_padding_each_side,
                            &selection_start, &selection_end);
  DCHECK(selection_start <= selection_end);
  callback.Run(context->GetBasePageEncoding(), sample_surrounding_text,
               selection_start, selection_end);
}

// Decodes the given response from the search term resolution request and sets
// the value of the given parameters.
void ContextualSearchDelegateImpl::DecodeSearchTermFromJsonResponse(
    const std::string& response,
    std::string* search_term,
    std::string* display_text,
    std::string* alternate_term,
    std::string* mid,
    std::string* prevent_preload,
    int* mention_start,
    int* mention_end,
    std::string* lang,
    std::string* thumbnail_url,
    std::string* caption,
    std::string* quick_action_uri,
    QuickActionCategory* quick_action_category,
    std::string* search_url_full,
    std::string* search_url_preload,
    int* coca_card_tag,
    std::string* related_searches_json) {
  bool contains_xssi_escape =
      base::StartsWith(response, kXssiEscape, base::CompareCase::SENSITIVE);
  const std::string& proper_json =
      contains_xssi_escape ? response.substr(sizeof(kXssiEscape) - 1)
                           : response;
  std::optional<base::Value> root = base::JSONReader::Read(proper_json);
  if (!root) {
    return;
  }

  const base::Value::Dict* dict = root->GetIfDict();
  if (!dict) {
    return;
  }

  auto extract_string = [&dict](std::string_view key, std::string* out) {
    const std::string* string_pointer = dict->FindString(key);
    if (string_pointer)
      *out = *string_pointer;
  };

  extract_string(kContextualSearchPreventPreload, prevent_preload);
  extract_string(kContextualSearchResponseSearchTermParam, search_term);
  extract_string(kContextualSearchResponseLanguageParam, lang);

  // For the display_text, if not present fall back to the "search_term".
  if (const std::string* display_text_pointer =
          dict->FindString(kContextualSearchResponseDisplayTextParam);
      display_text_pointer) {
    *display_text = *display_text_pointer;
  } else {
    *display_text = *search_term;
  }
  extract_string(kContextualSearchResponseMidParam, mid);

  // Extract mentions for selection expansion.
  if (!field_trial_->IsDecodeMentionsDisabled()) {
    const base::Value::List* mentions_list =
        dict->FindList(kContextualSearchMentionsKey);
    // Note that because we've deserialized the json and it's not used later, we
    // can just take the list without worrying about putting it back.
    if (mentions_list && mentions_list->size() >= 2u)
      ExtractMentionsStartEnd(*mentions_list, mention_start, mention_end);
  }

  // If either the selected text or the resolved term is not the search term,
  // use it as the alternate term.
  std::string selected_text;
  extract_string(kContextualSearchResponseSelectedTextParam, &selected_text);
  if (selected_text != *search_term) {
    *alternate_term = selected_text;
  } else {
    std::string resolved_term;
    extract_string(kContextualSearchResponseResolvedTermParam, &resolved_term);
    if (resolved_term != *search_term) {
      *alternate_term = resolved_term;
    }
  }

  // Contextual Cards V1+ Integration.
  // Get the basic Bar data for Contextual Cards integration directly
  // from the root.
  extract_string(kContextualSearchCaption, caption);
  extract_string(kContextualSearchThumbnail, thumbnail_url);

  // Contextual Cards V2+ Integration.
  // Get the Single Action data.
  extract_string(kContextualSearchAction, quick_action_uri);
  std::string quick_action_category_string;
  extract_string(kContextualSearchCategory, &quick_action_category_string);
  if (!quick_action_category_string.empty()) {
    if (quick_action_category_string == kActionCategoryAddress) {
      *quick_action_category = QUICK_ACTION_CATEGORY_ADDRESS;
    } else if (quick_action_category_string == kActionCategoryEmail) {
      *quick_action_category = QUICK_ACTION_CATEGORY_EMAIL;
    } else if (quick_action_category_string == kActionCategoryEvent) {
      *quick_action_category = QUICK_ACTION_CATEGORY_EVENT;
    } else if (quick_action_category_string == kActionCategoryPhone) {
      *quick_action_category = QUICK_ACTION_CATEGORY_PHONE;
    } else if (quick_action_category_string == kActionCategoryWebsite) {
      *quick_action_category = QUICK_ACTION_CATEGORY_WEBSITE;
    }
  }

  // Contextual Cards V4+ may also provide full search URLs to use in the
  // overlay.
  extract_string(kContextualSearchSearchUrlFull, search_url_full);
  extract_string(kContextualSearchSearchUrlPreload, search_url_preload);

  // Contextual Cards V5+ integration can provide the primary card tag, so
  // clients can tell what kind of card they have received.
  // TODO(donnd): make sure this works with a non-integer or missing value!
  std::optional<int> maybe_coca_card_tag =
      dict->FindInt(kContextualSearchCardTag);
  if (coca_card_tag && maybe_coca_card_tag)
    *coca_card_tag = *maybe_coca_card_tag;

  // Any Contextual Cards integration.
  // For testing purposes check if there was a diagnostic from Contextual
  // Cards and output that into the log.
  // TODO(donnd): remove after full Contextual Cards integration.
  std::string contextual_cards_diagnostic;
  extract_string("diagnostic", &contextual_cards_diagnostic);
  if (contextual_cards_diagnostic.empty()) {
    DVLOG(0) << "No diagnostic data in the response.";
  } else {
    DVLOG(0) << "The Contextual Cards backend response: ";
    DVLOG(0) << contextual_cards_diagnostic;
  }

  // Extract an arbitrary Related Searches payload as JSON and return to Java
  // for decoding.
  // TODO(donnd): remove soon (once the server is updated);
  if (const base::Value* rsearches_json_value =
          dict->Find(kRelatedSearchesSuggestions))
    base::JSONWriter::Write(*rsearches_json_value, related_searches_json);
}

// Extract the Start/End of the mentions in the surrounding text
// for selection-expansion.
void ContextualSearchDelegateImpl::ExtractMentionsStartEnd(
    const base::Value::List& mentions_list,
    int* start_result,
    int* end_result) const {
  if (mentions_list.size() >= 1 && mentions_list[0].is_int())
    *start_result = std::max(0, mentions_list[0].GetInt());
  if (mentions_list.size() >= 2 && mentions_list[1].is_int())
    *end_result = std::max(0, mentions_list[1].GetInt());
}

std::u16string ContextualSearchDelegateImpl::SampleSurroundingText(
    const std::u16string& surrounding_text,
    int padding_each_side,
    size_t* start,
    size_t* end) const {
  std::u16string result_text = surrounding_text;
  size_t start_offset = *start;
  size_t end_offset = *end;
  size_t padding_each_side_pinned =
      padding_each_side >= 0 ? padding_each_side : 0;
  // Now trim the context so the portions before or after the selection
  // are within the given limit.
  if (start_offset > padding_each_side_pinned) {
    // Trim the start.
    int trim = start_offset - padding_each_side_pinned;
    result_text = result_text.substr(trim);
    start_offset -= trim;
    end_offset -= trim;
  }
  if (result_text.length() > end_offset + padding_each_side_pinned) {
    // Trim the end.
    result_text = result_text.substr(0, end_offset + padding_each_side_pinned);
  }
  *start = start_offset;
  *end = end_offset;
  return result_text;
}
