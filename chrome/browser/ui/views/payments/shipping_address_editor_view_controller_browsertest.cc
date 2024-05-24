// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/test_region_data_loader.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/browser/ui/region_combobox_model.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"

namespace payments {

namespace {

const char kLocale[] = "en_US";
const char16_t kNameFull[] = u"Bob Jones";
const char16_t kHomeAddress[] = u"42 Answers-All Avenue";
const char16_t kHomeCity[] = u"Question-City";
const char16_t kHomeZip[] = u"ziiiiiip";
const char16_t kHomePhone[] =
    u"+1 575-555-5555";  // +1 555-555-5555 is invalid :-(.
const char kAnyState[] = "any state";
const char16_t kAnyState16[] = u"any state";
const char kAnyStateCode[] = "AS";
const char16_t kCountryWithoutStates[] = u"Albania";
const char16_t kCountryWithoutStatesCode[] = u"AL";
const char16_t kCountryWithoutStatesPhoneNumber[] = u"+35542223446";

const base::Time kJanuary2017 =
    base::Time::FromSecondsSinceUnixEpoch(1484505871);

}  // namespace

// This test suite is flaky on desktop platforms (crbug.com/1073972) and tests
// UI that is soon to be deprecated, so it is disabled.
class DISABLED_PaymentRequestShippingAddressEditorTest
    : public PaymentRequestBrowserTestBase {
 protected:
  DISABLED_PaymentRequestShippingAddressEditorTest() = default;

  void SetFieldTestValue(autofill::FieldType type) {
    std::u16string textfield_text;
    switch (type) {
      case (autofill::NAME_FULL): {
        textfield_text = kNameFull;
        break;
      }
      case (autofill::ADDRESS_HOME_STREET_ADDRESS): {
        textfield_text = kHomeAddress;
        break;
      }
      case (autofill::ADDRESS_HOME_CITY): {
        textfield_text = kHomeCity;
        break;
      }
      case (autofill::ADDRESS_HOME_STATE): {
        textfield_text = kAnyState16;
        break;
      }
      case (autofill::ADDRESS_HOME_ZIP): {
        textfield_text = kHomeZip;
        break;
      }
      case (autofill::PHONE_HOME_WHOLE_NUMBER): {
        textfield_text = kHomePhone;
        break;
      }
      default:
        ADD_FAILURE() << "Unexpected type: " << type;
    }
    SetEditorTextfieldValue(textfield_text, type);
  }

  void SetCommonFields() {
    SetFieldTestValue(autofill::NAME_FULL);
    SetFieldTestValue(autofill::ADDRESS_HOME_STREET_ADDRESS);
    SetFieldTestValue(autofill::ADDRESS_HOME_CITY);
    SetFieldTestValue(autofill::ADDRESS_HOME_ZIP);
    SetFieldTestValue(autofill::PHONE_HOME_WHOLE_NUMBER);
  }

  // First check if the requested field of |type| exists, if so, set its value
  // in |textfield_text| if it's not null, and return true.
  bool GetEditorTextfieldValueIfExists(autofill::FieldType type,
                                       std::u16string* textfield_text) {
    ValidatingTextfield* textfield =
        static_cast<ValidatingTextfield*>(dialog_view()->GetViewByID(
            EditorViewController::GetInputFieldViewId(type)));
    if (!textfield)
      return false;
    if (textfield_text)
      *textfield_text = textfield->GetText();
    return true;
  }

  // |unset_types| can be null, but when it's not, the fields that are not set
  // get their type added to it, so that the caller can tell which types are not
  // set for a given country. |accept_empty_phone_number| can be set to true to
  // accept a phone type field set to empty, and mark it as unset.
  void ExpectExistingRequiredFields(std::set<autofill::FieldType>* unset_types,
                                    bool accept_empty_phone_number) {
    std::u16string textfield_text;
    if (GetEditorTextfieldValueIfExists(autofill::NAME_FULL, &textfield_text)) {
      EXPECT_EQ(kNameFull, textfield_text);
    } else if (unset_types) {
      unset_types->insert(autofill::NAME_FULL);
    }

    if (GetEditorTextfieldValueIfExists(autofill::ADDRESS_HOME_STREET_ADDRESS,
                                        &textfield_text)) {
      EXPECT_EQ(kHomeAddress, textfield_text);
    } else if (unset_types) {
      unset_types->insert(autofill::ADDRESS_HOME_STREET_ADDRESS);
    }

    if (GetEditorTextfieldValueIfExists(autofill::ADDRESS_HOME_CITY,
                                        &textfield_text)) {
      EXPECT_EQ(kHomeCity, textfield_text);
    } else if (unset_types) {
      unset_types->insert(autofill::ADDRESS_HOME_CITY);
    }

    if (GetEditorTextfieldValueIfExists(autofill::ADDRESS_HOME_ZIP,
                                        &textfield_text)) {
      EXPECT_EQ(kHomeZip, textfield_text);
    } else if (unset_types) {
      unset_types->insert(autofill::ADDRESS_HOME_ZIP);
    }

    if (GetEditorTextfieldValueIfExists(autofill::PHONE_HOME_WHOLE_NUMBER,
                                        &textfield_text)) {
      // The phone can be empty when restored from a saved state, or it may be
      // formatted based on the currently selected country.
      if (!accept_empty_phone_number) {
        EXPECT_EQ(u"+1 575-555-5555", textfield_text);
      } else if (textfield_text.empty()) {
        if (unset_types)
          unset_types->insert(autofill::PHONE_HOME_WHOLE_NUMBER);
      }
    } else if (unset_types) {
      unset_types->insert(autofill::PHONE_HOME_WHOLE_NUMBER);
    }
  }

  std::u16string GetSelectedCountryCode() {
    views::Combobox* country_combobox = static_cast<views::Combobox*>(
        dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
            autofill::ADDRESS_HOME_COUNTRY)));
    DCHECK(country_combobox);
    size_t selected_country_row = country_combobox->GetSelectedRow().value();
    autofill::CountryComboboxModel* country_model =
        static_cast<autofill::CountryComboboxModel*>(
            country_combobox->GetModel());

