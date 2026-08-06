// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fusebox_action_mojo_utils.h"

#include "base/notreached.h"
#include "components/omnibox/browser/searchbox.mojom.h"

namespace fusebox_action {

mojom::FuseboxActionPtr SyncFuseboxActionProtoToMojo(
    const omnibox::SuggestTemplateInfo::FuseboxAction& proto) {
  auto mojo_action = mojom::FuseboxAction::New();

  if (proto.has_preselected_tool()) {
    mojo_action->preselected_tool = proto.preselected_tool();
  }
  if (proto.has_preferred_inventory()) {
    mojo_action->preferred_inventory = proto.preferred_inventory();
  }
  if (proto.has_query_action_override()) {
    auto maybe_valid_query_action_override =
        static_cast<mojom::QueryActionOverride>(proto.query_action_override());
    if (mojom::IsKnownEnumValue(maybe_valid_query_action_override)) {
      mojo_action->query_action_override = maybe_valid_query_action_override;
    } else {
      NOTREACHED() << "Unknown QueryActionOverride "
                    << proto.query_action_override();
    }
  }

  return mojo_action;
}

}  // namespace fusebox_action
