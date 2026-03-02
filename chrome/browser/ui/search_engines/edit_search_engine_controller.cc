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
#include "components/search_engines/util.h"
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
  return IsSearchEngineNameValidToUse(title_input);
}

bool EditSearchEngineController::IsURLValid(
    const std::string& url_input) const {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  return IsSearchEngineURLValidToUse(url_input, service, template_url_);
}

bool EditSearchEngineController::IsKeywordValid(
    const std::u16string& keyword_input) const {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  return IsSearchEngineKeywordValidToUse(keyword_input, service, template_url_);
}

void EditSearchEngineController::AcceptAddOrEdit(
    const std::u16string& title_input,
    const std::u16string& keyword_input,
    const std::string& url_input) {
  DCHECK(!keyword_input.empty());
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  std::string url_string = GetFixedUpSearchEngineUrl(
      url_input, template_url_service->search_terms_data());
  DCHECK(!url_string.empty());

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
