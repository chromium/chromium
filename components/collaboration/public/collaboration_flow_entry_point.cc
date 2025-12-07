// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/collaboration_flow_entry_point.h"

namespace collaboration {

CollaborationServiceJoinEntryPoint GetEntryPointFromPageTransition(
    ui::PageTransition transition_type) {
  switch (ui::PageTransitionStripQualifier(transition_type)) {
    case ui::PageTransition::PAGE_TRANSITION_LINK:
      return CollaborationServiceJoinEntryPoint::kLinkClick;
    case ui::PageTransition::PAGE_TRANSITION_TYPED:
    case ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR:
      return CollaborationServiceJoinEntryPoint::kUserTyped;
    case ui::PageTransition::PAGE_TRANSITION_FROM_API:
      return CollaborationServiceJoinEntryPoint::kExternalApp;
    case ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK:
      return CollaborationServiceJoinEntryPoint::kForwardBackButton;
    case ui::PageTransition::PAGE_TRANSITION_CHAIN_START:
    case ui::PageTransition::PAGE_TRANSITION_CHAIN_END:
    case ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT:
    case ui::PageTransition::PAGE_TRANSITION_SERVER_REDIRECT:
    case ui::PageTransition::PAGE_TRANSITION_IS_REDIRECT_MASK:
      return CollaborationServiceJoinEntryPoint::kRedirect;
    default:
      return CollaborationServiceJoinEntryPoint::kUnknown;
  }
}

}  // namespace collaboration
