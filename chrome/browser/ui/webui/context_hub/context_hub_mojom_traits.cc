// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_mojom_traits.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
browser::context_hub::mojom::AutoTodoStatus
EnumTraits<browser::context_hub::mojom::AutoTodoStatus,
           context_hub::AutoTodoEntry::Status>::
    ToMojom(context_hub::AutoTodoEntry::Status input) {
  switch (input) {
    case context_hub::AutoTodoEntry::Status::kActive:
      return browser::context_hub::mojom::AutoTodoStatus::kActive;
    case context_hub::AutoTodoEntry::Status::kCompleted:
      return browser::context_hub::mojom::AutoTodoStatus::kCompleted;
    case context_hub::AutoTodoEntry::Status::kDismissed:
      return browser::context_hub::mojom::AutoTodoStatus::kDismissed;
  }
  NOTREACHED();
}

// static
context_hub::AutoTodoEntry::Status
EnumTraits<browser::context_hub::mojom::AutoTodoStatus,
           context_hub::AutoTodoEntry::Status>::
    FromMojom(browser::context_hub::mojom::AutoTodoStatus input) {
  switch (input) {
    case browser::context_hub::mojom::AutoTodoStatus::kActive:
      return context_hub::AutoTodoEntry::Status::kActive;
    case browser::context_hub::mojom::AutoTodoStatus::kCompleted:
      return context_hub::AutoTodoEntry::Status::kCompleted;
    case browser::context_hub::mojom::AutoTodoStatus::kDismissed:
      return context_hub::AutoTodoEntry::Status::kDismissed;
  }
  NOTREACHED();
}

// static
browser::context_hub::mojom::AutoTodoGroup
EnumTraits<browser::context_hub::mojom::AutoTodoGroup,
           context_hub::ThirdPartyData::GroupType>::
    ToMojom(context_hub::ThirdPartyData::GroupType input) {
  switch (input) {
    case context_hub::ThirdPartyData::GroupType::kNoMatch:
      return browser::context_hub::mojom::AutoTodoGroup::kNoMatch;
    case context_hub::ThirdPartyData::GroupType::kNudgeToClose:
      return browser::context_hub::mojom::AutoTodoGroup::kNudgeToClose;
    case context_hub::ThirdPartyData::GroupType::kReadingList:
      return browser::context_hub::mojom::AutoTodoGroup::kReadingList;
    case context_hub::ThirdPartyData::GroupType::kUnfinishedAction:
      return browser::context_hub::mojom::AutoTodoGroup::kUnfinishedAction;
    case context_hub::ThirdPartyData::GroupType::kShoppingCart:
      return browser::context_hub::mojom::AutoTodoGroup::kShoppingCart;
  }
  NOTREACHED();
}

// static
context_hub::ThirdPartyData::GroupType
EnumTraits<browser::context_hub::mojom::AutoTodoGroup,
           context_hub::ThirdPartyData::GroupType>::
    FromMojom(browser::context_hub::mojom::AutoTodoGroup input) {
  switch (input) {
    case browser::context_hub::mojom::AutoTodoGroup::kNoMatch:
      return context_hub::ThirdPartyData::GroupType::kNoMatch;
    case browser::context_hub::mojom::AutoTodoGroup::kNudgeToClose:
      return context_hub::ThirdPartyData::GroupType::kNudgeToClose;
    case browser::context_hub::mojom::AutoTodoGroup::kReadingList:
      return context_hub::ThirdPartyData::GroupType::kReadingList;
    case browser::context_hub::mojom::AutoTodoGroup::kUnfinishedAction:
      return context_hub::ThirdPartyData::GroupType::kUnfinishedAction;
    case browser::context_hub::mojom::AutoTodoGroup::kShoppingCart:
      return context_hub::ThirdPartyData::GroupType::kShoppingCart;
  }
  NOTREACHED();
}

// static
bool StructTraits<browser::context_hub::mojom::GmailReferenceDataView,
                  context_hub::GmailReference>::
    Read(browser::context_hub::mojom::GmailReferenceDataView data,
         context_hub::GmailReference* out) {
  return data.ReadMessageUrl(&out->url) && data.ReadSubject(&out->subject);
}

// static
bool StructTraits<browser::context_hub::mojom::ChromeHistoryEntryDataView,
                  context_hub::ChromeHistoryEntry>::
    Read(browser::context_hub::mojom::ChromeHistoryEntryDataView data,
         context_hub::ChromeHistoryEntry* out) {
  return data.ReadUrl(&out->url) && data.ReadTitle(&out->title) &&
         data.ReadVisitTimestamp(&out->visit_timestamp);
}

