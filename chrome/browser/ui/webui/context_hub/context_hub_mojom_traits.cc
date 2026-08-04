// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_mojom_traits.h"

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
std::vector<browser::context_hub::mojom::SourceReferencePtr>
StructTraits<browser::context_hub::mojom::FirstPartyDataDataView,
             context_hub::FirstPartyData>::
    source_references(const context_hub::FirstPartyData& first_party) {
  std::vector<browser::context_hub::mojom::SourceReferencePtr> mojo_refs;
  mojo_refs.reserve(first_party.source_references.size());
  for (const GURL& url : first_party.source_references) {
    auto gmail = browser::context_hub::mojom::GmailReference::New();
    gmail->message_url = url;
    mojo_refs.push_back(browser::context_hub::mojom::SourceReference::NewGmail(
        std::move(gmail)));
  }
  return mojo_refs;
}

// static
bool StructTraits<browser::context_hub::mojom::FirstPartyDataDataView,
                  context_hub::FirstPartyData>::
    Read(browser::context_hub::mojom::FirstPartyDataDataView data,
         context_hub::FirstPartyData* out) {
  if (!data.ReadActionableUrl(&out->actionable_url)) {
    return false;
  }
  std::vector<browser::context_hub::mojom::SourceReferencePtr> refs;
  if (!data.ReadSourceReferences(&refs)) {
    return false;
  }
  out->source_references.clear();
  out->source_references.reserve(refs.size());
  for (const auto& ref : refs) {
    if (ref && ref->is_gmail()) {
      out->source_references.push_back(ref->get_gmail()->message_url);
    }
  }
  return true;
}

// static
bool StructTraits<browser::context_hub::mojom::ThirdPartyDataDataView,
                  context_hub::ThirdPartyData>::
    Read(browser::context_hub::mojom::ThirdPartyDataDataView data,
         context_hub::ThirdPartyData* out) {
  out->tab_id = data.tab_id();
  return data.ReadLastActiveTimestamp(&out->last_active_timestamp);
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

}  // namespace mojo
