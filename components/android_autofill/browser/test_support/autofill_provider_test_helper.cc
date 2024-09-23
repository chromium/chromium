// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <iterator>
#include <string>

#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/ranges/algorithm.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/android_autofill/browser/test_support/jni_headers/AutofillProviderTestHelper_jni.h"

namespace autofill {

namespace {
AutofillManager* GetAutofillManager(content::WebContents* web_contents,
                                    content::RenderFrameHost* rfh) {
  // Avoid using ContentAutofillDriver::GetForRenderFrameHost(), it will create
  // a new ContentAutofillDriver.
  if (ContentAutofillDriverFactory* factory =
          ContentAutofillDriverFactory::FromWebContents(web_contents)) {
    if (ContentAutofillDriver* driver = test_api(*factory).GetDriver(rfh)) {
      return &driver->GetAutofillManager();
    }
  }
  return nullptr;
}

AutofillManager* ToMainFrameAutofillManager(
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);
  AutofillManager* autofill_manager =
      GetAutofillManager(web_contents, web_contents->GetPrimaryMainFrame());
  CHECK(autofill_manager);
  return autofill_manager;
}

}  // namespace

static void JNI_AutofillProviderTestHelper_DisableCrowdsourcingForTesting(
    JNIEnv* env_md_ctx_st) {
  AutofillProvider::set_is_crowdsourcing_manager_disabled_for_testing();
}

static jboolean
JNI_AutofillProviderTestHelper_SimulateMainFrameAutofillServerResponseForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobjectArray>& jfield_ids,
    const base::android::JavaParamRef<jintArray>& jfield_types) {
  std::vector<std::u16string> field_ids;
  base::android::AppendJavaStringArrayToStringVector(env, jfield_ids,
                                                     &field_ids);
  std::vector<int> raw_field_types;
  base::android::JavaIntArrayToIntVector(env, jfield_types, &raw_field_types);

  AutofillManager* autofill_manager = ToMainFrameAutofillManager(jweb_contents);
  const std::map<FormGlobalId, std::unique_ptr<FormStructure>>&
      form_structures = autofill_manager->form_structures();
  CHECK(!form_structures.empty());

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;

  form_suggestion = response.add_form_suggestions();
  size_t found_fields_count = 0;
  std::vector<FormSignature> signatures;
  for (auto& j : form_structures) {
    FormData formData = j.second->ToFormData();
    for (size_t i = 0; i < field_ids.size(); ++i) {
      for (auto form_field_data : formData.fields()) {
        if (form_field_data.id_attribute() == field_ids[i]) {
          autofill::test::AddFieldPredictionToForm(
              form_field_data,
              static_cast<autofill::FieldType>(raw_field_types[i]),
              form_suggestion);
          found_fields_count++;
          break;
        }
      }
    }
    if (found_fields_count > 0) {
      signatures = autofill::test::GetEncodedSignatures(*(j.second));
      break;
    }
  }
  CHECK(found_fields_count == field_ids.size());

  std::string response_string;
  CHECK(response.SerializeToString(&response_string));
  test_api(*autofill_manager)
      .OnLoadedServerPredictions(base::Base64Encode(response_string),
                                 signatures);
  return true;
}

static jboolean
JNI_AutofillProviderTestHelper_SimulateMainFramePredictionsAutofillServerResponseForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobjectArray>& jfield_ids,
    const base::android::JavaParamRef<jobjectArray>& jfield_types) {
  std::vector<std::u16string> field_ids;
  base::android::AppendJavaStringArrayToStringVector(env, jfield_ids,
                                                     &field_ids);
  std::vector<std::vector<int>> raw_field_types;
  base::android::JavaArrayOfIntArrayToIntVector(env, jfield_types,
                                                &raw_field_types);

  AutofillManager* autofill_manager = ToMainFrameAutofillManager(jweb_contents);
  const std::map<FormGlobalId, std::unique_ptr<FormStructure>>&
      form_structures = autofill_manager->form_structures();
  CHECK(!form_structures.empty());

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;

  form_suggestion = response.add_form_suggestions();
  size_t found_fields_count = 0;
  std::vector<FormSignature> signatures;
  for (auto& j : form_structures) {
    FormData formData = j.second->ToFormData();
    for (size_t i = 0; i < field_ids.size(); ++i) {
      for (auto form_field_data : formData.fields()) {
        if (form_field_data.id_attribute() == field_ids[i]) {
          std::vector<FieldType> field_types;
          field_types.reserve(raw_field_types[i].size());
          base::ranges::transform(
              raw_field_types[i], std::back_inserter(field_types),
              [](int type) -> FieldType { return FieldType(type); });
          autofill::test::AddFieldPredictionsToForm(
              form_field_data, field_types, form_suggestion);
          found_fields_count++;
          break;
        }
      }
    }
    if (found_fields_count > 0) {
      signatures = autofill::test::GetEncodedSignatures(*(j.second));
      CHECK(found_fields_count == field_ids.size());
    }
  }

  std::string response_string;
  CHECK(response.SerializeToString(&response_string));
  test_api(*autofill_manager)
      .OnLoadedServerPredictions(base::Base64Encode(response_string),
                                 signatures);
  return true;
}

}  // namespace autofill
