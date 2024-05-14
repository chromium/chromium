// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/page_transition_conversion.h"

#include "base/notreached.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "ui/base/page_transition_types.h"

namespace syncer {

sync_pb::SyncEnums_PageTransition ToSyncPageTransition(
    ui::PageTransition transition_type) {
  static_assert(static_cast<int32_t>(ui::PAGE_TRANSITION_LAST_CORE) ==
                    static_cast<int32_t>(ui::PAGE_TRANSITION_KEYWORD_GENERATED),
                "PAGE_TRANSITION_LAST_CORE must equal "
                "PAGE_TRANSITION_KEYWORD_GENERATED");

  switch (ui::PageTransitionStripQualifier(transition_type)) {
    case ui::PAGE_TRANSITION_LINK:
      return sync_pb::SyncEnums_PageTransition_LINK;

    case ui::PAGE_TRANSITION_TYPED:
      return sync_pb::SyncEnums_PageTransition_TYPED;

    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      return sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK;

    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      return sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME;

    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      return sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME;

    case ui::PAGE_TRANSITION_GENERATED:
      return sync_pb::SyncEnums_PageTransition_GENERATED;

    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      return sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL;

    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      return sync_pb::SyncEnums_PageTransition_FORM_SUBMIT;

    case ui::PAGE_TRANSITION_RELOAD:
      return sync_pb::SyncEnums_PageTransition_RELOAD;

    case ui::PAGE_TRANSITION_KEYWORD:
      return sync_pb::SyncEnums_PageTransition_KEYWORD;

    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      return sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED;

    // Non-core values listed here although unreachable:
    case ui::PAGE_TRANSITION_CORE_MASK:
    case ui::PAGE_TRANSITION_BLOCKED:
    case ui::PAGE_TRANSITION_FORWARD_BACK:
    case ui::PAGE_TRANSITION_FROM_ADDRESS_BAR:
    case ui::PAGE_TRANSITION_HOME_PAGE:
    case ui::PAGE_TRANSITION_FROM_API:
    case ui::PAGE_TRANSITION_CHAIN_START:
    case ui::PAGE_TRANSITION_CHAIN_END:
    case ui::PAGE_TRANSITION_CLIENT_REDIRECT:
    case ui::PAGE_TRANSITION_SERVER_REDIRECT:
    case ui::PAGE_TRANSITION_IS_REDIRECT_MASK:
    case ui::PAGE_TRANSITION_QUALIFIER_MASK:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return sync_pb::SyncEnums_PageTransition_LINK;
}

ui::PageTransition FromSyncPageTransition(
    sync_pb::SyncEnums_PageTransition transition_type) {
  switch (transition_type) {
    case sync_pb::SyncEnums_PageTransition_LINK:
      return ui::PAGE_TRANSITION_LINK;

    case sync_pb::SyncEnums_PageTransition_TYPED:
      return ui::PAGE_TRANSITION_TYPED;

    case sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK:
      return ui::PAGE_TRANSITION_AUTO_BOOKMARK;

    case sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME:
      return ui::PAGE_TRANSITION_AUTO_SUBFRAME;

    case sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME:
      return ui::PAGE_TRANSITION_MANUAL_SUBFRAME;

    case sync_pb::SyncEnums_PageTransition_GENERATED:
      return ui::PAGE_TRANSITION_GENERATED;

    case sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL:
      return ui::PAGE_TRANSITION_AUTO_TOPLEVEL;

    case sync_pb::SyncEnums_PageTransition_FORM_SUBMIT:
      return ui::PAGE_TRANSITION_FORM_SUBMIT;

    case sync_pb::SyncEnums_PageTransition_RELOAD:
      return ui::PAGE_TRANSITION_RELOAD;

    case sync_pb::SyncEnums_PageTransition_KEYWORD:
      return ui::PAGE_TRANSITION_KEYWORD;

    case sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED:
      return ui::PAGE_TRANSITION_KEYWORD_GENERATED;
  }
  return ui::PAGE_TRANSITION_LINK;
}

}  // namespace syncer