    return base::ASCIIToUTF16(
        country_model->countries()[selected_country_row]->country_code());
  }

  void SelectCountryInCombobox(const std::u16string& country_name) {
    ResetEventWaiter(DialogEvent::EDITOR_VIEW_UPDATED);
    views::Combobox* country_combobox = static_cast<views::Combobox*>(
        dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
            autofill::ADDRESS_HOME_COUNTRY)));
    ASSERT_NE(nullptr, country_combobox);
    autofill::CountryComboboxModel* country_model =
        static_cast<autofill::CountryComboboxModel*>(
            country_combobox->GetModel());
    size_t i = 0;
    for (; i < country_model->GetItemCount(); i++) {
      if (country_model->GetItemAt(i) == country_name)
        break;
    }
    country_combobox->SetSelectedRow(i);
    country_combobox->OnBlur();
    ASSERT_TRUE(WaitForObservedEvent());
  }

  PersonalDataLoadedObserverMock personal_data_observer_;
  autofill::TestRegionDataLoader test_region_data_loader_;
};

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       SyncData) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  // No shipping profiles are available.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(0U, request->state()->shipping_profiles().size());
  EXPECT_EQ(nullptr, request->state()->selected_shipping_profile());

  test_region_data_loader_.set_synchronous_callback(true);
  OpenShippingAddressEditorScreen();

  std::u16string country_code(GetSelectedCountryCode());

  SetCommonFields();
  // We also need to set the state when no region data is provided.
  SetFieldTestValue(autofill::ADDRESS_HOME_STATE);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN});

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);
  EXPECT_EQ(country_code, profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(kAnyState16, profile->GetRawInfo(autofill::ADDRESS_HOME_STATE));
  ExpectExistingRequiredFields(/*unset_types=*/nullptr,
                               /*accept_empty_phone_number=*/false);
}

// Disabled for flakiness: crbug.com/799028
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       DISABLED_EnterAcceleratorSyncData) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  // No shipping profiles are available.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(0U, request->state()->shipping_profiles().size());
  EXPECT_EQ(nullptr, request->state()->selected_shipping_profile());

  test_region_data_loader_.set_synchronous_callback(true);
  OpenShippingAddressEditorScreen();

  std::u16string country_code(GetSelectedCountryCode());

  SetCommonFields();
  // We also need to set the state when no region data is provided.
  SetFieldTestValue(autofill::ADDRESS_HOME_STATE);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN});

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  views::View* editor_sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_EDITOR_SHEET));
  EXPECT_TRUE(editor_sheet->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);
  EXPECT_EQ(country_code, profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(kAnyState16, profile->GetRawInfo(autofill::ADDRESS_HOME_STATE));
  ExpectExistingRequiredFields(/*unset_types=*/nullptr,
                               /*accept_empty_phone_number=*/false);
}

