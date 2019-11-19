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
  // Fetch the root document node so that Inspector will push DOM node
  // information to the client.
  base::DictionaryValue params;
  return client->SendCommand("DOM.getDocument", params);
}

Status DomTracker::OnEvent(DevToolsClient* client,
                           const std::string& method,
                           const base::DictionaryValue& params) {
  if (method == "DOM.setChildNodes") {
    const base::Value* nodes;
    if (!params.Get("nodes", &nodes))
      return Status(kUnknownError, "DOM.setChildNodes missing 'nodes'");

    if (!ProcessNodeList(nodes)) {
      std::string json;
      base::JSONWriter::Write(*nodes, &json);
      return Status(kUnknownError,
                    "DOM.setChildNodes has invalid 'nodes': " + json);
    }
  } else if (method == "DOM.childNodeInserted") {
    const base::Value* node;
    if (!params.Get("node", &node))
      return Status(kUnknownError, "DOM.childNodeInserted missing 'node'");

    if (!ProcessNode(node)) {
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

bool DomTracker::ProcessNodeList(const base::Value* nodes) {
  const base::ListValue* nodes_list;
  if (!nodes->GetAsList(&nodes_list))
    return false;
  for (size_t i = 0; i < nodes_list->GetSize(); ++i) {
    const base::Value* node;
    if (!nodes_list->Get(i, &node))
      return false;
    if (!ProcessNode(node))
      return false;
  }
  return true;
}

bool DomTracker::ProcessNode(const base::Value* node) {
  const base::DictionaryValue* dict;
  if (!node->GetAsDictionary(&dict))
    return false;
  int node_id;
  if (!dict->GetInteger("nodeId", &node_id))
    return false;
  std::string frame_id;
  if (dict->GetString("frameId", &frame_id))
    node_to_frame_map_.insert(std::make_pair(node_id, frame_id));

  const base::Value* children;
  if (dict->Get("children", &children))
    return ProcessNodeList(children);
  return true;
}
