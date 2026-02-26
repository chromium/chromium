// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/android/template_url_service_android.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/search_engines/android/template_url_android.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "components/search_provider_logos/switches.h"
#include "net/base/url_util.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/search_engines/android/jni_headers/TemplateUrlService_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {
TemplateURLData CreatePlayAPITemplateURLData(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jname,
    const base::android::JavaRef<jstring>& jkeyword,
    const base::android::JavaRef<jstring>& jsearch_url,
    const base::android::JavaRef<jstring>& jsuggest_url,
    const base::android::JavaRef<jstring>& jfavicon_url,
    const base::android::JavaRef<jstring>& jnew_tab_url,
    const base::android::JavaRef<jstring>& jimage_url,
    const base::android::JavaRef<jstring>& jimage_url_post_params,
    const base::android::JavaRef<jstring>& jimage_translate_url,
    const base::android::JavaRef<jstring>&
        jimage_translate_source_language_param_key,
    const base::android::JavaRef<jstring>&
        jimage_translate_target_language_param_key) {
  std::u16string keyword =
      base::android::ConvertJavaStringToUTF16(env, jkeyword);
  std::u16string name = base::android::ConvertJavaStringToUTF16(env, jname);
  std::string search_url = base::android::ConvertJavaStringToUTF8(jsearch_url);
  std::string suggest_url;
  if (jsuggest_url) {
    suggest_url = base::android::ConvertJavaStringToUTF8(jsuggest_url);
  }
  std::string favicon_url;
  if (jfavicon_url) {
    favicon_url = base::android::ConvertJavaStringToUTF8(jfavicon_url);
  }
  std::string new_tab_url;
  if (jnew_tab_url) {
    new_tab_url = base::android::ConvertJavaStringToUTF8(jnew_tab_url);
  }
  std::string image_url;
  if (jimage_url) {
    image_url = base::android::ConvertJavaStringToUTF8(jimage_url);
  }
  std::string image_url_post_params;
  if (jimage_url_post_params) {
    image_url_post_params =
        base::android::ConvertJavaStringToUTF8(jimage_url_post_params);
  }
  std::string image_translate_url;
  if (jimage_translate_url) {
    image_translate_url =
        base::android::ConvertJavaStringToUTF8(jimage_translate_url);
  }
  std::string image_translate_source_language_param_key;
  if (jimage_translate_source_language_param_key) {
    image_translate_source_language_param_key =
        base::android::ConvertJavaStringToUTF8(
            jimage_translate_source_language_param_key);
  }
  std::string image_translate_target_language_param_key;
  if (jimage_translate_target_language_param_key) {
    image_translate_target_language_param_key =
        base::android::ConvertJavaStringToUTF8(
            jimage_translate_target_language_param_key);
  }

  return TemplateURLService::CreatePlayAPITemplateURLData(
      keyword, name, search_url, suggest_url, favicon_url, new_tab_url,
      image_url, image_url_post_params, image_translate_url,
      image_translate_source_language_param_key,
      image_translate_target_language_param_key

  );
}
}  // namespace