// TODO(crbug.com/40732468): flaky.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       DISABLED_AsyncData) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(false);
  OpenShippingAddressEditorScreen();
  // Complete the async fetch of region data.
  std::vector<std::pair<std::string, std::string>> regions;
  regions.push_back(std::make_pair(kAnyStateCode, kAnyState));
  test_region_data_loader_.SendAsynchronousData(regions);

  SetCommonFields();
  SetComboboxValue(u"United States", autofill::ADDRESS_HOME_COUNTRY);
  SetComboboxValue(kAnyState16, autofill::ADDRESS_HOME_STATE);

  std::u16string country_code(GetSelectedCountryCode());

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN});

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);
  EXPECT_EQ(country_code, profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(kAnyState16, profile->GetRawInfo(autofill::ADDRESS_HOME_STATE));
  ExpectExistingRequiredFields(/*unset_types=*/nullptr,
                               /*accept_empty_phone_number=*/false);

  // One shipping profile is available and selected.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1UL, request->state()->shipping_profiles().size());
  EXPECT_EQ(request->state()->shipping_profiles().back(),
            request->state()->selected_shipping_profile());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       SwitchingCountryUpdatesViewAndKeepsValues) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(false);
  OpenShippingAddressEditorScreen();
  std::vector<std::pair<std::string, std::string>> regions1;
  regions1.push_back(std::make_pair("1a", "region1a"));
  test_region_data_loader_.SendAsynchronousData(regions1);

  SetCommonFields();

  views::Combobox* country_combobox = static_cast<views::Combobox*>(
      dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
          autofill::ADDRESS_HOME_COUNTRY)));
  ASSERT_NE(nullptr, country_combobox);
  ASSERT_EQ(0u, country_combobox->GetSelectedRow());
  autofill::CountryComboboxModel* country_model =
      static_cast<autofill::CountryComboboxModel*>(
          country_combobox->GetModel());
  size_t num_countries = country_model->countries().size();
  ASSERT_GT(num_countries, 10UL);

  bool use_regions1 = true;
  std::vector<std::pair<std::string, std::string>> regions2;
  regions2.push_back(std::make_pair("2a", "region2a"));
  regions2.push_back(std::make_pair("2b", "region2b"));
  std::set<autofill::FieldType> unset_types;
  for (size_t country_index = 10; country_index < num_countries;
       country_index += num_countries / 10) {
    // The editor updates asynchronously when the country changes.
    ResetEventWaiter(DialogEvent::EDITOR_VIEW_UPDATED);

    views::Combobox* region_combobox = static_cast<views::Combobox*>(
        dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
            autofill::ADDRESS_HOME_STATE)));
    // Some countries don't have a state combobox.
    if (region_combobox) {
      autofill::RegionComboboxModel* region_model =
          static_cast<autofill::RegionComboboxModel*>(
              region_combobox->GetModel());
      if (use_regions1) {
        ASSERT_EQ(2u, region_model->GetItemCount());
        EXPECT_EQ(u"---", region_model->GetItemAt(0));
        EXPECT_EQ(u"region1a", region_model->GetItemAt(1));
      } else {
        ASSERT_EQ(3u, region_model->GetItemCount());
        EXPECT_EQ(u"---", region_model->GetItemAt(0));
        EXPECT_EQ(u"region2a", region_model->GetItemAt(1));
        EXPECT_EQ(u"region2b", region_model->GetItemAt(2));
      }
      use_regions1 = !use_regions1;
    }

    country_combobox->SetSelectedRow(country_index);
    country_combobox->OnBlur();

    // The view update will invalidate the country_combobox / model pointers.
    country_combobox = nullptr;
    country_model = nullptr;
    region_combobox = nullptr;
    ASSERT_TRUE(WaitForObservedEvent());

    // Some types could have been lost in previous countries and may now
    // available in this country.
    std::set<autofill::FieldType> set_types;
    for (auto type : unset_types) {
      ValidatingTextfield* textfield =
          static_cast<ValidatingTextfield*>(dialog_view()->GetViewByID(
              EditorViewController::GetInputFieldViewId(type)));
      if (textfield) {
        // The zip field will be populated after switching to a country for
        // which the profile has a zip set.
        EXPECT_TRUE(textfield->GetText().empty() ||
                    type == autofill::ADDRESS_HOME_ZIP)
            << type;
        SetFieldTestValue(type);
        set_types.insert(type);
      }
    }
    for (auto type : set_types) {
      unset_types.erase(type);
    }

    // Make sure the country combobox was properly reset to the chosen country.
    country_combobox = static_cast<views::Combobox*>(
        dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
            autofill::ADDRESS_HOME_COUNTRY)));
    DCHECK(country_combobox);
    EXPECT_EQ(country_index, country_combobox->GetSelectedRow().value());
    country_model = static_cast<autofill::CountryComboboxModel*>(
        country_combobox->GetModel());
    ASSERT_EQ(num_countries, country_model->countries().size());

    // Update regions.
    test_region_data_loader_.SendAsynchronousData(use_regions1 ? regions1
                                                               : regions2);
    // Make sure the fields common between previous and new country have been
    // properly restored. Note that some country don't support the test phone
    // number so the phone field will be present, but the value will not have
    // been restored from the profile.
    ExpectExistingRequiredFields(&unset_types,
                                 /*accept_empty_phone_number=*/true);
  }
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       FailToLoadRegionData) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  // The synchronous callback is made with no data, which causes a failure.
  test_region_data_loader_.set_synchronous_callback(true);
  OpenShippingAddressEditorScreen();
  // Even though the editor updates asynchronously when the regions fail to
  // load, the update is always completed before the runloop.quit() takes effect
  // when the OpenShippingAddressEditorScreen completes. So now any textual
  // value can be set as the state.
  SetFieldTestValue(autofill::ADDRESS_HOME_STATE);
  SetCommonFields();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN});

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(kAnyState16, profile->GetRawInfo(autofill::ADDRESS_HOME_STATE));
  ExpectExistingRequiredFields(/*unset_types=*/nullptr,
                               /*accept_empty_phone_number=*/false);
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       TimingOutRegionData) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(false);
  OpenShippingAddressEditorScreen();

  // The editor updates asynchronously when the regions data load times out.
  ResetEventWaiter(DialogEvent::EDITOR_VIEW_UPDATED);
  test_region_data_loader_.SendAsynchronousData(
      std::vector<std::pair<std::string, std::string>>());
  ASSERT_TRUE(WaitForObservedEvent());

  // Now any textual value can be set for the ADDRESS_HOME_STATE.
  SetFieldTestValue(autofill::ADDRESS_HOME_STATE);
  SetCommonFields();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN});

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(kAnyState16, profile->GetRawInfo(autofill::ADDRESS_HOME_STATE));
  ExpectExistingRequiredFields(/*unset_types=*/nullptr,
                               /*accept_empty_phone_number=*/false);
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       SelectingIncompleteAddress) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add incomplete address.
  autofill::AutofillProfile profile(
      AddressCountryCode(base::UTF16ToUTF8(kCountryWithoutStatesCode)));
  profile.SetInfo(autofill::NAME_FULL, kNameFull, kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  // One shipping address is available, but it's not selected.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->shipping_profiles().size());
  EXPECT_EQ(nullptr, request->state()->selected_shipping_profile());

  test_region_data_loader_.set_synchronous_callback(true);
  OpenShippingAddressSectionScreen();

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  EXPECT_EQ(kNameFull, GetEditorTextfieldValue(autofill::NAME_FULL));
  // There are no state field in |kCountryWithoutStates|.
  EXPECT_FALSE(
      GetEditorTextfieldValueIfExists(autofill::ADDRESS_HOME_STATE, nullptr));

  // Set all required fields.
  SetCommonFields();
  // The phone number must be replaced by one that is valid for
  // |kCountryWithoutStates|.
  SetEditorTextfieldValue(kCountryWithoutStatesPhoneNumber,
                          autofill::PHONE_HOME_WHOLE_NUMBER);

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN});

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* saved_profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(saved_profile);
  EXPECT_EQ(kCountryWithoutStates,
            saved_profile->GetInfo(autofill::ADDRESS_HOME_COUNTRY, kLocale));
  ExpectExistingRequiredFields(/*unset_types=*/nullptr,
                               /*accept_empty_phone_number=*/false);

  // Still have one shipping address, but the merchant doesn't ship to
  // kCountryWithoutStates.
  EXPECT_EQ(1U, request->state()->shipping_profiles().size());
  EXPECT_EQ(nullptr, request->state()->selected_shipping_profile());
  EXPECT_EQ(request->state()->shipping_profiles().back(),
            request->state()->selected_shipping_option_error_profile());
}