// static
browser::context_hub::mojom::SourceReferenceDataView::Tag UnionTraits<
    browser::context_hub::mojom::SourceReferenceDataView,
    context_hub::SourceReference>::GetTag(const context_hub::SourceReference&
                                              ref) {
  using Tag = browser::context_hub::mojom::SourceReferenceDataView::Tag;
  return std::visit(
      absl::Overload{
          [](const context_hub::GmailReference&) { return Tag::kGmail; },
          [](const context_hub::ChromeHistoryEntry&) {
            return Tag::kChromeHistoryEntry;
          },
      },
      ref);
}

// static
browser::context_hub::mojom::PhotosReferencePtr UnionTraits<
    browser::context_hub::mojom::SourceReferenceDataView,
    context_hub::SourceReference>::photos(const context_hub::SourceReference&
                                              ref) {
  NOTREACHED();
}

// static
browser::context_hub::mojom::DriveFilePtr UnionTraits<
    browser::context_hub::mojom::SourceReferenceDataView,
    context_hub::SourceReference>::drive(const context_hub::SourceReference&
                                             ref) {
  NOTREACHED();
}

// static
bool UnionTraits<browser::context_hub::mojom::SourceReferenceDataView,
                 context_hub::SourceReference>::
    Read(browser::context_hub::mojom::SourceReferenceDataView data,
         context_hub::SourceReference* out) {
  switch (data.tag()) {
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::kGmail: {
      context_hub::GmailReference gmail;
      if (!data.ReadGmail(&gmail)) {
        return false;
      }
      *out = std::move(gmail);
      return true;
    }
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::
        kChromeHistoryEntry: {
      context_hub::ChromeHistoryEntry history_entry;
      if (!data.ReadChromeHistoryEntry(&history_entry)) {
        return false;
      }
      *out = std::move(history_entry);
      return true;
    }
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::kPhotos:
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::kDrive:
      return false;
  }
  return false;
}

// static
bool StructTraits<browser::context_hub::mojom::FirstPartyDataDataView,
                  context_hub::FirstPartyData>::
    Read(browser::context_hub::mojom::FirstPartyDataDataView data,
         context_hub::FirstPartyData* out) {
  return data.ReadActionableUrl(&out->actionable_url) &&
         data.ReadSourceReferences(&out->source_references);
}

// static
bool StructTraits<browser::context_hub::mojom::ThirdPartyDataDataView,
                  context_hub::ThirdPartyData>::
    Read(browser::context_hub::mojom::ThirdPartyDataDataView data,
         context_hub::ThirdPartyData* out) {
  out->tab_id = data.tab_id();
  return data.ReadLastActiveTimestamp(&out->last_active_timestamp) &&
         data.ReadGroupType(&out->group_type);
}

// static
bool UnionTraits<
    browser::context_hub::mojom::AutoTodoDataDataView,
    std::variant<context_hub::FirstPartyData, context_hub::ThirdPartyData>>::
    Read(browser::context_hub::mojom::AutoTodoDataDataView data,
         std::variant<context_hub::FirstPartyData, context_hub::ThirdPartyData>*
             out) {
  switch (data.tag()) {
    case browser::context_hub::mojom::AutoTodoDataDataView::Tag::kFirstParty: {
      context_hub::FirstPartyData first_party;
      if (!data.ReadFirstParty(&first_party)) {
        return false;
      }
      *out = std::move(first_party);
      return true;
    }
    case browser::context_hub::mojom::AutoTodoDataDataView::Tag::kThirdParty: {
      context_hub::ThirdPartyData third_party;
      if (!data.ReadThirdParty(&third_party)) {
        return false;
      }
      *out = std::move(third_party);
      return true;
    }
  }
  return false;
}

// static
bool StructTraits<browser::context_hub::mojom::AutoTodoItemDataView,
                  context_hub::AutoTodoEntry>::
    Read(browser::context_hub::mojom::AutoTodoItemDataView data,
         context_hub::AutoTodoEntry* out) {
  if (!data.ReadId(&out->id) || !data.ReadTitle(&out->title) ||
      !data.ReadDescription(&out->description) ||
      !data.ReadStatus(&out->status) || !data.ReadData(&out->data)) {
    return false;
  }
  out->importance_score = data.score();
  return true;
}

// static
bool StructTraits<
    browser::context_hub::mojom::AutoTodosGenerationMetadataDataView,
    context_hub::AutoTodosGenerationMetadata>::
    Read(browser::context_hub::mojom::AutoTodosGenerationMetadataDataView data,
         context_hub::AutoTodosGenerationMetadata* out) {
  out->has_error = data.has_error();
  return data.ReadLastGenerationTime(&out->last_generation_time);
}