TemplateUrlServiceAndroid::TemplateUrlServiceAndroid(
    TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {
  template_url_subscription_ = template_url_service_->RegisterOnLoadedCallback(
      base::BindOnce(&TemplateUrlServiceAndroid::OnTemplateURLServiceLoaded,
                     base::Unretained(this)));
  template_url_service_->AddObserver(this);
}

TemplateUrlServiceAndroid::~TemplateUrlServiceAndroid() {
  if (java_ref_) {
    Java_TemplateUrlService_clearNativePtr(jni_zero::AttachCurrentThread(),
                                           java_ref_);
    java_ref_.Reset();
  }
  template_url_service_->RemoveObserver(this);
}

ScopedJavaLocalRef<jobject> TemplateUrlServiceAndroid::GetJavaObject() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (!java_ref_) {
    java_ref_.Reset(
        Java_TemplateUrlService_create(env, reinterpret_cast<intptr_t>(this)));
  }
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

void TemplateUrlServiceAndroid::Load(JNIEnv* env) {
  template_url_service_->Load();
}

void TemplateUrlServiceAndroid::SetUserSelectedDefaultSearchProvider(
    JNIEnv* env,
    const JavaRef<jstring>& jkeyword,
    int32_t choice_made_location) {
  std::u16string keyword(
      base::android::ConvertJavaStringToUTF16(env, jkeyword));
  TemplateURL* template_url =
      template_url_service_->GetTemplateURLForKeyword(keyword);
  template_url_service_->SetUserSelectedDefaultSearchProvider(
      template_url,
      static_cast<search_engines::ChoiceMadeLocation>(choice_made_location));
}

bool TemplateUrlServiceAndroid::IsLoaded(JNIEnv* env) const {
  return template_url_service_->loaded();
}

bool TemplateUrlServiceAndroid::IsDefaultSearchManaged(JNIEnv* env) {
  return template_url_service_->is_default_search_managed();
}

bool TemplateUrlServiceAndroid::IsSearchByImageAvailable(JNIEnv* env) {
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();
  return default_search_provider &&
         !default_search_provider->image_url().empty() &&
         default_search_provider->image_url_ref().IsValid(
             template_url_service_->search_terms_data());
}

bool TemplateUrlServiceAndroid::DoesDefaultSearchEngineHaveLogo(JNIEnv* env) {
  // |kSearchProviderLogoURL| applies to all search engines (Google or
  // third-party).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          search_provider_logos::switches::kSearchProviderLogoURL)) {
    return true;
  }

  // Google always has a logo.
  if (IsDefaultSearchEngineGoogle(env)) {
    return true;
  }

  // Third-party search engines can have a doodle specified via the command
  // line, or a static logo or doodle from the TemplateURLService.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          search_provider_logos::switches::kThirdPartyDoodleURL)) {
    return true;
  }
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();
  return default_search_provider &&
         (default_search_provider->doodle_url().is_valid() ||
          default_search_provider->logo_url().is_valid());
}

bool TemplateUrlServiceAndroid::IsDefaultSearchEngineGoogle(JNIEnv* env) {
  return IsDefaultSearchEngineGoogle();
}

bool TemplateUrlServiceAndroid::IsSearchResultsPageFromDefaultSearchProvider(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jurl) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, jurl);
  return template_url_service_->IsSearchResultsPageFromDefaultSearchProvider(
      url);
}

bool TemplateUrlServiceAndroid::IsDefaultSearchEngineGoogle() {
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();
  return default_search_provider &&
         default_search_provider->url_ref().HasGoogleBaseURLs(
             template_url_service_->search_terms_data());
}

void TemplateUrlServiceAndroid::OnTemplateURLServiceLoaded() {
  template_url_subscription_ = {};
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (!java_ref_)
    return;
  Java_TemplateUrlService_templateUrlServiceLoaded(env, java_ref_);
}

void TemplateUrlServiceAndroid::OnTemplateURLServiceChanged() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (!java_ref_)
    return;
  Java_TemplateUrlService_onTemplateURLServiceChanged(env, java_ref_);
}

base::android::ScopedJavaLocalRef<jstring>
TemplateUrlServiceAndroid::GetUrlForSearchQuery(
    JNIEnv* env,
    const JavaRef<jstring>& jquery,
    const JavaRef<jobjectArray>& jsearch_params) {
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();

  std::u16string query(base::android::ConvertJavaStringToUTF16(env, jquery));

  std::string url;
  if (default_provider &&
      default_provider->url_ref().SupportsReplacement(
          template_url_service_->search_terms_data()) &&
      !query.empty()) {
    std::string additional_params;
    if (jsearch_params) {
      std::vector<std::string> params;
      base::android::AppendJavaStringArrayToStringVector(env, jsearch_params,
                                                         &params);
      additional_params = base::JoinString(params, "&");
    }
    TemplateURLRef::SearchTermsArgs args(query);
    args.additional_query_params = std::move(additional_params);
    url = default_provider->url_ref().ReplaceSearchTerms(
        args, template_url_service_->search_terms_data());
  }

  return base::android::ConvertUTF8ToJavaString(env, url);
}

base::android::ScopedJavaLocalRef<jstring>
TemplateUrlServiceAndroid::GetSearchQueryForUrl(JNIEnv* env,
                                                const JavaRef<jobject>& jurl) {
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();

  GURL url = url::GURLAndroid::ToNativeGURL(env, jurl);

  std::u16string query;

  if (default_provider &&
      default_provider->url_ref().SupportsReplacement(
          template_url_service_->search_terms_data()) &&
      template_url_service_->IsSearchResultsPageFromDefaultSearchProvider(
          url)) {
    default_provider->ExtractSearchTermsFromURL(
        url, template_url_service_->search_terms_data(), &query);
  }

  return base::android::ConvertUTF16ToJavaString(env, query);
}

