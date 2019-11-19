// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/proto_value_conversions.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/proto_visitors.h"

namespace syncer {

namespace {

// ToValueVisitor is a VisitProtoFields()-compatible visitor that serializes
// protos to base::DictionaryValues. To serialize a proto you call ToValue()
// method:
//
//  ToValueVisitor visitor;
//  auto value = visitor.ToValue(proto);
//
// By default all fields visited by VisitProtoFields() are serialized, but
// there are several ways to customize that on per-field / per-proto basis:
//
// 1. If you want to change how fields of a particular proto type are
//    serialized, customize Visit() method:
//
//    template <class P, class F>
//    void Visit(const P& parent_proto,
//               const char* field_name, const F& field);
//
//    By default Visit() serializes |field| and sets it to |value_| under
//    |field_name| name. Default implementation is accessible via VisitImpl().
//
//    For example here is how you would serialize only GreenProto::content
//    for all GreenProto fields:
//
//    template <class P>
//    void Visit(const P& parent_proto,
//               const char* field_name, const sync_pb::GreenProto& field) {
//      if (field.has_content()) {
//        value_->Set(field_name, field.content());
//      }
//    }
//
//    You can further fine-tune this method by specifying parent proto. For
//    example let's say we don't want to serialize fields of type GreenProto
//    that are contained in RedProto:
//
//    void Visit(const sync_pb::RedProto& parent_proto,
//               const char* field_name, const sync_pb::GreenProto& field) {}
//
//    Note: Visit() method only called to serialize fields, and doesn't
//    affect top level protos. I.e. ToValueVisitor().ToValue(GreenProto)
//    won't call methods above.
//
// 2. If you want to change how proto itself is serialized, you need to
//    customize ToValue() method:
//
//    template <class P>
//    std::unique_ptr<base::DictionaryValue> ToValue(const P& proto) const;
//
//    By default ToValue() creates new instance of ToValueVisitor, calls
//    VisitProtoFields(visitor, |proto|) and returns visitor's |value_|.
//    Default implementation is accessible via ToValueImpl().
//
//    For example let's say you want to clobber a sensitive field:
//
//    std::unique_ptr<base::DictionaryValue> ToValue(
//        const sync_pb::GreenProto& proto) const {
//      auto value = ToValueImpl(proto);
//      value->SetString("secret", "<clobbered>");
//      return value;
//    }
//
//    ToValue() doesn't have to return base::DictionaryValue though. It might
//    be more appropriate to serialize GreenProto into a string instead:
//
//    std::unique_ptr<base::Value> ToValue(
//        const sync_pb::GreenProto& proto) const {
//      return std::make_unique<base::Value>(proto.content());
//    }
//
class ToValueVisitor {
 public:
  ToValueVisitor(bool include_specifics = true,
                 base::DictionaryValue* value = nullptr)
    : value_(value)
    , include_specifics_(include_specifics) {}

  template <class P>
  void VisitBytes(const P& parent_proto,
                  const char* field_name,
                  const std::string& field) {
    value_->Set(field_name, BytesToValue(field));
  }

  template <class P, class E>
  void VisitEnum(const P& parent_proto, const char* field_name, E field) {
    value_->Set(field_name, EnumToValue(field));
  }

  template <class P, class F>
  void Visit(const P& parent_proto,
             const char* field_name,
             const google::protobuf::RepeatedPtrField<F>& repeated_field) {
    if (!repeated_field.empty()) {
      std::unique_ptr<base::ListValue> list(new base::ListValue());
      for (const auto& field : repeated_field) {
        list->Append(ToValue(field));
      }
      value_->Set(field_name, std::move(list));
    }
  }

  template <class P, class F>
  void Visit(const P& parent_proto,
             const char* field_name,
             const google::protobuf::RepeatedField<F>& repeated_field) {
    if (!repeated_field.empty()) {
      std::unique_ptr<base::ListValue> list(new base::ListValue());
      for (const auto& field : repeated_field) {
        list->Append(ToValue(field));
      }
      value_->Set(field_name, std::move(list));
    }
  }

  template <class P, class F>
  void Visit(const P& parent_proto, const char* field_name, const F& field) {
    VisitImpl(parent_proto, field_name, field);
  }

  template <class P>
  std::unique_ptr<base::DictionaryValue> ToValue(const P& proto) const {
    return ToValueImpl(proto);
  }

  // Customizations