// Flaky on ozone: https://crbug.com/1216184
#if BUILDFLAG(IS_OZONE)
#define MAYBE_FocusFirstField_Name DISABLED_FocusFirstField_Name
#else
#define MAYBE_FocusFirstField_Name FocusFirstField_Name
#endif
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       MAYBE_FocusFirstField_Name) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  // Focus expectations are different in Keyboard Accessible mode.
  dialog_view()->GetFocusManager()->SetKeyboardAccessible(false);

  test_region_data_loader_.set_synchronous_callback(true);
  OpenShippingAddressEditorScreen();

  // We know that the name field is always the first one in a shipping address.
  views::Textfield* textfield =
      static_cast<views::Textfield*>(dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(autofill::NAME_FULL)));
  DCHECK(textfield);
  EXPECT_TRUE(textfield->GetText().empty());
  // Field is not invalid because there is nothing in it.
  EXPECT_FALSE(textfield->GetInvalid());
  EXPECT_TRUE(textfield->HasFocus());
}

// Flaky on ozone: https://crbug.com/1216184
#if BUILDFLAG(IS_OZONE)
#define MAYBE_FocusFirstInvalidField_NotName \
  DISABLED_FocusFirstInvalidField_NotName
#else
#define MAYBE_FocusFirstInvalidField_NotName FocusFirstInvalidField_NotName
#endif
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       MAYBE_FocusFirstInvalidField_NotName) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address with only the name set, so that another view takes focus.
  autofill::AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetInfo(autofill::NAME_FULL, kNameFull, "fr_CA");
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);
  test_region_data_loader_.set_synchronous_callback(true);
  OpenShippingAddressSectionScreen();
  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  views::Textfield* textfield =
      static_cast<views::Textfield*>(dialog_view()->GetViewByID(
          EditorViewController::GetInputFieldViewId(autofill::NAME_FULL)));
  DCHECK(textfield);
  EXPECT_FALSE(textfield->GetText().empty());
  EXPECT_FALSE(textfield->GetInvalid());
  EXPECT_FALSE(textfield->HasFocus());

  // Since we can't easily tell which field is after name, let's just make sure
  // that a view has focus. Unfortunately, we can't cast it to a specific type
  // that we could query for validity (it could be either text or combobox).
  EXPECT_NE(textfield->GetFocusManager()->GetFocusedView(), nullptr);
}

