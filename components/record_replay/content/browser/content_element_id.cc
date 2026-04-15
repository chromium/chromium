// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/content/browser/content_element_id.h"

#include <sstream>

namespace record_replay {

ContentElementId::ContentElementId(base::UnguessableToken frame_token,
                                   DomNodeId dom_node_id)
    : ElementId(dom_node_id), frame_token_(frame_token) {}

ContentElementId::ContentElementId(const ContentElementId&) = default;

ContentElementId& ContentElementId::operator=(const ContentElementId&) =
    default;

ContentElementId::~ContentElementId() = default;

std::string ContentElementId::ToString() const {
  std::ostringstream ss;
  ss << frame_token_ << "_" << dom_node_id();
  return ss.str();
}

}  // namespace record_replay
