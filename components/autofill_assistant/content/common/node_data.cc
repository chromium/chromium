// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/common/node_data.h"

namespace autofill_assistant {

NodeData::NodeData() = default;

NodeData::NodeData(const NodeData&) = default;

NodeData& NodeData::operator=(const NodeData&) = default;

NodeData::NodeData(NodeData&&) = default;

NodeData& NodeData::operator=(NodeData&&) = default;

NodeData::~NodeData() = default;

}  // namespace autofill_assistant