// Tests that the editor accepts an international phone from another country.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       AddInternationalPhoneNumberFromOtherCountry) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);
  test_region_data_loader_.set_synchronous_callback(true);
  std::vector<std::pair<std::string, std::string>> regions1;
  regions1.push_back(std::make_pair("AL", "Alabama"));
  regions1.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SetRegionData(regions1);
  OpenShippingAddressEditorScreen();

  SetCommonFields();
  SetComboboxValue(u"California", autofill::ADDRESS_HOME_STATE);

  // Set an Australian phone number in international format.
  SetEditorTextfieldValue(u"+61 2 9374 4000",
                          autofill::PHONE_HOME_WHOLE_NUMBER);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION});
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);
}

// Tests that the editor accepts a phone number looks like a possible number
// but is actually invalid.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       AddPossiblePhoneNumber) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  OpenShippingAddressEditorScreen();

  SetCommonFields();

  // Set an Australian phone number in local format. This is an invalid
  // US number as there is no area code 029, but it can be considered and parsed
  // as a US number.
  SetEditorTextfieldValue(u"02 9374 4000", autofill::PHONE_HOME_WHOLE_NUMBER);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::PHONE_HOME_WHOLE_NUMBER));
}

// Tests that the editor does not accept a impossible phone number.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       AddImpossiblePhoneNumber) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  OpenShippingAddressEditorScreen();

  SetCommonFields();

  // Trying to set an impossible number, note it has 11 digits.
  SetEditorTextfieldValue(u"02 9374 40001", autofill::PHONE_HOME_WHOLE_NUMBER);

  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::PHONE_HOME_WHOLE_NUMBER));
}

// Tests that updating the country to one with no states clears the state value.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       UpdateToCountryWithoutState) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Create a profile in the US.
  autofill::AutofillProfile california = autofill::test::GetFullProfile();
  california.set_use_count(50U);
  california.set_use_date(kJanuary2017);
  AddAutofillProfile(california);  // California, USA.

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);
  test_region_data_loader_.set_synchronous_callback(true);
  std::vector<std::pair<std::string, std::string>> regions;
  regions.push_back(std::make_pair("AK", "Alaska"));
  regions.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SetRegionData(regions);
  OpenShippingAddressSectionScreen();

  // Opening the address editor by clicking the edit button.
  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  EXPECT_TRUE(list_view);
  EXPECT_EQ(1u, list_view->children().size());
  views::View* edit_button = list_view->children().front()->GetViewByID(
      static_cast<int>(DialogViewID::EDIT_ITEM_BUTTON));
  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnDialogViewAndWait(edit_button);

  SelectCountryInCombobox(kCountryWithoutStates);

  // The phone number must be replaced by one that is valid for
  // |kCountryWithoutStates|.
  SetEditorTextfieldValue(kCountryWithoutStatesPhoneNumber,
                          autofill::PHONE_HOME_WHOLE_NUMBER);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN});

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);
  // Use GetRawInfo to get the country code.
  EXPECT_EQ(kCountryWithoutStatesCode,
            profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  // State/Region is no longer set.
  EXPECT_EQ(u"", profile->GetInfo(autofill::ADDRESS_HOME_STATE, kLocale));
  EXPECT_EQ(50U, profile->use_count());
  EXPECT_EQ(kJanuary2017, profile->use_date());
}