base::android::ScopedJavaLocalRef<jobject>
TemplateUrlServiceAndroid::GetUrlForVoiceSearchQuery(
    JNIEnv* env,
    const JavaRef<jstring>& jquery) {
  std::u16string query(base::android::ConvertJavaStringToUTF16(env, jquery));

  if (!query.empty()) {
    GURL gurl(GetDefaultSearchURLForSearchTerms(template_url_service_, query));
    if (IsDefaultSearchEngineGoogle())
      gurl = net::AppendQueryParameter(gurl, "inm", "vs");
    return url::GURLAndroid::FromNativeGURL(env, gurl);
  }

  return url::GURLAndroid::EmptyGURL(env);
}

base::android::ScopedJavaLocalRef<jobject>
TemplateUrlServiceAndroid::GetComposeplateUrl(JNIEnv* env,
                                              const JavaRef<jobject>& obj) {
  if (!IsDefaultSearchEngineGoogle()) {
    return nullptr;
  }

  return url::GURLAndroid::FromNativeGURL(
      env,
      GetUrlForAim(template_url_service_,
                   omnibox::ANDROID_CHROME_NTP_FAKE_OMNIBOX_ENTRY_POINT,
                   /*query_start_time=*/base::Time::Now(),
                   /*query_text=*/std::u16string(),
                   lens::LensOverlayInvocationSource::kOmniboxContextualQuery,
                   /*additional_params=*/{}));
}

base::android::ScopedJavaLocalRef<jobject>
TemplateUrlServiceAndroid::GetUrlForContextualSearchQuery(
    JNIEnv* env,
    const JavaRef<jstring>& jquery,
    const JavaRef<jstring>& jalternate_term,
    bool jshould_prefetch,
    const JavaRef<jstring>& jprotocol_version) {
  std::u16string query(base::android::ConvertJavaStringToUTF16(env, jquery));

  if (!query.empty()) {
    GURL gurl(GetDefaultSearchURLForSearchTerms(template_url_service_, query));
    if (IsDefaultSearchEngineGoogle()) {
      std::string protocol_version(
          base::android::ConvertJavaStringToUTF8(env, jprotocol_version));
      gurl = net::AppendQueryParameter(gurl, "ctxs", protocol_version);
      if (jshould_prefetch) {
        // Indicate that the search page is being prefetched.
        gurl = net::AppendQueryParameter(gurl, "pf", "c");
      }

      if (jalternate_term) {
        std::string alternate_term(
            base::android::ConvertJavaStringToUTF8(env, jalternate_term));
        if (!alternate_term.empty()) {
          gurl = net::AppendQueryParameter(gurl, "ctxsl_alternate_term",
                                           alternate_term);
        }
      }
    }
    return url::GURLAndroid::FromNativeGURL(env, gurl);
  }

  return url::GURLAndroid::EmptyGURL(env);
}

base::android::ScopedJavaLocalRef<jstring>
TemplateUrlServiceAndroid::GetSearchEngineUrlFromTemplateUrl(
    JNIEnv* env,
    const JavaRef<jstring>& jkeyword) {
  std::u16string keyword =
      base::android::ConvertJavaStringToUTF16(env, jkeyword);
  TemplateURL* template_url =
      template_url_service_->GetTemplateURLForKeyword(keyword);
  if (!template_url)
    return base::android::ScopedJavaLocalRef<jstring>::Adopt(env, nullptr);
  std::string url(template_url->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"query"),
      template_url_service_->search_terms_data()));
  return base::android::ConvertUTF8ToJavaString(env, url);
}

int TemplateUrlServiceAndroid::GetSearchEngineTypeFromTemplateUrl(
    JNIEnv* env,
    const JavaRef<jstring>& jkeyword) {
  std::u16string keyword =
      base::android::ConvertJavaStringToUTF16(env, jkeyword);
  TemplateURL* template_url =
      template_url_service_->GetTemplateURLForKeyword(keyword);
  if (!template_url)
    return -1;
  const SearchTermsData& search_terms_data =
      template_url_service_->search_terms_data();
  return template_url->GetEngineType(search_terms_data);
}

std::u16string TemplateUrlServiceAndroid::GetFullNameFromTemplateUrl(
    JNIEnv* env,
    const std::u16string& keyword) {
  TemplateURL* template_url =
      template_url_service_->GetTemplateURLForKeyword(keyword);
  if (!template_url) {
    return u"";
  }
  return template_url->GetFullName();
}

