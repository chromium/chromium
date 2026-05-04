// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_PROTO_CONVERSIONS_H_
#define COMPONENTS_SEND_TAB_TO_SELF_PROTO_CONVERSIONS_H_

#include <optional>

#include "components/autofill/core/browser/field_types.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"

namespace send_tab_to_self {

struct PageContext;

sync_pb::PageContext PageContextToProto(const PageContext& context);
PageContext PageContextFromProto(const sync_pb::PageContext& pb_page_context);

std::optional<sync_pb::FormField_AutofillFieldType> AutofillFieldTypeToProto(
    autofill::FieldType type);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_PROTO_CONVERSIONS_H_
