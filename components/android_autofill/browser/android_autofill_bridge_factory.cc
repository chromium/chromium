// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_bridge_factory.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "components/android_autofill/browser/android_autofill_provider_bridge_impl.h"
#include "components/android_autofill/browser/form_data_android_bridge_impl.h"
#include "components/android_autofill/browser/form_field_data_android_bridge_impl.h"

namespace autofill {

AndroidAutofillBridgeFactory::AndroidAutofillBridgeFactory() = default;

AndroidAutofillBridgeFactory::~AndroidAutofillBridgeFactory() = default;

// static
AndroidAutofillBridgeFactory& AndroidAutofillBridgeFactory::GetInstance() {
  static base::NoDestructor<AndroidAutofillBridgeFactory> instance;
  return *instance;
}

std::unique_ptr<AndroidAutofillProviderBridge>
AndroidAutofillBridgeFactory::CreateAndroidAutofillProviderBridge(
    AndroidAutofillProviderBridge::Delegate* delegate) {
  if (autofill_provider_android_bridge_testing_factory_) {
    return autofill_provider_android_bridge_testing_factory_.Run(delegate);
  }
  return std::make_unique<AndroidAutofillProviderBridgeImpl>(delegate);
}

std::unique_ptr<FormDataAndroidBridge>
AndroidAutofillBridgeFactory::CreateFormDataAndroidBridge() {
  if (form_data_android_bridge_testing_factory_) {
    return form_data_android_bridge_testing_factory_.Run();
  }
  return std::make_unique<FormDataAndroidBridgeImpl>();
}

std::unique_ptr<FormFieldDataAndroidBridge>
AndroidAutofillBridgeFactory::CreateFormFieldDataAndroidBridge() {
  if (form_field_data_android_bridge_testing_factory_) {
    return form_field_data_android_bridge_testing_factory_.Run();
  }
  return std::make_unique<FormFieldDataAndroidBridgeImpl>();
}

}  // namespace autofill