// Tests that there is no error label for an international phone from another
// country.
IN_PROC_BROWSER_TEST_F(
    DISABLED_PaymentRequestShippingAddressEditorTest,
    NoErrorLabelForInternationalPhoneNumberFromOtherCountry) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Create a profile in the US and add a valid AU phone number in international
  // format.
  autofill::AutofillProfile california = autofill::test::GetFullProfile();
  california.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER, u"+61 2 9374 4000");
  AddAutofillProfile(california);

  InvokePaymentRequestUI();
  OpenShippingAddressSectionScreen();

  // There should be no error label.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  EXPECT_EQ(nullptr, sheet->children().front()->GetViewByID(
                         static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR)));
}

// Tests that there is no error label for an phone number that can be
// technically parsed as a US number even if it is actually invalid.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       NoErrorLabelForPossibleButInvalidPhoneNumber) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Create a profile in the US and add a valid AU phone number in local format.
  autofill::AutofillProfile california = autofill::test::GetFullProfile();
  california.set_use_count(50U);
  california.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER, u"02 9374 4000");
  AddAutofillProfile(california);

  InvokePaymentRequestUI();
  OpenShippingAddressSectionScreen();

  // There should not be an error label for the phone number.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  EXPECT_EQ(nullptr, sheet->children().front()->GetViewByID(
                         static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR)));
}

// Tests that there is error label for an impossible phone number.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       ErrorLabelForImpossiblePhoneNumber) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Create a profile in the US and add a impossible number.
  autofill::AutofillProfile california = autofill::test::GetFullProfile();
  california.set_use_count(50U);
  california.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER, u"02 9374 40001");
  AddAutofillProfile(california);

  InvokePaymentRequestUI();
  OpenShippingAddressSectionScreen();

  // There should be an error label for the phone number.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  views::View* error_label = sheet->children().front()->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  EXPECT_EQ(u"Phone number required",
            static_cast<views::Label*>(error_label)->GetText());
}

// Tests that if the a profile has a country and no state, the editor makes the
// user pick a state. This should also disable the "Done" button.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       CountryButNoState) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address with a country but no state.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(false);
  OpenShippingAddressSectionScreen();

  // There should be an error label for the address.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  views::View* error_label = sheet->children().front()->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  EXPECT_EQ(u"Enter a valid address",
            static_cast<views::Label*>(error_label)->GetText());

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);
  std::vector<std::pair<std::string, std::string>> regions1;
  regions1.push_back(std::make_pair("AL", "Alabama"));
  regions1.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SendAsynchronousData(regions1);
  // Expect that the country is set correctly.
  EXPECT_EQ(u"United States", GetComboboxValue(autofill::ADDRESS_HOME_COUNTRY));

  // Expect that no state is selected.
  EXPECT_EQ(u"---", GetComboboxValue(autofill::ADDRESS_HOME_STATE));

  // Expect that the save button is disabled.
  views::View* save_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SAVE_ADDRESS_BUTTON));
  EXPECT_FALSE(save_button->GetEnabled());
}

// TODO(crbug.com/40524578): This address should be invalid.
// Tests that if the a profile has a country and an invalid state for the
// country, the address is considered valid.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       CountryAndInvalidState) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address with a country but no state.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"INVALIDSTATE", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);
  test_region_data_loader_.set_synchronous_callback(false);
  OpenShippingAddressSectionScreen();

  // There should be no error label.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  EXPECT_EQ(nullptr, sheet->children().front()->GetViewByID(
                         static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR)));
}

// Tests that if the a profile has no country and no state, the editor sets the
// country and lets the user pick a state. This should also disable the "Done"
// button.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       NoCountryNoState) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address without a country or no state.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"", kLocale);
  profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(false);
  OpenShippingAddressSectionScreen();

  // There should be an error label for the address.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  views::View* error_label = sheet->children().front()->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  EXPECT_EQ(u"Enter a valid address",
            static_cast<views::Label*>(error_label)->GetText());

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);
  std::vector<std::pair<std::string, std::string>> regions1;
  regions1.push_back(std::make_pair("AL", "Alabama"));
  regions1.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SendAsynchronousData(regions1);

  // Expect that the default country was selected.
  EXPECT_FALSE(GetComboboxValue(autofill::ADDRESS_HOME_COUNTRY).empty());

  // Expect that no state is selected.
  EXPECT_EQ(u"---", GetComboboxValue(autofill::ADDRESS_HOME_STATE));

  // Expect that the save button is disabled.
  views::View* save_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SAVE_ADDRESS_BUTTON));
  EXPECT_FALSE(save_button->GetEnabled());
}

