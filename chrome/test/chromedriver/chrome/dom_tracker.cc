// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/dom_tracker.h"

#include <stddef.h>

#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

DomTracker::DomTracker(DevToolsClient* client) {
  client->AddListener(this);
}

DomTracker::~DomTracker() {}

Status DomTracker::GetFrameIdForNode(
    int node_id, std::string* frame_id) {
  if (node_to_frame_map_.count(node_id) == 0)
    return Status(kNoSuchFrame, "element is not a frame");
  *frame_id = node_to_frame_map_[node_id];
  return Status(kOk);
}

Status DomTracker::OnConnected(DevToolsClient* client) {
  node_to_frame_map_.clear();
  // Fetch the root document and traverse it populating node_to_frame_map_.
  // The map will be updated later whenever Inspector pushes DOM node information to the client.
  base::DictionaryValue params;
  params.SetInteger("depth", -1);
  std::unique_ptr<base::DictionaryValue> result;
  auto status =
      client->SendCommandAndGetResult("DOM.getDocument", params, &result);
  if (status.IsError()) {
    return status;
  }

  if (const base::Value* root = result->FindKey("root")) {
    ProcessNode(*root);
  } else {
    status =
        Status(kUnknownError, "DOM.getDocument missing 'root' in the response");
  }

  return status;
}

Status DomTracker::OnEvent(DevToolsClient* client,
                           const std::string& method,
                           const base::DictionaryValue& params) {
  if (method == "DOM.setChildNodes") {
    const base::Value* nodes = params.FindKey("nodes");
    if (nodes == nullptr)
      return Status(kUnknownError, "DOM.setChildNodes missing 'nodes'");

    if (!ProcessNodeList(*nodes)) {
      std::string json;
      base::JSONWriter::Write(*nodes, &json);
      return Status(kUnknownError,
                    "DOM.setChildNodes has invalid 'nodes': " + json);
    }
  } else if (method == "DOM.childNodeInserted") {
    const base::Value* node = params.FindKey("node");
    if (node == nullptr)
      return Status(kUnknownError, "DOM.childNodeInserted missing 'node'");

    if (!ProcessNode(*node)) {
      std::string json;
      base::JSONWriter::Write(*node, &json);
      return Status(kUnknownError,
                    "DOM.childNodeInserted has invalid 'node': " + json);
    }
  } else if (method == "DOM.documentUpdated") {
    node_to_frame_map_.clear();
    // Calling DOM.getDocument is necessary to receive future DOM update events.
    client->SendCommandAndIgnoreResponse("DOM.getDocument", {});
  }
  return Status(kOk);
}

bool DomTracker::ProcessNodeList(const base::Value& nodes) {
  if (!nodes.is_list())
    return false;
  for (const base::Value& node : nodes.GetList()) {
    if (!ProcessNode(node))
      return false;
  }
  return true;
}

bool DomTracker::ProcessNode(const base::Value& node) {
  const base::DictionaryValue* dict;
  if (!node.GetAsDictionary(&dict))
    return false;
  int node_id;
  if (!dict->GetInteger("nodeId", &node_id))
    return false;
  std::string frame_id;
  if (dict->GetString("frameId", &frame_id))
    node_to_frame_map_.insert(std::make_pair(node_id, frame_id));

  if (const base::Value* children = dict->FindKey("children"))
    return ProcessNodeList(*children);
  return true;
}
