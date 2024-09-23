// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/proto_value_conversions.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"
#include "components/sync/protocol/autofill_offer_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/protocol/contact_info_specifics.pb.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/dictionary_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/os_preference_specifics.pb.h"
#include "components/sync/protocol/os_priority_preference_specifics.pb.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"
#include "components/sync/protocol/plus_address_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/printer_specifics.pb.h"
#include "components/sync/protocol/printers_authorization_server_specifics.pb.h"
#include "components/sync/protocol/priority_preference_specifics.pb.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "components/sync/protocol/proto_visitors.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sharing_message_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"

namespace syncer {

namespace {

// ToValueVisitor is a VisitProtoFields()-compatible visitor that serializes
// protos to base::Value. To serialize a proto you call ToValue() method:
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
//    base::Value ToValue(const P& proto) const;
//
//    By default ToValue() creates new instance of ToValueVisitor, calls
//    VisitProtoFields(visitor, |proto|) and returns visitor's |value_|.
//    Default implementation is accessible via ToValueDictImpl().
//
//    For example let's say you want to clobber a sensitive field:
//
//    base::Value ToValue(const sync_pb::GreenProto& proto) const {
//      base::Value::Dict value = ToValueDictImpl(proto);
//      value.Set("secret", "<clobbered>");
//      return base::Value(value);
//    }
//
//    ToValue() doesn't have to return a dictionary though. It might
//    be more appropriate to serialize GreenProto into a string instead:
//
//    base::Value ToValue(const sync_pb::GreenProto& proto) const {
//      return base::Value(proto.content());
//    }
//
class ToValueVisitor {
 public:
  explicit ToValueVisitor(const ProtoValueConversionOptions& options =
                              ProtoValueConversionOptions(),
                          base::Value::Dict* value = nullptr)
      : options_(options), value_(value) {}

  template <class P>
  void VisitBytes(const P& parent_proto,
                  const char* field_name,
                  const std::string& field) {
    value_->Set(field_name,
                base::Base64Encode(base::as_bytes(base::make_span(field))));
  }

  template <class P>
  void VisitBytes(
      const P& parent_proto,
      const char* field_name,
      const google::protobuf::RepeatedPtrField<std::string>& repeated_field) {
    if (!repeated_field.empty()) {
      base::Value::List list;
      for (const auto& field : repeated_field) {
        list.Append(base::Base64Encode(base::as_bytes(base::make_span(field))));
      }
      value_->Set(field_name, std::move(list));
    }
  }

  template <class P>
  void VisitSecret(const P& parent_proto,
                   const char* field_name,
                   const std::string& field) {
    value_->Set(field_name,
                base::StringPrintf("<%zu-byte secret>", field.size()));
  }

  template <class P, class E>
  void VisitEnum(const P& parent_proto, const char* field_name, E field) {
    value_->Set(field_name, ProtoEnumToString(field));
  }

  template <class P, class F>
  void Visit(const P& parent_proto,
             const char* field_name,
             const google::protobuf::RepeatedPtrField<F>& repeated_field) {
    if (!repeated_field.empty()) {
      base::Value::List list;
      for (const auto& field : repeated_field) {
        list.Append(ToValue(field));
      }
      value_->Set(field_name, std::move(list));
    }
  }

  template <class P, class F>
  void Visit(const P& parent_proto,
             const char* field_name,
             const google::protobuf::RepeatedField<F>& repeated_field) {
    if (!repeated_field.empty()) {
      base::Value::List list;
      for (const auto& field : repeated_field) {
        list.Append(ToValue(field));
      }
      value_->Set(field_name, std::move(list));
    }
  }

  template <class P, class F>
  void Visit(const P& parent_proto, const char* field_name, const F& field) {
    VisitImpl(parent_proto, field_name, field);
  }

  template <class P>
  base::Value ToValue(const P& proto) const {
    return base::Value(ToValueDictImpl(proto));
  }

  // Customizations

  // EntitySpecifics
  template <class P>
  void Visit(const P& parent_proto,
             const char* field_name,
             const sync_pb::EntitySpecifics& field) {
    if (options_.include_specifics) {
      VisitImpl(parent_proto, field_name, field);
    }
  }

