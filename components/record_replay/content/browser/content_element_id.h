// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_ELEMENT_ID_H_
#define COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_ELEMENT_ID_H_

#include <string>

#include "base/unguessable_token.h"
#include "components/record_replay/core/common/element_id.h"

namespace record_replay {

// Extends ElementId with a base::UnguessableToken for use in content-specific
// code.
class ContentElementId : public ElementId {
 public:
  ContentElementId(base::UnguessableToken frame_token, DomNodeId dom_node_id);
  ContentElementId(const ContentElementId&);
  ContentElementId& operator=(const ContentElementId&);
  ~ContentElementId() override;

  base::UnguessableToken frame_token() const { return frame_token_; }

  std::string ToString() const override;

 private:
  base::UnguessableToken frame_token_;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_ELEMENT_ID_H_