// static
browser::context_hub::mojom::EntryType EnumTraits<
    browser::context_hub::mojom::EntryType,
    context_hub::MemoryBankType>::ToMojom(context_hub::MemoryBankType input) {
  switch (input) {
    case context_hub::MemoryBankType::kTab:
      return browser::context_hub::mojom::EntryType::kTab;
    case context_hub::MemoryBankType::kTextSelection:
      return browser::context_hub::mojom::EntryType::kTextSelection;
    case context_hub::MemoryBankType::kImage:
      return browser::context_hub::mojom::EntryType::kImage;
  }
  NOTREACHED();
}

// static
context_hub::MemoryBankType EnumTraits<browser::context_hub::mojom::EntryType,
                                       context_hub::MemoryBankType>::
    FromMojom(browser::context_hub::mojom::EntryType input) {
  switch (input) {
    case browser::context_hub::mojom::EntryType::kTab:
      return context_hub::MemoryBankType::kTab;
    case browser::context_hub::mojom::EntryType::kTextSelection:
      return context_hub::MemoryBankType::kTextSelection;
    case browser::context_hub::mojom::EntryType::kImage:
      return context_hub::MemoryBankType::kImage;
  }
  NOTREACHED();
}

// static
bool StructTraits<browser::context_hub::mojom::MemoryBankEntryDataView,
                  context_hub::MemoryBankEntry>::
    Read(browser::context_hub::mojom::MemoryBankEntryDataView data,
         context_hub::MemoryBankEntry* out) {
  out->id = data.id();
  return data.ReadType(&out->type) && data.ReadTimestamp(&out->timestamp) &&
         data.ReadUrl(&out->url) && data.ReadTabTitle(&out->tab_title) &&
         data.ReadSelectedText(&out->selected_text) &&
         data.ReadImageUrl(&out->image_url) && data.ReadTags(&out->tags) &&
         data.ReadNote(&out->note) && data.ReadCollection(&out->collection);
}

// static
browser::context_hub::mojom::SourceReferenceDataView::Tag
UnionTraits<browser::context_hub::mojom::SourceReferenceDataView,
            personal_context::proto::SourceReference>::
    GetTag(const personal_context::proto::SourceReference& ref) {
  switch (ref.source_reference_case()) {
    case personal_context::proto::SourceReference::kDrive:
      return browser::context_hub::mojom::SourceReferenceDataView::Tag::kDrive;
    case personal_context::proto::SourceReference::kGmail:
      return browser::context_hub::mojom::SourceReferenceDataView::Tag::kGmail;
    case personal_context::proto::SourceReference::kPhotos:
      return browser::context_hub::mojom::SourceReferenceDataView::Tag::kPhotos;
    case personal_context::proto::SourceReference::kChromeHistoryEntry:
      return browser::context_hub::mojom::SourceReferenceDataView::Tag::
          kChromeHistoryEntry;
    case personal_context::proto::SourceReference::SOURCE_REFERENCE_NOT_SET:
      NOTREACHED();
  }
}

// static
context_hub::GmailReference
UnionTraits<browser::context_hub::mojom::SourceReferenceDataView,
            personal_context::proto::SourceReference>::
    gmail(const personal_context::proto::SourceReference& ref) {
  return context_hub::GmailReference{
      .url = GURL(ref.gmail().message_url()),
      .subject = std::string(ref.gmail().subject()),
  };
}

// static
browser::context_hub::mojom::PhotosReferencePtr UnionTraits<
    browser::context_hub::mojom::SourceReferenceDataView,
    personal_context::proto::SourceReference>::
    photos(const personal_context::proto::SourceReference& ref) {
  auto photos = browser::context_hub::mojom::PhotosReference::New();
  photos->photos_url = GURL(ref.photos().photos_url());
  return photos;
}

// static
context_hub::ChromeHistoryEntry
UnionTraits<browser::context_hub::mojom::SourceReferenceDataView,
            personal_context::proto::SourceReference>::
    chrome_history_entry(const personal_context::proto::SourceReference& ref) {
  const personal_context::proto::ChromeHistoryEntry& entry =
      ref.chrome_history_entry();
  base::Time visit_timestamp;
  if (entry.has_visit_timestamp()) {
    visit_timestamp = base::Time::FromSecondsSinceUnixEpoch(
                          entry.visit_timestamp().seconds()) +
                      base::Nanoseconds(entry.visit_timestamp().nanos());
  }
  return context_hub::ChromeHistoryEntry{
      .url = GURL(entry.url()),
      .title = std::string(entry.title()),
      .visit_timestamp = visit_timestamp,
  };
}

// static
bool StructTraits<browser::context_hub::mojom::DriveFileDataView,
                  personal_context::proto::DriveFile>::
    Read(browser::context_hub::mojom::DriveFileDataView data,
         personal_context::proto::DriveFile* out) {
  GURL url;
  if (!data.ReadUrl(&url)) {
    return false;
  }
  out->set_url(url.spec());

  std::string_view name;
  if (!data.ReadName(&name)) {
    return false;
  }
  out->set_name(name);
  return true;
}