// Tests that if the a profile has no country and an invalid state for the
// default country, the editor sets the country and lets the user pick a state.
// This should also disable the "Done" button.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       NoCountryInvalidState) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address without a country or no state.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"", kLocale);
  profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"INVALIDSTATE", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(false);
  OpenShippingAddressSectionScreen();

  // There should be an error label for the address.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  views::View* error_label = sheet->children().front()->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  EXPECT_EQ(u"Enter a valid address",
            static_cast<views::Label*>(error_label)->GetText());

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);
  std::vector<std::pair<std::string, std::string>> regions1;
  regions1.push_back(std::make_pair("AL", "Alabama"));
  regions1.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SendAsynchronousData(regions1);

  // Expect that the default country was selected.
  EXPECT_FALSE(GetComboboxValue(autofill::ADDRESS_HOME_COUNTRY).empty());

  // Expect that no state is selected.
  EXPECT_EQ(u"---", GetComboboxValue(autofill::ADDRESS_HOME_STATE));

  // Expect that the save button is disabled.
  views::View* save_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SAVE_ADDRESS_BUTTON));
  EXPECT_FALSE(save_button->GetEnabled());
}

// TODO(crbug.com/40524404): The profile should be considered valid.
// Tests that if the a profile has no country but has a valid state for the
// default country, the editor sets the country and the state for the user.
// This should also enable the "Done" button.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       NoCountryValidState_SyncRegionLoad) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address without a country but a valid state for the default country.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"", kLocale);
  profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"California", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(true);
  std::vector<std::pair<std::string, std::string>> regions;
  regions.push_back(std::make_pair("AL", "Alabama"));
  regions.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SetRegionData(regions);
  OpenShippingAddressSectionScreen();

  // There should be an error label for the address.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  views::View* error_label = sheet->children().front()->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  EXPECT_EQ(u"Enter a valid address",
            static_cast<views::Label*>(error_label)->GetText());

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  // Expect that the default country was selected.
  EXPECT_FALSE(GetComboboxValue(autofill::ADDRESS_HOME_COUNTRY).empty());

  // Expect that the state was selected.
  EXPECT_EQ(u"California", GetComboboxValue(autofill::ADDRESS_HOME_STATE));

  // Expect that the save button is enabled, since the profile is now valid.
  views::View* save_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SAVE_ADDRESS_BUTTON));
  EXPECT_TRUE(save_button->GetEnabled());
}

// TODO(crbug.com/40524404): The profile should be considered valid.
// Tests that if the a profile has no country but has a valid state for the
// default country, the editor sets the country and the state for the user.
// This should also enable the "Done" button.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       NoCountryValidState_AsyncRegionLoad) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address without a country but a valid state for the default country.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"", kLocale);
  profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"California", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(false);
  OpenShippingAddressSectionScreen();

  // There should be an error label for the address.
  views::View* sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW));
  ASSERT_EQ(1u, sheet->children().size());
  views::View* error_label = sheet->children().front()->GetViewByID(
      static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  EXPECT_EQ(u"Enter a valid address",
            static_cast<views::Label*>(error_label)->GetText());

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  // Send the region data.
  std::vector<std::pair<std::string, std::string>> regions;
  regions.push_back(std::make_pair("AL", "Alabama"));
  regions.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SendAsynchronousData(regions);

  // Expect that the default country was selected.
  EXPECT_FALSE(GetComboboxValue(autofill::ADDRESS_HOME_COUNTRY).empty());

  // Expect that the state was selected.
  EXPECT_EQ(u"California", GetComboboxValue(autofill::ADDRESS_HOME_STATE));

  // Expect that the save button is enabled, since the profile is now valid.
  views::View* save_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::SAVE_ADDRESS_BUTTON));
  EXPECT_TRUE(save_button->GetEnabled());
}

// Tests that the state dropdown is set to the right value if the value from the
// profile is a region code.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       DefaultRegion_RegionCode) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address without a country but a valid state for the default country.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"", kLocale);
  profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"ca", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(true);
  std::vector<std::pair<std::string, std::string>> regions;
  regions.push_back(std::make_pair("AL", "Alabama"));
  regions.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SetRegionData(regions);
  OpenShippingAddressSectionScreen();

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  // Expect that the state was selected.
  EXPECT_EQ(u"California", GetComboboxValue(autofill::ADDRESS_HOME_STATE));
}