  // ExperimentsSpecifics flags
  #define IMPLEMENT_VISIT_EXPERIMENT_ENABLED_FIELD(Name) \
    void Visit(const sync_pb::ExperimentsSpecifics&, \
               const char* field_name, \
               const sync_pb::Name& field) { \
      if (field.has_enabled()) { \
        Visit(field, field_name, field.enabled()); \
      } \
    }
  IMPLEMENT_VISIT_EXPERIMENT_ENABLED_FIELD(KeystoreEncryptionFlags)
  IMPLEMENT_VISIT_EXPERIMENT_ENABLED_FIELD(HistoryDeleteDirectives)
  IMPLEMENT_VISIT_EXPERIMENT_ENABLED_FIELD(AutofillCullingFlags)
  IMPLEMENT_VISIT_EXPERIMENT_ENABLED_FIELD(PreCommitUpdateAvoidanceFlags)
  IMPLEMENT_VISIT_EXPERIMENT_ENABLED_FIELD(GcmChannelFlags)
  IMPLEMENT_VISIT_EXPERIMENT_ENABLED_FIELD(GcmInvalidationsFlags)
  #undef IMPLEMENT_VISIT_EXPERIMENT_ENABLED_FIELD

  // EntitySpecifics
  template <class P>
  void Visit(const P& parent_proto,
             const char* field_name,
             const sync_pb::EntitySpecifics& field) {
    if (include_specifics_) {
      VisitImpl(parent_proto, field_name, field);
    }
  }

  // EnhancedBookmarksFlags
  template <class P>
  void Visit(const P& parent_proto,
             const char* field_name,
             const sync_pb::EnhancedBookmarksFlags& field) {
    // Obsolete, don't visit
  }

  // WalletSyncFlags
  template <class P>
  void Visit(const P& parent_proto,
             const char* field_name,
             const sync_pb::WalletSyncFlags& field) {
    // Obsolete, don't visit
  }

  // PasswordSpecifics
  std::unique_ptr<base::DictionaryValue> ToValue(
      const sync_pb::PasswordSpecifics& proto) const {
    auto value = ToValueImpl(proto);
    value->Remove("client_only_encrypted_data", nullptr);
    return value;
  }

  // PasswordSpecificsData
  std::unique_ptr<base::DictionaryValue> ToValue(
      const sync_pb::PasswordSpecificsData& proto) const {
    auto value = ToValueImpl(proto);
    value->SetString("password_value", "<redacted>");
    return value;
  }

  // AutofillWalletSpecifics
  std::unique_ptr<base::DictionaryValue> ToValue(
      const sync_pb::AutofillWalletSpecifics& proto) const {
    auto value = ToValueImpl(proto);
    if (proto.type() != sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS) {
      value->Remove("address", nullptr);
    }
    if (proto.type() != sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD) {
      value->Remove("masked_card", nullptr);
    }
    if (proto.type() != sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA) {
      value->Remove("customer_data", nullptr);
    }
    if (proto.type() !=
        sync_pb::AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA) {
      value->Remove("cloud_token_data", nullptr);
    }
    return value;
  }

  // UniquePosition
  std::unique_ptr<base::Value> ToValue(
      const sync_pb::UniquePosition& proto) const {
    UniquePosition pos = UniquePosition::FromProto(proto);
    return std::make_unique<base::Value>(pos.ToDebugString());
  }

 private:
  template <class P>
  std::unique_ptr<base::DictionaryValue> ToValueImpl(const P& proto) const {
    auto value = std::make_unique<base::DictionaryValue>();
    ToValueVisitor visitor(include_specifics_, value.get());
    VisitProtoFields(visitor, proto);
    return value;
  }

  static std::unique_ptr<base::Value> BytesToValue(const std::string& bytes) {
    std::string bytes_base64;
    base::Base64Encode(bytes, &bytes_base64);
    return std::make_unique<base::Value>(bytes_base64);
  }

  template <class E>
  static std::unique_ptr<base::Value> EnumToValue(E value) {
    return std::make_unique<base::Value>(ProtoEnumToString(value));
  }

  std::unique_ptr<base::Value> ToValue(const std::string& value) const {
    return std::make_unique<base::Value>(value);
  }

  std::unique_ptr<base::Value> ToValue(int64_t value) const {
    return std::make_unique<base::Value>(base::NumberToString(value));
  }
  std::unique_ptr<base::Value> ToValue(uint64_t value) const {
    return std::make_unique<base::Value>(base::NumberToString(value));
  }
  std::unique_ptr<base::Value> ToValue(uint32_t value) const {
    return std::make_unique<base::Value>(base::NumberToString(value));
  }
  std::unique_ptr<base::Value> ToValue(int32_t value) const {
    return std::make_unique<base::Value>(base::NumberToString(value));
  }

  std::unique_ptr<base::Value> ToValue(bool value) const {
    return std::make_unique<base::Value>(value);
  }
  std::unique_ptr<base::Value> ToValue(float value) const {
    return std::make_unique<base::Value>(value);
  }
  std::unique_ptr<base::Value> ToValue(double value) const {
    return std::make_unique<base::Value>(value);
  }

  // Needs to be here to see all ToValue() overloads above.
  template <class P, class F>
  void VisitImpl(P&, const char* field_name, const F& field) {
    value_->Set(field_name, ToValue(field));
  }