bool TemplateUrlServiceAndroid::SetPlayAPISearchEngine(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jname,
    const base::android::JavaRef<jstring>& jkeyword,
    const base::android::JavaRef<jstring>& jsearch_url,
    const base::android::JavaRef<jstring>& jsuggest_url,
    const base::android::JavaRef<jstring>& jfavicon_url,
    const base::android::JavaRef<jstring>& jnew_tab_url,
    const base::android::JavaRef<jstring>& jimage_url,
    const base::android::JavaRef<jstring>& jimage_url_post_params,
    const base::android::JavaRef<jstring>& jimage_translate_url,
    const base::android::JavaRef<jstring>&
        jimage_translate_source_language_param_key,
    const base::android::JavaRef<jstring>&
        jimage_translate_target_language_param_key) {
  // The function is scheduled to run only when the service is loaded, see
  // `TemplateUrlService#runWhenLoaded()`.
  CHECK(template_url_service_->loaded());

  // Check if there is already a search engine created by a regulatory program.
  TemplateURLService::TemplateURLVector template_urls =
      template_url_service_->GetTemplateURLs();
  TemplateURL* regulatory_api_turl = nullptr;
  auto found = std::ranges::find_if(template_urls,
                                    &TemplateURL::CreatedByRegulatoryProgram);

  if (found != template_urls.cend()) {
    // Migrate old Play API database entries that were incorrectly marked as
    // safe_for_autoreplace() before M89.
    regulatory_api_turl = *found;
    if (regulatory_api_turl->safe_for_autoreplace()) {
      template_url_service_->ResetTemplateURL(
          regulatory_api_turl, regulatory_api_turl->short_name(),
          regulatory_api_turl->keyword(), regulatory_api_turl->url());
    }
  }

  TemplateURLData new_play_api_turl_data = CreatePlayAPITemplateURLData(
      env, jname, jkeyword, jsearch_url, jsuggest_url, jfavicon_url,
      jnew_tab_url, jimage_url, jimage_url_post_params, jimage_translate_url,
      jimage_translate_source_language_param_key,
      jimage_translate_target_language_param_key);

  return template_url_service_->ResetPlayAPISearchEngine(
      new_play_api_turl_data);
}

bool TemplateUrlServiceAndroid::RemoveSearchEngine(
    JNIEnv* env,
    const std::u16string& keyword) {
  TemplateURL* template_url =
      template_url_service_->GetTemplateURLForKeyword(keyword);
  if (!template_url) {
    return false;
  }

  if (template_url_service_->GetDefaultSearchProvider() == template_url) {
    return false;
  }

  template_url_service_->Remove(template_url);
  return true;
}

bool TemplateUrlServiceAndroid::EditSearchEngine(
    JNIEnv* env,
    const std::u16string& keyword,
    const std::u16string& short_name,
    const std::u16string& new_keyword,
    const std::string& search_url) {
  TemplateURL* template_url =
      template_url_service_->GetTemplateURLForKeyword(keyword);
  if (!template_url) {
    return false;
  }

  // If the template url is prepopulated, we are only allowed to edit the
  // short name and keyword.
  if (template_url->prepopulate_id() != 0) {
    if (search_url != template_url->url()) {
      return false;
    }
  }

  template_url_service_->ResetTemplateURL(template_url, short_name, new_keyword,
                                          search_url);
  return true;
}

bool TemplateUrlServiceAndroid::AddSearchEngine(
    JNIEnv* env,
    const std::u16string& short_name,
    const std::u16string& keyword,
    const std::string& search_url) {
  if (template_url_service_->GetTemplateURLForKeyword(keyword)) {
    return false;
  }

  TemplateURLData data;
  data.SetShortName(short_name);
  data.SetKeyword(keyword);
  data.SetURL(search_url);
  data.safe_for_autoreplace = false;
  if (!template_url_service_->Add(std::make_unique<TemplateURL>(data))) {
    return false;
  }
  return true;
}

base::android::ScopedJavaLocalRef<jstring>
TemplateUrlServiceAndroid::AddSearchEngineForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jkeyword,
    int32_t age_in_days) {
  TemplateURLData data;
  std::u16string keyword =
      base::android::ConvertJavaStringToUTF16(env, jkeyword);
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  data.SetURL("https://testurl.com/?searchstuff={searchTerms}");
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = true;
  data.input_encodings.push_back("UTF-8");
  data.prepopulate_id = 0;
  data.date_created =
      base::Time::Now() - base::Days(static_cast<int>(age_in_days));
  data.last_modified =
      base::Time::Now() - base::Days(static_cast<int>(age_in_days));
  data.last_visited =
      base::Time::Now() - base::Days(static_cast<int>(age_in_days));
  TemplateURL* t_url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  CHECK(t_url) << "Failed adding template url for: " << keyword;
  return base::android::ConvertUTF16ToJavaString(env, t_url->data().keyword());
}

