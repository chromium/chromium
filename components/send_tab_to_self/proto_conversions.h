// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_PROTO_CONVERSIONS_H_
#define COMPONENTS_SEND_TAB_TO_SELF_PROTO_CONVERSIONS_H_

namespace sync_pb {
class PageContext;
}  // namespace sync_pb

namespace send_tab_to_self {

struct PageContext;

sync_pb::PageContext PageContextToProto(const PageContext& context);
PageContext PageContextFromProto(const sync_pb::PageContext& pb_page_context);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_PROTO_CONVERSIONS_H_