  base::DictionaryValue* value_;
  bool include_specifics_;
};

}  // namespace

#define IMPLEMENT_PROTO_TO_VALUE(Proto) \
  std::unique_ptr<base::DictionaryValue> Proto##ToValue( \
      const sync_pb::Proto& proto) { \
    return ToValueVisitor().ToValue(proto); \
  }

#define IMPLEMENT_PROTO_TO_VALUE_INCLUDE_SPECIFICS(Proto) \
  std::unique_ptr<base::DictionaryValue> Proto##ToValue( \
      const sync_pb::Proto& proto, \
      bool include_specifics) { \
    return ToValueVisitor(include_specifics).ToValue(proto); \
  }

IMPLEMENT_PROTO_TO_VALUE(AppListSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AppNotificationSettings)
IMPLEMENT_PROTO_TO_VALUE(AppSettingSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AppSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ArcPackageSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillProfileSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillWalletSpecifics)
IMPLEMENT_PROTO_TO_VALUE(BookmarkSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ClientConfigParams)
IMPLEMENT_PROTO_TO_VALUE(DatatypeAssociationStats)
IMPLEMENT_PROTO_TO_VALUE(DebugEventInfo)
IMPLEMENT_PROTO_TO_VALUE(DebugInfo)
IMPLEMENT_PROTO_TO_VALUE(DeviceInfoSpecifics)
IMPLEMENT_PROTO_TO_VALUE(DictionarySpecifics)
IMPLEMENT_PROTO_TO_VALUE(EncryptedData)
IMPLEMENT_PROTO_TO_VALUE(EntityMetadata)
IMPLEMENT_PROTO_TO_VALUE(EntitySpecifics)
IMPLEMENT_PROTO_TO_VALUE(ExperimentsSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ExtensionSettingSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ExtensionSpecifics)
IMPLEMENT_PROTO_TO_VALUE(FaviconImageSpecifics)
IMPLEMENT_PROTO_TO_VALUE(FaviconTrackingSpecifics)
IMPLEMENT_PROTO_TO_VALUE(GlobalIdDirective)
IMPLEMENT_PROTO_TO_VALUE(HistoryDeleteDirectiveSpecifics)
IMPLEMENT_PROTO_TO_VALUE(LinkedAppIconInfo)
IMPLEMENT_PROTO_TO_VALUE(ManagedUserSettingSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ManagedUserWhitelistSpecifics)
IMPLEMENT_PROTO_TO_VALUE(NavigationRedirect)
IMPLEMENT_PROTO_TO_VALUE(NigoriSpecifics)
IMPLEMENT_PROTO_TO_VALUE(OsPreferenceSpecifics)
IMPLEMENT_PROTO_TO_VALUE(OsPriorityPreferenceSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PasswordSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PasswordSpecificsData)
IMPLEMENT_PROTO_TO_VALUE(PaymentsCustomerData)
IMPLEMENT_PROTO_TO_VALUE(PreferenceSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PrinterPPDReference)
IMPLEMENT_PROTO_TO_VALUE(PrinterSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PriorityPreferenceSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ReadingListSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SearchEngineSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SecurityEventSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SendTabToSelfSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SessionHeader)
IMPLEMENT_PROTO_TO_VALUE(SessionSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SessionTab)
IMPLEMENT_PROTO_TO_VALUE(SessionWindow)
IMPLEMENT_PROTO_TO_VALUE(SyncCycleCompletedEventInfo)
IMPLEMENT_PROTO_TO_VALUE(TabNavigation)
IMPLEMENT_PROTO_TO_VALUE(ThemeSpecifics)
IMPLEMENT_PROTO_TO_VALUE(TimeRangeDirective)
IMPLEMENT_PROTO_TO_VALUE(TypedUrlSpecifics)
IMPLEMENT_PROTO_TO_VALUE(UrlDirective)
IMPLEMENT_PROTO_TO_VALUE(UserConsentSpecifics)
IMPLEMENT_PROTO_TO_VALUE(UserEventSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WalletCreditCardCloudTokenData)
IMPLEMENT_PROTO_TO_VALUE(WalletMaskedCreditCard)
IMPLEMENT_PROTO_TO_VALUE(WalletMetadataSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WalletPostalAddress)
IMPLEMENT_PROTO_TO_VALUE(WebAppSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WifiConfigurationSpecifics)

IMPLEMENT_PROTO_TO_VALUE_INCLUDE_SPECIFICS(ClientToServerMessage)
IMPLEMENT_PROTO_TO_VALUE_INCLUDE_SPECIFICS(ClientToServerResponse)
IMPLEMENT_PROTO_TO_VALUE_INCLUDE_SPECIFICS(SyncEntity)

#undef IMPLEMENT_PROTO_TO_VALUE
#undef IMPLEMENT_PROTO_TO_VALUE_INCLUDE_SPECIFICS

}  // namespace syncer
