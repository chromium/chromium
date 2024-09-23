// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_BRIDGE_FACTORY_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_BRIDGE_FACTORY_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "components/android_autofill/browser/android_autofill_provider_bridge.h"

namespace autofill {

class FormDataAndroidBridge;
class FormFieldDataAndroidBridge;

// Factory for all C++ <-> Java bridges in `//components/android_autofill`. All
// factory methods allow setting testing factories.
class AndroidAutofillBridgeFactory {
 public:
  static AndroidAutofillBridgeFactory& GetInstance();

  // Creates and returns a `AndroidAutofillProviderBridge`.
  std::unique_ptr<AndroidAutofillProviderBridge>
  CreateAndroidAutofillProviderBridge(
      AndroidAutofillProviderBridge::Delegate* delegate);

  // Creates and returns a `FormDataAndroidBridge`.
  std::unique_ptr<FormDataAndroidBridge> CreateFormDataAndroidBridge();

  // Creates and returns a `FormFieldDataAndroidBridge`.
  std::unique_ptr<FormFieldDataAndroidBridge>
  CreateFormFieldDataAndroidBridge();

  // Sets a testing factory for `AndroidAutofillProviderBridge`s. If set, the
  // testing factory is used in the factory method.
  using AndroidAutofillProviderBridgeTestingFactory =
      base::RepeatingCallback<std::unique_ptr<AndroidAutofillProviderBridge>(
          AndroidAutofillProviderBridge::Delegate* delegate)>;
  void SetAndroidAutofillProviderTestingFactory(
      AndroidAutofillProviderBridgeTestingFactory factory) {
    autofill_provider_android_bridge_testing_factory_ = std::move(factory);
  }

  // Sets a testing factory for `FormDataAndroidBridge`s. If set, the
  // testing factory is used in the factory method.
  using FormDataAndroidBridgeTestingFactory =
      base::RepeatingCallback<std::unique_ptr<FormDataAndroidBridge>()>;
  void SetFormDataAndroidTestingFactory(
      FormDataAndroidBridgeTestingFactory factory) {
    form_data_android_bridge_testing_factory_ = std::move(factory);
  }

  // Sets a testing factory for `FormFieldDataAndroidBridge`s. If set, the
  // testing factory is used in the factory method.
  using FormFieldDataAndroidBridgeTestingFactory =
      base::RepeatingCallback<std::unique_ptr<FormFieldDataAndroidBridge>()>;
  void SetFormFieldDataAndroidTestingFactory(
      FormFieldDataAndroidBridgeTestingFactory factory) {
    form_field_data_android_bridge_testing_factory_ = std::move(factory);
  }

 private:
  friend class base::NoDestructor<AndroidAutofillBridgeFactory>;

  AndroidAutofillBridgeFactory();
  ~AndroidAutofillBridgeFactory();

  AndroidAutofillProviderBridgeTestingFactory
      autofill_provider_android_bridge_testing_factory_;
  FormDataAndroidBridgeTestingFactory form_data_android_bridge_testing_factory_;
  FormFieldDataAndroidBridgeTestingFactory
      form_field_data_android_bridge_testing_factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_BRIDGE_FACTORY_H_
