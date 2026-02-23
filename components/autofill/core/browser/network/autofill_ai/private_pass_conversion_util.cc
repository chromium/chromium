// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/private_pass_conversion_util.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/proto/autofill_ai_chrome_metadata.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/wallet/core/browser/proto/common.pb.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"

namespace autofill {

namespace {

using ::wallet::Any;
using ::wallet::PrivatePass;
using DriverLicense = ::wallet::PrivatePass::DriverLicense;
using IdCard = ::wallet::PrivatePass::IdCard;
using KnownTravelerNumber = ::wallet::PrivatePass::KnownTravelerNumber;
using Passport = ::wallet::PrivatePass::Passport;
using RedressNumber = ::wallet::PrivatePass::RedressNumber;

using enum AttributeTypeName;

Any SerializeMetadata(const EntityInstance& entity) {
  ChromeValuablesMetadata metadata = SerializeChromeValuablesMetadata(entity);
  Any serialised_metadata;
  serialised_metadata.set_type_url(
      base::StrCat({"type.googleapis.com/", metadata.GetTypeName()}));
  metadata.SerializeToString(serialised_metadata.mutable_value());
  return serialised_metadata;
}

// Helper class to populate the fields of a given private `Pass` proto with
// attributes of an entity.
template <typename Pass>
class AttributeSetter {
 public:
  explicit AttributeSetter(const EntityInstance& entity, Pass& pass)
      : entity_(entity), pass_(pass) {}

  void SetString(AttributeTypeName attribute_name,
                 void (Pass::*setter)(const std::string&)) const {
    base::optional_ref<const AttributeInstance> attribute =
        entity_->attribute(AttributeType(attribute_name));
    if (!attribute.has_value()) {
      return;
    }
    std::invoke(setter, pass_,
                base::UTF16ToUTF8(attribute->GetCompleteRawInfo()));
  }

  std::optional<PrivatePass::NaiveDate> AttributeToDate(
      AttributeTypeName attribute_name) {
    base::optional_ref<const AttributeInstance> attribute =
        entity_->attribute(AttributeType(attribute_name));
    if (!attribute.has_value()) {
      return std::nullopt;
    }
    // Note that for retrieving date components the app locale doesn't matter.
    FieldType date_type = attribute->type().field_type();
    std::u16string day = attribute->GetInfo(
        date_type, "", AutofillFormatString(u"D", FormatString_Type_DATE));
    std::u16string month = attribute->GetInfo(
        date_type, "", AutofillFormatString(u"M", FormatString_Type_DATE));
    std::u16string year = attribute->GetInfo(
        date_type, "", AutofillFormatString(u"YYYY", FormatString_Type_DATE));
    return ParseDate(day, month, year);
  }

 private:
  std::optional<PrivatePass::NaiveDate> ParseDate(
      const std::u16string& day_str,
      const std::u16string& month_str,
      const std::u16string& year_str) {
    int day, month, year;
    if (day_str.empty() || !base::StringToInt(day_str, &day) ||
        month_str.empty() || !base::StringToInt(month_str, &month) ||
        year_str.empty() || !base::StringToInt(year_str, &year)) {
      return std::nullopt;
    }
    PrivatePass::NaiveDate proto;
    proto.set_day(day);
    proto.set_month(month);
    proto.set_year(year);
    return proto;
  }