// Tests that the state dropdown is set to the right value if the value from the
// profile is a region name.
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       DefaultRegion_RegionName) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address without a country but a valid state for the default country.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"", kLocale);
  profile.SetInfo(autofill::ADDRESS_HOME_STATE, u"california", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  test_region_data_loader_.set_synchronous_callback(true);
  std::vector<std::pair<std::string, std::string>> regions;
  regions.push_back(std::make_pair("AL", "Alabama"));
  regions.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SetRegionData(regions);
  OpenShippingAddressSectionScreen();

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  // Expect that the state was selected.
  EXPECT_EQ(u"California", GetComboboxValue(autofill::ADDRESS_HOME_STATE));
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       NoCountryProfileDoesntSetCountryToLocale) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  // Add address without a country but a valid state for the default country.
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  profile.SetInfo(autofill::NAME_FULL, kNameFull, kLocale);
  profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY, u"", kLocale);
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  OpenShippingAddressSectionScreen();

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  ClickOnBackArrow();
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(u"", request->state()->shipping_profiles()[0]->GetInfo(
                     autofill::ADDRESS_HOME_COUNTRY, kLocale));
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       SyncDataInIncognito) {
  SetIncognito();
  NavigateTo("/payment_request_dynamic_shipping_test.html");
  InvokePaymentRequestUI();
  SetRegionDataLoader(&test_region_data_loader_);

  // No shipping profiles are available.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(0U, request->state()->shipping_profiles().size());
  EXPECT_EQ(nullptr, request->state()->selected_shipping_profile());

  test_region_data_loader_.set_synchronous_callback(true);
  OpenShippingAddressEditorScreen();

  std::u16string country_code(GetSelectedCountryCode());

  SetCommonFields();
  // We also need to set the state when no region data is provided.
  SetFieldTestValue(autofill::ADDRESS_HOME_STATE);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN});

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  // In incognito, the profile should be available in shipping_profiles but it
  // shouldn't be saved to the PersonalDataManager.
  ASSERT_EQ(0UL,
            personal_data_manager->address_data_manager().GetProfiles().size());

  ASSERT_EQ(1UL, request->state()->shipping_profiles().size());
  autofill::AutofillProfile* profile =
      request->state()->shipping_profiles().back();
  DCHECK(profile);
  EXPECT_EQ(country_code, profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(kAnyState16, profile->GetRawInfo(autofill::ADDRESS_HOME_STATE));
  ExpectExistingRequiredFields(/*unset_types=*/nullptr,
                               /*accept_empty_phone_number=*/false);
}

// Tests that there is error label for an impossible
IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       RetryWithShippingAddressErrors) {
  NavigateTo("/payment_request_retry_with_shipping_address_errors.html");

  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);

  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(address.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  PayWithCreditCard(u"123");
  RetryPaymentRequest(
      "{"
      "  shippingAddress: {"
      "    addressLine: 'ADDRESS LINE ERROR',"
      "    city: 'CITY ERROR'"
      "  }"
      "}",
      DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED, dialog_view());

  EXPECT_EQ(u"ADDRESS LINE ERROR",
            GetErrorLabelForType(autofill::ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(u"CITY ERROR", GetErrorLabelForType(autofill::ADDRESS_HOME_CITY));
}

// Tests that there is error label for an impossible
IN_PROC_BROWSER_TEST_F(
    DISABLED_PaymentRequestShippingAddressEditorTest,
    RetryWithShippingAddressErrors_HasSameValueButDifferentErrorsShown) {
  NavigateTo("/payment_request_retry_with_shipping_address_errors.html");

  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  // Set the same value in both of address line and city field.
  address.SetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS, u"Elysium");
  address.SetRawInfo(autofill::ADDRESS_HOME_CITY, u"Elysium");
  AddAutofillProfile(address);

  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(address.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  PayWithCreditCard(u"123");

  RetryPaymentRequest(
      "{"
      "  shippingAddress: {"
      "    addressLine: 'ADDRESS LINE ERROR',"
      "    city: 'CITY ERROR'"
      "  }"
      "}",
      DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED, dialog_view());

  EXPECT_EQ(u"ADDRESS LINE ERROR",
            GetErrorLabelForType(autofill::ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(u"CITY ERROR", GetErrorLabelForType(autofill::ADDRESS_HOME_CITY));
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       RetryWithShippingAddressErrors_NoRequestShippingOption) {
  NavigateTo("/payment_request_retry_with_no_payment_options.html");

  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  AddAutofillProfile(address);

  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(address.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  PayWithCreditCard(u"123");
  RetryPaymentRequest(
      "{"
      "  shippingAddress: {"
      "    addressLine: 'ADDRESS LINE ERROR',"
      "    city: 'CITY ERROR'"
      "  }"
      "}",
      dialog_view());

  const int kErrorLabelOffset =
      static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET);
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(kErrorLabelOffset +
                                       autofill::ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(kErrorLabelOffset +
                                                autofill::ADDRESS_HOME_CITY));
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestShippingAddressEditorTest,
                       UpdateWithShippingAddressErrors) {
  NavigateTo("/payment_request_dynamic_shipping_test.html");

  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetRawInfo(autofill::ADDRESS_HOME_COUNTRY, u"KR");
  AddAutofillProfile(address);

  InvokePaymentRequestUI();
  OpenShippingAddressSectionScreen();

  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED});
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_EQ(u"ADDRESS LINE ERROR",
            GetErrorLabelForType(autofill::ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(u"CITY ERROR", GetErrorLabelForType(autofill::ADDRESS_HOME_CITY));
}

}  // namespace payments
