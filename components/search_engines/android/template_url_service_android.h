// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_SERVICE_ANDROID_H_
#define COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_SERVICE_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

// Android wrapper of the TemplateUrlService which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper.
class TemplateUrlServiceAndroid : public TemplateURLServiceObserver {
 public:
  explicit TemplateUrlServiceAndroid(TemplateURLService* template_url_service);
  ~TemplateUrlServiceAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  void Load(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetUserSelectedDefaultSearchProvider(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jkeyword);
  jboolean IsLoaded(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj) const;
  jboolean IsDefaultSearchManaged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsSearchByImageAvailable(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsDefaultSearchEngineGoogle(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsSearchResultsPageFromDefaultSearchProvider(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl);
  base::android::ScopedJavaLocalRef<jstring> GetUrlForSearchQuery(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jquery,
      const base::android::JavaParamRef<jobjectArray>& jsearch_params);
  base::android::ScopedJavaLocalRef<jstring> GetSearchQueryForUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jurl);
  base::android::ScopedJavaLocalRef<jobject> GetUrlForVoiceSearchQuery(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jquery);
  base::android::ScopedJavaLocalRef<jobject> GetUrlForContextualSearchQuery(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jquery,
      const base::android::JavaParamRef<jstring>& jalternate_term,
      jboolean jshould_prefetch,
      const base::android::JavaParamRef<jstring>& jprotocol_version);
  base::android::ScopedJavaLocalRef<jstring> GetSearchEngineUrlFromTemplateUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jkeyword);
  int GetSearchEngineTypeFromTemplateUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jkeyword);

  // Adds a search engine, set by Play API. Sets it as DSE if possible.
  // Returns true if search engine was successfully added, false if search
  // engine from Play API with such keyword already existed (e.g. from previous
  // attempt to set search engine).
  jboolean SetPlayAPISearchEngine(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jname,
      const base::android::JavaParamRef<jstring>& jkeyword,
      const base::android::JavaParamRef<jstring>& jsearch_url,
      const base::android::JavaParamRef<jstring>& jsuggest_url,
      const base::android::JavaParamRef<jstring>& jfavicon_url,
      jboolean set_as_default);

  // Adds a custom search engine, sets |jkeyword| as its short_name and keyword,
  // and sets its date_created as |age_in_days| days before the current time.
  base::android::ScopedJavaLocalRef<jstring> AddSearchEngineForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jkeyword,
      jint age_in_days);

  // Finds the search engine whose keyword matches |jkeyword| and sets its
  // last_visited time as the current time.
  base::android::ScopedJavaLocalRef<jstring> UpdateLastVisitedForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jkeyword);

  // Get all the available search engines and add them to the
  // |template_url_list_obj| list.
  void GetTemplateUrls(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& template_url_list_obj);

  // Get current default search engine.
  base::android::ScopedJavaLocalRef<jobject> GetDefaultSearchEngine(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  void OnTemplateURLServiceLoaded();

  // TemplateUrlServiceObserver:
  void OnTemplateURLServiceChanged() override;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Pointer to the TemplateUrlService for the main profile.
  TemplateURLService* template_url_service_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_subscription_;

  DISALLOW_COPY_AND_ASSIGN(TemplateUrlServiceAndroid);
};

#endif  // COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_SERVICE_ANDROID_H_
