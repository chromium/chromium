// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_I18N_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_I18N_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"

namespace i18n {
namespace addressinput {
struct AddressData;
}
}  // namespace i18n

namespace autofill {

class AutofillProfile;
class AutofillType;

namespace i18n {

// Creates an AddressData object for internationalized address display or
// validation using |get_info| for field values.
std::unique_ptr<::i18n::addressinput::AddressData> CreateAddressData(
    const base::RepeatingCallback<std::u16string(const AutofillType&)>&
        get_info);

// Creates an |AddressData| from |profile|.
std::unique_ptr<::i18n::addressinput::AddressData>
CreateAddressDataFromAutofillProfile(const AutofillProfile& profile,
                                     const std::string& app_locale);

// Returns the corresponding Autofill server type for |field|.
FieldType TypeForField(::i18n::addressinput::AddressField field);

// Sets |field| to the corresponding address field for the Autofill
// |server_type|. Returns |true| if |server_type| can be represented as an
// address field. The |field| parameter can be NULL.
bool FieldForType(FieldType server_type,
                  ::i18n::addressinput::AddressField* field);

// Returns whether the field specified by |server_type| is required for this
// |country_code|, according to the libaddressinput metadata.
bool IsFieldRequired(FieldType server_type, const std::string& country_code);

}  // namespace i18n
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ADDRESS_I18N_H_
