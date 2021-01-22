// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/provider/test_support/jni_headers/AutofillProviderTestHelper_jni.h"

#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/strings/string16.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_provider.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

namespace {
AutofillHandler* GetAutofillHandler(content::WebContents* web_contents,
                                    content::RenderFrameHost* rfh) {
  // Avoid using ContentAutofillDriver::GetForRenderFrameHost(), it will create
  // a new ContentAutofillDriver.
  if (ContentAutofillDriverFactory* factory =
          ContentAutofillDriverFactory::FromWebContents(web_contents)) {
    if (ContentAutofillDriver* driver =
            static_cast<ContentAutofillDriver*>(factory->DriverForKey(rfh))) {
      return driver->autofill_handler();
    }
  }
  return nullptr;
}

AutofillHandler* ToMainFrameAutofillHandler(
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);
  AutofillHandler* autofill_handler =
      GetAutofillHandler(web_contents, web_contents->GetMainFrame());
  CHECK(autofill_handler);
  return autofill_handler;
}

}  // namespace

static void JNI_AutofillProviderTestHelper_DisableDownloadServerForTesting(
    JNIEnv* env_md_ctx_st) {
  AutofillProvider::set_is_download_manager_disabled_for_testing();
}

static jboolean
JNI_AutofillProviderTestHelper_SimulateMainFrameAutofillServerResponseForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobjectArray>& jfield_ids,
    const base::android::JavaParamRef<jintArray>& jfield_types) {
  std::vector<base::string16> field_ids;
  base::android::AppendJavaStringArrayToStringVector(env, jfield_ids,
                                                     &field_ids);
  std::vector<int> field_types;
  base::android::JavaIntArrayToIntVector(env, jfield_types, &field_types);

  AutofillHandler* autofill_handler = ToMainFrameAutofillHandler(jweb_contents);
  const std::map<FormRendererId, std::unique_ptr<FormStructure>>&
      form_structures = autofill_handler->form_structures();
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
      for (auto form_field_data : formData.fields) {
        if (form_field_data.id_attribute == field_ids[i]) {
          autofill::test::AddFieldSuggestionToForm(
              form_field_data,
              static_cast<autofill::ServerFieldType>(field_types[i]),
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
  std::string encoded_response_string;
  base::Base64Encode(response_string, &encoded_response_string);
  autofill_handler->OnLoadedServerPredictionsForTest(encoded_response_string,
                                                     signatures);
  return true;
}

static jboolean
JNI_AutofillProviderTestHelper_SimulateMainFramePredictionsAutofillServerResponseForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobjectArray>& jfield_ids,
    const base::android::JavaParamRef<jobjectArray>& jfield_types) {
  std::vector<base::string16> field_ids;
  base::android::AppendJavaStringArrayToStringVector(env, jfield_ids,
                                                     &field_ids);
  std::vector<std::vector<int>> field_types;
  base::android::JavaArrayOfIntArrayToIntVector(env, jfield_types,
                                                &field_types);

  AutofillHandler* autofill_handler = ToMainFrameAutofillHandler(jweb_contents);
  const std::map<FormRendererId, std::unique_ptr<FormStructure>>&
      form_structures = autofill_handler->form_structures();
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
      for (auto form_field_data : formData.fields) {
        if (form_field_data.id_attribute == field_ids[i]) {
          autofill::test::AddFieldPredictionsToForm(
              form_field_data, field_types[i], form_suggestion);
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
  std::string encoded_response_string;
  base::Base64Encode(response_string, &encoded_response_string);
  autofill_handler->OnLoadedServerPredictionsForTest(encoded_response_string,
                                                     signatures);
  return true;
}

static void
JNI_AutofillProviderTestHelper_SimulateMainFrameAutofillQueryFailedForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  AutofillHandler* autofill_handler = ToMainFrameAutofillHandler(jweb_contents);
  const std::map<FormRendererId, std::unique_ptr<FormStructure>>&
      form_structures = autofill_handler->form_structures();
  // Always use first form.
  CHECK(form_structures.size());
  autofill_handler->OnServerRequestErrorForTest(
      *(autofill::test::GetEncodedSignatures(*(form_structures.begin()->second))
            .begin()),
      AutofillDownloadManager::RequestType::REQUEST_QUERY, 400);
}

}  // namespace autofill
