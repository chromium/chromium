// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <string>

#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/containers/to_vector.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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
    const base::android::JavaRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);
  AutofillManager* autofill_manager =
      GetAutofillManager(web_contents, web_contents->GetPrimaryMainFrame());
  CHECK(autofill_manager);
  return autofill_manager;
}

}  // namespace

static bool
JNI_AutofillProviderTestHelper_SimulateMainFrameAutofillServerResponseForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    const base::android::JavaRef<jobjectArray>& jfield_ids,
    const base::android::JavaRef<jintArray>& jfield_types) {
  std::vector<std::u16string> field_ids;
  base::android::AppendJavaStringArrayToStringVector(env, jfield_ids,
                                                     &field_ids);
  std::vector<int> raw_field_types;
  base::android::JavaIntArrayToIntVector(env, jfield_types, &raw_field_types);

  AutofillManager* autofill_manager = ToMainFrameAutofillManager(jweb_contents);
  std::vector<const FormStructure*> form_structures =
      test_api(*autofill_manager).form_structures();
  CHECK(!form_structures.empty());

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;

  form_suggestion = response.add_form_suggestions();
  size_t found_fields_count = 0;
  std::vector<FormSignature> signatures;
  std::vector<FormData> forms;
  for (const FormStructure* form_structure : form_structures) {
    FormData form_data = form_structure->ToFormData();
    for (size_t i = 0; i < field_ids.size(); ++i) {
      for (auto form_field_data : form_data.fields()) {
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
      signatures = autofill::test::GetEncodedSignatures(*form_structure);
      forms.push_back(std::move(form_data));
      break;
    }
  }
  CHECK(found_fields_count == field_ids.size());

  std::string response_string;
  CHECK(response.SerializeToString(&response_string));
  test_api(*autofill_manager)
      .OnLoadedServerPredictions(base::Base64Encode(response_string),
                                 signatures, forms);
  return true;
}

static bool
JNI_AutofillProviderTestHelper_SimulateMainFramePredictionsAutofillServerResponseForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    const base::android::JavaRef<jobjectArray>& jfield_ids,
    const base::android::JavaRef<jobjectArray>& jfield_types) {
  std::vector<std::u16string> field_ids;
  base::android::AppendJavaStringArrayToStringVector(env, jfield_ids,
                                                     &field_ids);
  std::vector<std::vector<int>> raw_field_types;
  base::android::JavaArrayOfIntArrayToIntVector(env, jfield_types,
                                                &raw_field_types);

  AutofillManager* autofill_manager = ToMainFrameAutofillManager(jweb_contents);
  std::vector<const FormStructure*> form_structures =
      test_api(*autofill_manager).form_structures();
  CHECK(!form_structures.empty());

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;

  form_suggestion = response.add_form_suggestions();
  size_t found_fields_count = 0;
  std::vector<FormSignature> signatures;
  std::vector<FormData> forms;
  for (const FormStructure* form_structure : form_structures) {
    FormData form_data = form_structure->ToFormData();
    for (size_t i = 0; i < field_ids.size(); ++i) {
      for (auto form_field_data : form_data.fields()) {
        if (form_field_data.id_attribute() == field_ids[i]) {
          std::vector<FieldType> field_types = base::ToVector(
              raw_field_types[i],
              [](int type) -> FieldType { return FieldType(type); });
          autofill::test::AddFieldPredictionsToForm(
              form_field_data, field_types, form_suggestion);
          found_fields_count++;
          break;
        }
      }
    }
    if (found_fields_count > 0) {
      signatures = autofill::test::GetEncodedSignatures(*form_structure);
      CHECK(found_fields_count == field_ids.size());
      forms = {std::move(form_data)};
    }
  }

  std::string response_string;
  CHECK(response.SerializeToString(&response_string));
  test_api(*autofill_manager)
      .OnLoadedServerPredictions(base::Base64Encode(response_string),
                                 signatures, forms);
  return true;
}

}  // namespace autofill

DEFINE_JNI(AutofillProviderTestHelper)