  // GetUpdateTriggers.
  base::Value ToValue(const sync_pb::GetUpdateTriggers& proto) const {
    base::Value::Dict dict = ToValueDictImpl(proto);
    if (!options_.include_full_get_update_triggers) {
      if (!proto.client_dropped_hints()) {
        dict.Remove("client_dropped_hints");
      }
      if (!proto.invalidations_out_of_sync()) {
        dict.Remove("invalidations_out_of_sync");
      }
      if (proto.local_modification_nudges() == 0) {
        dict.Remove("local_modification_nudges");
      }
      if (proto.datatype_refresh_nudges() == 0) {
        dict.Remove("datatype_refresh_nudges");
      }
      if (!proto.server_dropped_hints()) {
        dict.Remove("server_dropped_hints");
      }
      if (!proto.initial_sync_in_progress()) {
        dict.Remove("initial_sync_in_progress");
      }
      if (!proto.sync_for_resolve_conflict_in_progress()) {
        dict.Remove("sync_for_resolve_conflict_in_progress");
      }
    }
    return base::Value(std::move(dict));
  }

  // AutofillWalletSpecifics
  base::Value ToValue(const sync_pb::AutofillWalletSpecifics& proto) const {
    base::Value::Dict dict = ToValueDictImpl(proto);
    // TODO(crbug.com/40252694): consider whether the VISIT_SECRET macro in
    // proto_visitors.h could replace this.
    if (proto.type() != sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS) {
      dict.Remove("address");
    }
    if (proto.type() != sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD) {
      dict.Remove("masked_card");
    }
    if (proto.type() != sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA) {
      dict.Remove("customer_data");
    }
    if (proto.type() !=
        sync_pb::AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA) {
      dict.Remove("cloud_token_data");
    }
    if (proto.type() != sync_pb::AutofillWalletSpecifics::PAYMENT_INSTRUMENT) {
      dict.Remove("payment_instrument");
    }
    return base::Value(std::move(dict));
  }

  // UniquePosition
  base::Value ToValue(const sync_pb::UniquePosition& proto) const {
    UniquePosition pos = UniquePosition::FromProto(proto);
    return base::Value(pos.ToDebugString());
  }

 private:
  template <class P>
  base::Value::Dict ToValueDictImpl(const P& proto) const {
    base::Value::Dict dict;
    ToValueVisitor visitor(options_, &dict);
    VisitProtoFields(visitor, proto);
    return dict;
  }

  base::Value ToValue(const std::string& value) const {
    return base::Value(value);
  }

  base::Value ToValue(int64_t value) const {
    return base::Value(base::NumberToString(value));
  }
  base::Value ToValue(uint64_t value) const {
    return base::Value(base::NumberToString(value));
  }
  base::Value ToValue(uint32_t value) const {
    return base::Value(base::NumberToString(value));
  }
  base::Value ToValue(int32_t value) const {
    return base::Value(base::NumberToString(value));
  }

  base::Value ToValue(bool value) const { return base::Value(value); }
  base::Value ToValue(float value) const { return base::Value(value); }
  base::Value ToValue(double value) const { return base::Value(value); }

  // Needs to be here to see all ToValue() overloads above.
  template <class P, class F>
  void VisitImpl(P&, const char* field_name, const F& field) {
    value_->Set(field_name, ToValue(field));
  }

  const ProtoValueConversionOptions options_;
  const raw_ptr<base::Value::Dict> value_;
};

}  // namespace

#define IMPLEMENT_PROTO_TO_VALUE(Proto)                     \
  base::Value Proto##ToValue(const sync_pb::Proto& proto) { \
    return ToValueVisitor().ToValue(proto);                 \
  }

#define IMPLEMENT_PROTO_TO_VALUE_WITH_OPTIONS(Proto)                       \
  base::Value Proto##ToValue(const sync_pb::Proto& proto,                  \
                             const ProtoValueConversionOptions& options) { \
    return ToValueVisitor(options).ToValue(proto);                         \
  }