// static
bool UnionTraits<browser::context_hub::mojom::SourceReferenceDataView,
                 personal_context::proto::SourceReference>::
    Read(browser::context_hub::mojom::SourceReferenceDataView data,
         personal_context::proto::SourceReference* out) {
  switch (data.tag()) {
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::kDrive:
      return data.ReadDrive(out->mutable_drive());
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::kGmail: {
      context_hub::GmailReference gmail_ref;
      if (!data.ReadGmail(&gmail_ref)) {
        return false;
      }
      out->mutable_gmail()->set_message_url(gmail_ref.url.spec());
      out->mutable_gmail()->set_subject(gmail_ref.subject);
      return true;
    }
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::kPhotos: {
      browser::context_hub::mojom::PhotosReferencePtr photos_ref;
      if (!data.ReadPhotos(&photos_ref)) {
        return false;
      }
      out->mutable_photos()->set_photos_url(photos_ref->photos_url.spec());
      return true;
    }
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::
        kChromeHistoryEntry: {
      context_hub::ChromeHistoryEntry history_entry;
      if (!data.ReadChromeHistoryEntry(&history_entry)) {
        return false;
      }
      personal_context::proto::ChromeHistoryEntry* history_proto =
          out->mutable_chrome_history_entry();
      history_proto->set_url(history_entry.url.spec());
      history_proto->set_title(history_entry.title);
      if (history_entry.visit_timestamp >= base::Time::UnixEpoch() &&
          !history_entry.visit_timestamp.is_inf()) {
        const base::TimeDelta since_epoch =
            history_entry.visit_timestamp - base::Time::UnixEpoch();
        personal_context::proto::Timestamp* timestamp =
            history_proto->mutable_visit_timestamp();
        timestamp->set_seconds(since_epoch.InSeconds());
        timestamp->set_nanos(static_cast<int32_t>(
            (since_epoch % base::Seconds(1)).InNanoseconds()));
      }
      return true;
    }
  }
  return false;
}

// static
bool StructTraits<browser::context_hub::mojom::SmartSearchResultDataView,
                  personal_context::proto::SmartSearchItem>::
    Read(browser::context_hub::mojom::SmartSearchResultDataView data,
         personal_context::proto::SmartSearchItem* out) {
  std::string_view description;
  if (!data.ReadDescription(&description)) {
    return false;
  }
  out->set_description(description);
  return data.ReadSourceReferences(out->mutable_source_references());
}

// static
bool StructTraits<browser::context_hub::mojom::ChatReferenceDataView,
                  context_hub::MemoryBankChatCitation>::
    Read(browser::context_hub::mojom::ChatReferenceDataView data,
         context_hub::MemoryBankChatCitation* out) {
  std::string title;
  GURL url;
  if (!data.ReadTitle(&title) || !data.ReadUrl(&url)) {
    return false;
  }
  out->title = std::move(title);
  out->url = std::move(url);
  return true;
}

// static
browser::context_hub::mojom::ChatRole
StructTraits<browser::context_hub::mojom::MemoryBankChatMessageDataView,
             context_hub::MemoryBankChatHistoryTurn>::
    role(const context_hub::MemoryBankChatHistoryTurn& entry) {
  if (entry.turn.role() ==
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER) {
    return browser::context_hub::mojom::ChatRole::kUser;
  }
  CHECK_EQ(entry.turn.role(),
           optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT);
  return browser::context_hub::mojom::ChatRole::kAssistant;
}

// static
bool StructTraits<browser::context_hub::mojom::MemoryBankChatMessageDataView,
                  context_hub::MemoryBankChatHistoryTurn>::
    Read(browser::context_hub::mojom::MemoryBankChatMessageDataView data,
         context_hub::MemoryBankChatHistoryTurn* out) {
  browser::context_hub::mojom::ChatRole role;
  std::string_view content;
  std::vector<context_hub::MemoryBankChatCitation> citations;
  if (!data.ReadRole(&role) || !data.ReadContent(&content) ||
      !data.ReadReferences(&citations)) {
    return false;
  }
  switch (role) {
    case browser::context_hub::mojom::ChatRole::kUser:
      out->turn.set_role(optimization_guide::proto::ChatHistoryTurn::ROLE_USER);
      break;
    case browser::context_hub::mojom::ChatRole::kAssistant:
      out->turn.set_role(
          optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT);
      break;
  }
  out->turn.set_message_content(content);
  out->citations = std::move(citations);
  return true;
}

}  // namespace mojo
