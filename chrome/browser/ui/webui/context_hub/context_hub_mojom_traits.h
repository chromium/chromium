// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_MOJOM_TRAITS_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-forward.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "components/personal_context/proto/features/smart_search.pb.h"
#include "mojo/public/cpp/bindings/array_traits_protobuf.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace mojo {

template <>
struct EnumTraits<browser::context_hub::mojom::AutoTodoStatus,
                  context_hub::AutoTodoEntry::Status> {
  static browser::context_hub::mojom::AutoTodoStatus ToMojom(
      context_hub::AutoTodoEntry::Status input);
  static context_hub::AutoTodoEntry::Status FromMojom(
      browser::context_hub::mojom::AutoTodoStatus input);
};

template <>
struct EnumTraits<browser::context_hub::mojom::AutoTodoGroup,
                  context_hub::ThirdPartyData::GroupType> {
  static browser::context_hub::mojom::AutoTodoGroup ToMojom(
      context_hub::ThirdPartyData::GroupType input);
  static context_hub::ThirdPartyData::GroupType FromMojom(
      browser::context_hub::mojom::AutoTodoGroup input);
};

template <>
struct StructTraits<browser::context_hub::mojom::GmailReferenceDataView,
                    context_hub::SourceReference> {
  static const GURL& message_url(const context_hub::SourceReference& ref) {
    return ref.url;
  }

  static const std::string& subject(const context_hub::SourceReference& ref) {
    return ref.subject;
  }

  static bool Read(browser::context_hub::mojom::GmailReferenceDataView data,
                   context_hub::SourceReference* out);
};

template <>
struct UnionTraits<browser::context_hub::mojom::SourceReferenceDataView,
                   context_hub::SourceReference> {
  static browser::context_hub::mojom::SourceReferenceDataView::Tag GetTag(
      const context_hub::SourceReference& ref) {
    return browser::context_hub::mojom::SourceReferenceDataView::Tag::kGmail;
  }

  static const context_hub::SourceReference& gmail(
      const context_hub::SourceReference& ref) {
    return ref;
  }

  static browser::context_hub::mojom::PhotosReferencePtr photos(
      const context_hub::SourceReference& ref);

  static browser::context_hub::mojom::DriveFilePtr drive(
      const context_hub::SourceReference& ref);

  static bool Read(browser::context_hub::mojom::SourceReferenceDataView data,
                   context_hub::SourceReference* out);
};

template <>
struct StructTraits<browser::context_hub::mojom::FirstPartyDataDataView,
                    context_hub::FirstPartyData> {
  static const std::vector<context_hub::SourceReference>& source_references(
      const context_hub::FirstPartyData& first_party) {
    return first_party.source_references;
  }

  static const GURL& actionable_url(
      const context_hub::FirstPartyData& first_party) {
    return first_party.actionable_url;
  }

  static bool Read(browser::context_hub::mojom::FirstPartyDataDataView data,
                   context_hub::FirstPartyData* out);
};

template <>
struct StructTraits<browser::context_hub::mojom::ThirdPartyDataDataView,
                    context_hub::ThirdPartyData> {
  static int64_t tab_id(const context_hub::ThirdPartyData& third_party) {
    return third_party.tab_id;
  }

  static base::Time last_active_timestamp(
      const context_hub::ThirdPartyData& third_party) {
    return third_party.last_active_timestamp;
  }

  static context_hub::ThirdPartyData::GroupType group_type(
      const context_hub::ThirdPartyData& third_party) {
    return third_party.group_type;
  }

  static bool Read(browser::context_hub::mojom::ThirdPartyDataDataView data,
                   context_hub::ThirdPartyData* out);
};