IMPLEMENT_PROTO_TO_VALUE(AppListSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AppSettingSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AppSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ArcPackageSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillOfferSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillProfileSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillWalletCredentialSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillWalletSpecifics)
IMPLEMENT_PROTO_TO_VALUE(AutofillWalletUsageSpecifics)
IMPLEMENT_PROTO_TO_VALUE(BankAccountDetails)
IMPLEMENT_PROTO_TO_VALUE(BookmarkSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ClientConfigParams)
IMPLEMENT_PROTO_TO_VALUE(CollaborationGroupSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ContactInfoSpecifics)
IMPLEMENT_PROTO_TO_VALUE(CookieSpecifics)
IMPLEMENT_PROTO_TO_VALUE(CrossUserSharingPublicKey)
IMPLEMENT_PROTO_TO_VALUE(DebugEventInfo)
IMPLEMENT_PROTO_TO_VALUE(DebugInfo)
IMPLEMENT_PROTO_TO_VALUE(DeviceDetails)
IMPLEMENT_PROTO_TO_VALUE(DeviceInfoSpecifics)
IMPLEMENT_PROTO_TO_VALUE(DictionarySpecifics)
IMPLEMENT_PROTO_TO_VALUE(EncryptedData)
IMPLEMENT_PROTO_TO_VALUE(EntityMetadata)
IMPLEMENT_PROTO_TO_VALUE(EntitySpecifics)
IMPLEMENT_PROTO_TO_VALUE(EwalletDetails)
IMPLEMENT_PROTO_TO_VALUE(ExtensionSettingSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ExtensionSpecifics)
IMPLEMENT_PROTO_TO_VALUE(GlobalIdDirective)
IMPLEMENT_PROTO_TO_VALUE(HistoryDeleteDirectiveSpecifics)
IMPLEMENT_PROTO_TO_VALUE(HistorySpecifics)
IMPLEMENT_PROTO_TO_VALUE(IncomingPasswordSharingInvitationSpecifics)
IMPLEMENT_PROTO_TO_VALUE(LinkedAppIconInfo)
IMPLEMENT_PROTO_TO_VALUE(ManagedUserSettingSpecifics)
IMPLEMENT_PROTO_TO_VALUE(NavigationRedirect)
IMPLEMENT_PROTO_TO_VALUE(NigoriSpecifics)
IMPLEMENT_PROTO_TO_VALUE(OsPreferenceSpecifics)
IMPLEMENT_PROTO_TO_VALUE(OsPriorityPreferenceSpecifics)
IMPLEMENT_PROTO_TO_VALUE(OutgoingPasswordSharingInvitationSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PasswordSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PasswordSpecificsData)
IMPLEMENT_PROTO_TO_VALUE(PasswordSpecificsData_Notes)
IMPLEMENT_PROTO_TO_VALUE(PasswordSpecificsData_Notes_Note)
IMPLEMENT_PROTO_TO_VALUE(PaymentInstrument)
IMPLEMENT_PROTO_TO_VALUE(PaymentsCustomerData)
IMPLEMENT_PROTO_TO_VALUE(PlusAddressSettingSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PlusAddressSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PowerBookmarkSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PreferenceSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PrinterPPDReference)
IMPLEMENT_PROTO_TO_VALUE(PrinterSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PrintersAuthorizationServerSpecifics)
IMPLEMENT_PROTO_TO_VALUE(PriorityPreferenceSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ProductComparisonSpecifics)
IMPLEMENT_PROTO_TO_VALUE(ReadingListSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SavedTabGroupSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SearchEngineSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SecurityEventSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SendTabToSelfPush)
IMPLEMENT_PROTO_TO_VALUE(SendTabToSelfSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SessionHeader)
IMPLEMENT_PROTO_TO_VALUE(SessionSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SessionTab)
IMPLEMENT_PROTO_TO_VALUE(SessionWindow)
IMPLEMENT_PROTO_TO_VALUE(SharingMessageSpecifics)
IMPLEMENT_PROTO_TO_VALUE(SyncCycleCompletedEventInfo)
IMPLEMENT_PROTO_TO_VALUE(TabNavigation)
IMPLEMENT_PROTO_TO_VALUE(ThemeSpecifics)
IMPLEMENT_PROTO_TO_VALUE(TimeRangeDirective)
IMPLEMENT_PROTO_TO_VALUE(TypedUrlSpecifics)
IMPLEMENT_PROTO_TO_VALUE(UnencryptedSharingMessage)
IMPLEMENT_PROTO_TO_VALUE(UrlDirective)
IMPLEMENT_PROTO_TO_VALUE(UserConsentSpecifics)
IMPLEMENT_PROTO_TO_VALUE(UserEventSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WalletCreditCardCloudTokenData)
IMPLEMENT_PROTO_TO_VALUE(WalletMaskedCreditCard)
IMPLEMENT_PROTO_TO_VALUE(WalletMetadataSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WalletPostalAddress)
IMPLEMENT_PROTO_TO_VALUE(WebApkSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WebAppSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WebauthnCredentialSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WifiConfigurationSpecifics)
IMPLEMENT_PROTO_TO_VALUE(WorkspaceDeskSpecifics)

IMPLEMENT_PROTO_TO_VALUE_WITH_OPTIONS(ClientToServerMessage)
IMPLEMENT_PROTO_TO_VALUE_WITH_OPTIONS(ClientToServerResponse)
IMPLEMENT_PROTO_TO_VALUE_WITH_OPTIONS(SyncEntity)

#undef IMPLEMENT_PROTO_TO_VALUE
#undef IMPLEMENT_PROTO_TO_VALUE_WITH_OPTIONS

}  // namespace syncer
