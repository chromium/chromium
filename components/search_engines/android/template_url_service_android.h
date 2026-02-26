// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_SERVICE_ANDROID_H_
#define COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_SERVICE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

// Android wrapper of the TemplateUrlService which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper.
class TemplateUrlServiceAndroid : public TemplateURLServiceObserver {
 public:
  // Defines the category of template URLs to be displayed in different UI
  // sections. The values are shared with
  // org.chromium.components.search_engines.TemplateUrlService.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.search_engines
  enum class TemplateUrlCategory {
    kDefault = 0,
    kActiveSiteSearch = 1,
    kInactiveSiteSearch = 2,
    kExtension = 3,
  };

  explicit TemplateUrlServiceAndroid(TemplateURLService* template_url_service);

  TemplateUrlServiceAndroid(const TemplateUrlServiceAndroid&) = delete;
  TemplateUrlServiceAndroid& operator=(const TemplateUrlServiceAndroid&) =
      delete;

  ~TemplateUrlServiceAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  void Load(JNIEnv* env);
  void SetUserSelectedDefaultSearchProvider(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jkeyword,
      int32_t choice_made_location);
  bool IsLoaded(JNIEnv* env) const;
  bool IsDefaultSearchManaged(JNIEnv* env);
  bool IsSearchByImageAvailable(JNIEnv* env);
  bool DoesDefaultSearchEngineHaveLogo(JNIEnv* env);
  bool IsDefaultSearchEngineGoogle(JNIEnv* env);
  bool IsSearchResultsPageFromDefaultSearchProvider(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jurl);
  base::android::ScopedJavaLocalRef<jstring> GetUrlForSearchQuery(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jquery,
      const base::android::JavaRef<jobjectArray>& jsearch_params);
  base::android::ScopedJavaLocalRef<jstring> GetSearchQueryForUrl(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jurl);
  base::android::ScopedJavaLocalRef<jobject> GetUrlForVoiceSearchQuery(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jquery);
  base::android::ScopedJavaLocalRef<jobject> GetComposeplateUrl(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobject> GetUrlForContextualSearchQuery(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jquery,
      const base::android::JavaRef<jstring>& jalternate_term,
      bool jshould_prefetch,
      const base::android::JavaRef<jstring>& jprotocol_version);
  base::android::ScopedJavaLocalRef<jstring> GetSearchEngineUrlFromTemplateUrl(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jkeyword);
  int GetSearchEngineTypeFromTemplateUrl(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jkeyword);
  std::u16string GetFullNameFromTemplateUrl(JNIEnv* env,
                                            const std::u16string& keyword);

  // Adds a search engine, set by Play API. Sets it as DSE if possible.
  // Returns true if search engine was successfully added, false if search
  // engine from Play API with such keyword already existed (e.g. from previous
  // attempt to set search engine).
  bool SetPlayAPISearchEngine(
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
          jimage_translate_target_language_param_key);

  // Removes the search engine with the given keyword. Returns true if the
  // search engine was successfully removed, false if the search engine was not
  // found or if it is the default search engine.
  bool RemoveSearchEngine(JNIEnv* env, const std::u16string& keyword);

  // Edits the search engine with the given keyword. Returns true if the search
  // engine was successfully edited, false if the search engine was not found or
  // try to edit the url of prepopulated search engines.
  bool EditSearchEngine(JNIEnv* env,
                        const std::u16string& keyword,
                        const std::u16string& short_name,
                        const std::u16string& new_keyword,
                        const std::string& search_url);

  // Adds a search engine with the given attributes. Returns true if the search
  // engine was successfully added, false if the search engine with the given
  // keyword already exists or failed to add internally.
  bool AddSearchEngine(JNIEnv* env,
                       const std::u16string& short_name,
                       const std::u16string& keyword,
                       const std::string& search_url);

  // Adds a custom search engine, sets |jkeyword| as its short_name and keyword,
  // and sets its date_created as |age_in_days| days before the current time.
  base::android::ScopedJavaLocalRef<jstring> AddSearchEngineForTesting(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jkeyword,
      int32_t age_in_days);

  // Finds the search engine whose keyword matches |jkeyword| and sets its
  // last_visited time as the current time.
  base::android::ScopedJavaLocalRef<jstring> UpdateLastVisitedForTesting(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jkeyword);

  // Get all the available search engines and add them to the
  // |template_url_list_obj| list.
  void GetTemplateUrls(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& template_url_list_obj);

  // Get the available search engines filtered by |category|.
  std::vector<const TemplateURL*> GetTemplateUrlsByCategory(
      JNIEnv* env,
      TemplateUrlCategory category);

  // Get current default search engine.
  base::android::ScopedJavaLocalRef<jobject> GetDefaultSearchEngine(
      JNIEnv* env);

  // Get the image search url and the post content.
  base::android::ScopedJavaLocalRef<jobjectArray> GetImageUrlAndPostContent(
      JNIEnv* env);

 private:
  FRIEND_TEST_ALL_PREFIXES(TemplateUrlServiceAndroidUnitTest,
                           FilterUserSelectableTemplateUrls);
  FRIEND_TEST_ALL_PREFIXES(TemplateUrlServiceAndroidUnitTest,
                           FilterTemplateUrlsByCategory);

  bool IsDefaultSearchEngineGoogle();

  void OnTemplateURLServiceLoaded();

  // TemplateUrlServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Given a vector of TemplateURL pointers, returns an array of TemplateURLs
  // that should be selectable by the user as their primary Search Engine.
  static std::vector<raw_ptr<TemplateURL>> FilterUserSelectableTemplateUrls(
      std::vector<raw_ptr<TemplateURL, VectorExperimental>> template_urls);

  std::vector<const TemplateURL*> FilterTemplateUrlsByCategory(
      const std::vector<raw_ptr<TemplateURL, VectorExperimental>>&
          template_urls,
      TemplateUrlCategory category);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Pointer to the TemplateUrlService for the main profile.
  raw_ptr<TemplateURLService> template_url_service_;

  base::CallbackListSubscription template_url_subscription_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_SERVICE_ANDROID_H_