template <>
struct UnionTraits<
    browser::context_hub::mojom::AutoTodoDataDataView,
    std::variant<context_hub::FirstPartyData, context_hub::ThirdPartyData>> {
  static browser::context_hub::mojom::AutoTodoDataDataView::Tag GetTag(
      const std::variant<context_hub::FirstPartyData,
                         context_hub::ThirdPartyData>& data) {
    using Tag = browser::context_hub::mojom::AutoTodoDataDataView::Tag;
    return std::visit(
        absl::Overload{
            [](const context_hub::FirstPartyData&) { return Tag::kFirstParty; },
            [](const context_hub::ThirdPartyData&) { return Tag::kThirdParty; },
        },
        data);
  }

  static const context_hub::FirstPartyData& first_party(
      const std::variant<context_hub::FirstPartyData,
                         context_hub::ThirdPartyData>& data) {
    return std::get<context_hub::FirstPartyData>(data);
  }

  static const context_hub::ThirdPartyData& third_party(
      const std::variant<context_hub::FirstPartyData,
                         context_hub::ThirdPartyData>& data) {
    return std::get<context_hub::ThirdPartyData>(data);
  }

  static bool Read(browser::context_hub::mojom::AutoTodoDataDataView data,
                   std::variant<context_hub::FirstPartyData,
                                context_hub::ThirdPartyData>* out);
};

template <>
struct StructTraits<browser::context_hub::mojom::AutoTodoItemDataView,
                    context_hub::AutoTodoEntry> {
  static const std::string& id(const context_hub::AutoTodoEntry& entry) {
    return entry.id;
  }
  static const std::string& title(const context_hub::AutoTodoEntry& entry) {
    return entry.title;
  }
  static const std::string& description(
      const context_hub::AutoTodoEntry& entry) {
    return entry.description;
  }
  static context_hub::AutoTodoEntry::Status status(
      const context_hub::AutoTodoEntry& entry) {
    return entry.status;
  }
  static float score(const context_hub::AutoTodoEntry& entry) {
    return entry.importance_score;
  }
  static const std::variant<context_hub::FirstPartyData,
                            context_hub::ThirdPartyData>&
  data(const context_hub::AutoTodoEntry& entry) {
    return entry.data;
  }

  static bool Read(browser::context_hub::mojom::AutoTodoItemDataView data,
                   context_hub::AutoTodoEntry* out);
};

template <>
struct StructTraits<browser::context_hub::mojom::DriveFileDataView,
                    personal_context::proto::DriveFile> {
  static GURL url(const personal_context::proto::DriveFile& drive_file) {
    return GURL(drive_file.url());
  }

  static std::string_view name(
      const personal_context::proto::DriveFile& drive_file) {
    return drive_file.name();
  }

  static bool Read(browser::context_hub::mojom::DriveFileDataView data,
                   personal_context::proto::DriveFile* out);
};

template <>
struct UnionTraits<browser::context_hub::mojom::SourceReferenceDataView,
                   personal_context::proto::SourceReference> {
  static browser::context_hub::mojom::SourceReferenceDataView::Tag GetTag(
      const personal_context::proto::SourceReference& ref);

  static context_hub::SourceReference gmail(
      const personal_context::proto::SourceReference& ref);

  static browser::context_hub::mojom::PhotosReferencePtr photos(
      const personal_context::proto::SourceReference& ref);

  static const personal_context::proto::DriveFile& drive(
      const personal_context::proto::SourceReference& ref) {
    return ref.drive();
  }

  static bool Read(browser::context_hub::mojom::SourceReferenceDataView data,
                   personal_context::proto::SourceReference* out);
};

template <>
struct StructTraits<browser::context_hub::mojom::SmartSearchResultDataView,
                    personal_context::proto::SmartSearchItem> {
  static std::string_view description(
      const personal_context::proto::SmartSearchItem& item) {
    return item.description();
  }

  static const google::protobuf::RepeatedPtrField<
      personal_context::proto::SourceReference>&
  source_references(const personal_context::proto::SmartSearchItem& item) {
    return item.source_references();
  }

  static bool Read(
      browser::context_hub::mojom::SmartSearchResultDataView data,
      personal_context::proto::SmartSearchItem* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_MOJOM_TRAITS_H_