  const raw_ref<const EntityInstance> entity_;
  raw_ref<Pass> pass_;
};

#define MAYBE_SET_DATE(setter, attribute_name, proto_field) \
  if (std::optional<PrivatePass::NaiveDate> maybe_date =    \
          setter.AttributeToDate(attribute_name)) {         \
    *proto_field = std::move(*maybe_date);                  \
  }

Passport EntityInstanceToPassport(const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kPassport);
  Passport pass;
  AttributeSetter setter(entity, pass);
  setter.SetString(kPassportName, &Passport::set_owner_name);
  setter.SetString(kPassportNumber, &Passport::set_passport_number);
  setter.SetString(kPassportCountry, &Passport::set_country_code);
  MAYBE_SET_DATE(setter, kPassportIssueDate, pass.mutable_issue_date());
  MAYBE_SET_DATE(setter, kPassportExpirationDate,
                 pass.mutable_expiration_date());
  return pass;
}

DriverLicense EntityInstanceToDriverLicense(const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kDriversLicense);
  DriverLicense pass;
  AttributeSetter setter(entity, pass);
  setter.SetString(kDriversLicenseName, &DriverLicense::set_owner_name);
  setter.SetString(kDriversLicenseNumber,
                   &DriverLicense::set_driver_license_number);
  setter.SetString(kDriversLicenseState, &DriverLicense::set_region);
  MAYBE_SET_DATE(setter, kDriversLicenseIssueDate, pass.mutable_issue_date());
  MAYBE_SET_DATE(setter, kDriversLicenseExpirationDate,
                 pass.mutable_expiration_date());
  return pass;
}

IdCard EntityInstanceToIdCard(const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kNationalIdCard);
  IdCard pass;
  AttributeSetter setter(entity, pass);
  setter.SetString(kNationalIdCardName, &IdCard::set_owner_name);
  setter.SetString(kNationalIdCardNumber, &IdCard::set_id_number);
  setter.SetString(kNationalIdCardCountry, &IdCard::set_country_code);
  MAYBE_SET_DATE(setter, kNationalIdCardIssueDate, pass.mutable_issue_date());
  MAYBE_SET_DATE(setter, kNationalIdCardExpirationDate,
                 pass.mutable_expiration_date());
  return pass;
}

KnownTravelerNumber EntityInstanceToKnownTravelerNumber(
    const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kKnownTravelerNumber);
  KnownTravelerNumber pass;
  AttributeSetter setter(entity, pass);
  setter.SetString(kKnownTravelerNumberName,
                   &KnownTravelerNumber::set_owner_name);
  setter.SetString(kKnownTravelerNumberNumber,
                   &KnownTravelerNumber::set_known_traveler_number);
  MAYBE_SET_DATE(setter, kKnownTravelerNumberExpirationDate,
                 pass.mutable_expiration_date());
  return pass;
}

RedressNumber EntityInstanceToRedressNumber(const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kRedressNumber);
  RedressNumber pass;
  AttributeSetter setter(entity, pass);
  setter.SetString(kRedressNumberName, &RedressNumber::set_owner_name);
  setter.SetString(kRedressNumberNumber, &RedressNumber::set_redress_number);
  return pass;
}

#undef MAYBE_SET_DATE

}  // namespace

PrivatePass EntityInstanceToPrivatePass(const EntityInstance& entity) {
  PrivatePass pass;
  pass.set_pass_id(entity.guid().value());
  *pass.mutable_client_data()->mutable_chrome_client_data() =
      SerializeMetadata(entity);
  switch (entity.type().name()) {
    case EntityTypeName::kPassport:
      *pass.mutable_passport() = EntityInstanceToPassport(entity);
      break;
    case EntityTypeName::kDriversLicense:
      *pass.mutable_driver_license() = EntityInstanceToDriverLicense(entity);
      break;
    case EntityTypeName::kNationalIdCard:
      *pass.mutable_id_card() = EntityInstanceToIdCard(entity);
      break;
    case EntityTypeName::kKnownTravelerNumber:
      *pass.mutable_known_traveler_number() =
          EntityInstanceToKnownTravelerNumber(entity);
      break;
    case EntityTypeName::kRedressNumber:
      *pass.mutable_redress_number() = EntityInstanceToRedressNumber(entity);
      break;
    // Non-private pass types are not supported.
    case EntityTypeName::kVehicle:
    case EntityTypeName::kFlightReservation:
    case EntityTypeName::kOrder:
      NOTREACHED();
  }
  return pass;
}

}  // namespace autofill