std::vector<raw_ptr<TemplateURL>>
TemplateUrlServiceAndroid::FilterUserSelectableTemplateUrls(
    std::vector<raw_ptr<TemplateURL, VectorExperimental>> template_urls) {
  std::vector<raw_ptr<TemplateURL>> result;

  // Clean up duplication between a Play API template URL and a corresponding
  // prepopulated template URL.
  auto regulatory_api_it = std::ranges::find_if(
      template_urls, &TemplateURL::CreatedByRegulatoryProgram);
  TemplateURL* regulatory_api_turl =
      regulatory_api_it != template_urls.end() ? *regulatory_api_it : nullptr;

  for (TemplateURL* template_url : template_urls) {
    // When Play API template URL supercedes the current template URL, skip it.
    if (regulatory_api_turl &&
        regulatory_api_turl->keyword() == template_url->keyword() &&
        regulatory_api_turl->IsBetterThanConflictingEngine(template_url)) {
      continue;
    }

    // Do not include starter pack engines (@aimode, @tabs, ...) as these are
    // not actual search engines.
    if (template_url->starter_pack_id() !=
        template_url_starter_pack_data::StarterPackId::kNone) {
      continue;
    }

    result.push_back(template_url);
  }

  return result;
}

void TemplateUrlServiceAndroid::GetTemplateUrls(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& template_url_list_obj) {
  auto template_urls = FilterUserSelectableTemplateUrls(
      template_url_service_->GetTemplateURLs());

  for (TemplateURL* template_url : template_urls) {
    Java_TemplateUrlService_addTemplateUrlToList(
        env, template_url_list_obj,
        CreateTemplateUrlAndroid(env, template_url));
  }
}

std::vector<const TemplateURL*>
TemplateUrlServiceAndroid::FilterTemplateUrlsByCategory(
    const std::vector<raw_ptr<TemplateURL, VectorExperimental>>& template_urls,
    TemplateUrlServiceAndroid::TemplateUrlCategory category) {
  std::vector<const TemplateURL*> result;
  for (TemplateURL* turl : template_urls) {
    bool is_default = template_url_service_->ShowInDefaultList(turl);
    bool is_extension = turl->type() == TemplateURL::OMNIBOX_API_EXTENSION;
    bool is_active = template_url_service_->ShowInActivesList(turl);
    bool is_hidden = template_url_service_->HiddenFromLists(turl);

    switch (category) {
      case TemplateUrlCategory::kDefault:
        if (is_default) {
          result.push_back(turl);
        }
        break;
      case TemplateUrlCategory::kActiveSiteSearch:
        if (!is_default && !is_hidden && !is_extension && is_active) {
          result.push_back(turl);
        }
        break;
      case TemplateUrlCategory::kInactiveSiteSearch:
        if (!is_default && !is_hidden && !is_extension && !is_active) {
          result.push_back(turl);
        }
        break;
      case TemplateUrlCategory::kExtension:
        if (!is_default && !is_hidden && is_extension) {
          result.push_back(turl);
        }
        break;
      default:
        NOTREACHED();
    }
  }
  return result;
}

std::vector<const TemplateURL*>
TemplateUrlServiceAndroid::GetTemplateUrlsByCategory(
    JNIEnv* env,
    TemplateUrlCategory category) {
  return FilterTemplateUrlsByCategory(template_url_service_->GetTemplateURLs(),
                                      category);
}

base::android::ScopedJavaLocalRef<jobject>
TemplateUrlServiceAndroid::GetDefaultSearchEngine(JNIEnv* env) {
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (default_search_provider == nullptr) {
    return base::android::ScopedJavaLocalRef<jobject>::Adopt(env, nullptr);
  }
  return CreateTemplateUrlAndroid(env, default_search_provider);
}

base::android::ScopedJavaLocalRef<jobjectArray>
TemplateUrlServiceAndroid::GetImageUrlAndPostContent(JNIEnv* env) {
  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();

  TemplateURLRef::PostContent post_content;
  GURL result(template_url->image_url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u""),
      template_url_service_->search_terms_data(), &post_content));

  std::vector<std::string> output;
  output.push_back(result.spec());
  output.push_back(post_content.first);
  return base::android::ToJavaArrayOfStrings(env, output);
}

DEFINE_JNI(TemplateUrlService)
