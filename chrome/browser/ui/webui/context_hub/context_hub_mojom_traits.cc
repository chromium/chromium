// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_mojom_traits.h"

#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/notreached.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
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
                  context_hub::SourceReference>::
    Read(browser::context_hub::mojom::GmailReferenceDataView data,
         context_hub::SourceReference* out) {
  return data.ReadMessageUrl(&out->url) && data.ReadSubject(&out->subject);
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
    case browser::context_hub::mojom::SourceReferenceDataView::Tag::kGmail:
      return data.ReadGmail(out);
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
    case personal_context::proto::SourceReference::SOURCE_REFERENCE_NOT_SET:
      NOTREACHED();
  }
}

// static
context_hub::SourceReference UnionTraits<
    browser::context_hub::mojom::SourceReferenceDataView,
    personal_context::proto::SourceReference>::
    gmail(const personal_context::proto::SourceReference& ref) {
  return context_hub::SourceReference{
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
      context_hub::SourceReference gmail_ref;
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

}  // namespace mojo
