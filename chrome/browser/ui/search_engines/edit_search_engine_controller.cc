// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

using base::UserMetricsAction;

EditSearchEngineController::EditSearchEngineController(
    TemplateURL* template_url,
    EditSearchEngineControllerDelegate* edit_keyword_delegate,
    Profile* profile)
    : template_url_(template_url),
      edit_keyword_delegate_(edit_keyword_delegate),
      profile_(profile) {
  DCHECK(profile_);
}

bool EditSearchEngineController::IsTitleValid(
    const std::u16string& title_input) const {
  return !base::CollapseWhitespace(title_input, true).empty();
}

bool EditSearchEngineController::IsURLValid(
    const std::string& url_input) const {
  std::string url = GetFixedUpURL(url_input);
  if (url.empty())
    return false;

  // Convert |url| to a TemplateURLRef so we can check its validity even if it
  // contains replacement strings.  We do this by constructing a dummy
  // TemplateURL owner because |template_url_| might be nullptr and we can't
  // call TemplateURLRef::IsValid() when its owner is nullptr.
  TemplateURLData data;
  data.SetURL(url);
  TemplateURL t_url(data);
  const TemplateURLRef& template_ref = t_url.url_ref();
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_ref.IsValid(service->search_terms_data()))
    return false;

  // If this is going to be the default search engine, it must support
  // replacement.
  if (!template_ref.SupportsReplacement(service->search_terms_data()) &&
      template_url_ &&
      template_url_ == service->GetDefaultSearchProvider())
    return false;

  // Replace any search term with a placeholder string and make sure the
  // resulting URL is valid.
  return GURL(template_ref.ReplaceSearchTerms(
                  TemplateURLRef::SearchTermsArgs(u"x"),
                  service->search_terms_data()))
      .is_valid();
}

bool EditSearchEngineController::IsKeywordValid(
    const std::u16string& keyword_input) const {
  std::u16string keyword_input_trimmed(
      base::CollapseWhitespace(keyword_input, true));
  if (keyword_input_trimmed.empty())
    return false;  // Do not allow empty keyword.

  // The omnibox doesn't properly handle search keywords with whitespace,
  // so do not allow such keywords.
  if (keyword_input_trimmed.find_first_of(base::kWhitespaceUTF16) !=
      std::u16string::npos)
    return false;

  const TemplateURL* turl_with_keyword =
      TemplateURLServiceFactory::GetForProfile(profile_)->
      GetTemplateURLForKeyword(keyword_input_trimmed);
  return (!turl_with_keyword || turl_with_keyword == template_url_);
}

void EditSearchEngineController::AcceptAddOrEdit(
    const std::u16string& title_input,
    const std::u16string& keyword_input,
    const std::string& url_input) {
  DCHECK(!keyword_input.empty());
  std::string url_string = GetFixedUpURL(url_input);
  DCHECK(!url_string.empty());

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* existing =
      template_url_service->GetTemplateURLForKeyword(keyword_input);
  if (existing && (!edit_keyword_delegate_ || existing != template_url_)) {
    // An entry may have been added with the same keyword string while the
    // user edited the dialog, either automatically or by the user (if we're
    // confirming a JS addition, they could have the Options dialog open at the
    // same time). If so, just ignore this add.
    // TODO(pamg): Really, we should modify the entry so this later one
    // overwrites it. But we don't expect this case to be common.
    CleanUpCancelledAdd();
    return;
  }

  if (!edit_keyword_delegate_) {
    // Confiming an entry we got from JS. We have a template_url_, but it
    // hasn't yet been added to the model.
    DCHECK(template_url_);
    template_url_service->AddWithOverrides(
        base::WrapUnique(template_url_.get()), title_input, keyword_input,
        url_string);
    base::RecordAction(UserMetricsAction("KeywordEditor_AddKeywordJS"));
  } else {
    // Adding or modifying an entry via the Delegate.
    edit_keyword_delegate_->OnEditedKeyword(template_url_, title_input,
                                            keyword_input, url_string);
  }
}

void EditSearchEngineController::CleanUpCancelledAdd() {
  if (!edit_keyword_delegate_ && template_url_) {
    // When we have no Delegate, we know that the template_url_ hasn't yet been
    // added to the model, so we need to clean it up.
    delete template_url_;
    template_url_ = nullptr;
  }
}

std::string EditSearchEngineController::GetFixedUpURL(
    const std::string& url_input) const {
  std::u16string url16;
  base::TrimWhitespace(base::UTF8ToUTF16(url_input), base::TRIM_ALL, &url16);
  if (url16.empty())
    return std::string();
  std::string url = TemplateURLRef::DisplayURLToURLRef(url16);

  // Parse the string as a URL to determine the scheme. If we need to, add the
  // scheme. As the scheme may be expanded (as happens with {google:baseURL})
  // we need to replace the search terms before testing for the scheme.
  TemplateURLData data;
  data.SetURL(url);
  TemplateURL t_url(data);
  std::string expanded_url(t_url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"x"),
      TemplateURLServiceFactory::GetForProfile(profile_)->search_terms_data()));
  url::Parsed parts;
  std::string scheme(url_formatter::SegmentURL(expanded_url, &parts));
  if (!parts.scheme.is_valid())
    url.insert(0, scheme + "://");

  return url;
}
