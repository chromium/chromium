// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

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
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
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
  return GetAutofillManager(web_contents, web_contents->GetPrimaryMainFrame());
}

std::vector<std::u16string> ToFieldIds(
    JNIEnv* env,
    const base::android::JavaRef<JArray<jstring>>& jfield_ids) {
  std::vector<std::u16string> field_ids;
  for (int32_t i = 0; i < jfield_ids.GetLength(env); ++i) {
    field_ids.push_back(jfield_ids.GetAs<std::u16string>(env, i));
  }
  return field_ids;
}

// Returns the first form cached by `manager` that has a field for each id of
// `field_ids`, or nullptr if there is none. Since form parsing is
// asynchronous, the renderer may have extracted a form that the browser has not
// parsed yet. For frame-transcending forms, the form only contains all fields
// once the forms of all subframes have been parsed.
const FormStructure* FindFormWithFieldIds(
    AutofillManager& manager,
    const std::vector<std::u16string>& field_ids) {
  for (const FormStructure* form_structure :
       test_api(manager).form_structures()) {
    if (std::ranges::all_of(field_ids, [&](const std::u16string& field_id) {
          return std::ranges::any_of(
              form_structure->fields(),
              [&](const std::unique_ptr<AutofillField>& field) {
                return field->id_attribute() == field_id;
              });
        })) {
      return form_structure;
    }
  }
  return nullptr;
}

// Simulates a server response with `field_predictions[i]` for the field with
// id `field_ids[i]`. Returns false and does nothing if the main frame's
// AutofillManager has not parsed a form with all `field_ids` yet. Callers are
// expected to retry until this returns true.
bool SimulateServerResponse(
    const base::android::JavaRef<jobject>& jweb_contents,
    const std::vector<std::u16string>& field_ids,
    const std::vector<std::vector<FieldType>>& field_predictions) {
  CHECK_EQ(field_ids.size(), field_predictions.size());
  AutofillManager* autofill_manager = ToMainFrameAutofillManager(jweb_contents);
  if (!autofill_manager) {
    return false;
  }
  const FormStructure* form_structure =
      FindFormWithFieldIds(*autofill_manager, field_ids);
  if (!form_structure) {
    return false;
  }

  FormData form_data = form_structure->ToFormData();
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion =
      response.add_form_suggestions();
  for (size_t i = 0; i < field_ids.size(); ++i) {
    auto field = std::ranges::find(form_data.fields(), field_ids[i],
                                   &FormFieldData::id_attribute);
    CHECK(field != form_data.fields().end());
    test::AddFieldPredictionsToForm(*field, field_predictions[i],
                                    form_suggestion);
  }

  std::string response_string;
  CHECK(response.SerializeToString(&response_string));
  test_api(*autofill_manager)
      .OnLoadedServerPredictions(base::Base64Encode(response_string),
                                 test::GetEncodedSignatures(*form_structure),
                                 {std::move(form_data)});
  return true;
}

}  // namespace

static bool
JNI_AutofillProviderTestHelper_SimulateMainFrameAutofillServerResponseForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    const base::android::JavaRef<JArray<jstring>>& jfield_ids,
    const base::android::JavaRef<JArray<int32_t>>& jfield_types) {
  std::vector<std::vector<FieldType>> field_predictions =
      base::ToVector(jfield_types.CreateViewCritical(env), [](int32_t type) {
        return std::vector<FieldType>{
            ToSafeFieldType(type).value_or(NO_SERVER_DATA)};
      });
  return SimulateServerResponse(jweb_contents, ToFieldIds(env, jfield_ids),
                                field_predictions);
}

static bool
JNI_AutofillProviderTestHelper_SimulateMainFramePredictionsAutofillServerResponseForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    const base::android::JavaRef<JArray<jstring>>& jfield_ids,
    const base::android::JavaRef<JArray<JArray<int32_t>>>& jfield_types) {
  std::vector<std::vector<FieldType>> field_predictions;
  for (int32_t i = 0; i < jfield_types.GetLength(env); ++i) {
    base::android::ScopedJavaLocalRef<JArray<int32_t>> field_types_jarray =
        jfield_types.Get(env, i);
    field_predictions.push_back(base::ToVector(
        field_types_jarray.CreateViewCritical(env), [](int32_t type) {
          return ToSafeFieldType(type).value_or(NO_SERVER_DATA);
        }));
  }
  return SimulateServerResponse(jweb_contents, ToFieldIds(env, jfield_ids),
                                field_predictions);
}

}  // namespace autofill

DEFINE_JNI(AutofillProviderTestHelper)
