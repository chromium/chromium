// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/content/browser/content_record_replay_driver.h"

#include "base/functional/callback.h"
#include "components/record_replay/core/browser/record_replay_client.h"
#include "components/record_replay/core/browser/record_replay_manager.h"
#include "components/record_replay/core/common/aliases.h"
#include "components/record_replay/core/common/element_id.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace record_replay {

ContentRecordReplayDriver::ContentRecordReplayDriver(
    content::RenderFrameHost* render_frame_host,
    RecordReplayClient& client)
    : client_(client), rfh_(*render_frame_host) {}

ContentRecordReplayDriver::~ContentRecordReplayDriver() = default;

void ContentRecordReplayDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::RecordReplayDriver>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

mojom::RecordReplayAgent* ContentRecordReplayDriver::GetRecordReplayAgent() {
  if (test_record_replay_agent_) {
    return test_record_replay_agent_;
  }
  if (!agent_) {
    rfh_->GetRemoteAssociatedInterfaces()->GetInterface(&agent_);
  }
  return agent_.get();
}

bool ContentRecordReplayDriver::IsActive() const {
  return rfh_->IsActive();
}

base::UnguessableToken ContentRecordReplayDriver::GetFrameToken() const {
  return rfh_->GetFrameToken().value();
}

void ContentRecordReplayDriver::StartRecording() {
  GetRecordReplayAgent()->StartRecording();
}

void ContentRecordReplayDriver::StopRecording() {
  GetRecordReplayAgent()->StopRecording();
}

void ContentRecordReplayDriver::GetElementSelector(
    DomNodeId dom_node_id,
    base::OnceCallback<void(Selector)> cb) {
  GetRecordReplayAgent()->GetElementSelector(dom_node_id, std::move(cb));
}

void ContentRecordReplayDriver::GetMatchingElements(
    Selector element_selector,
    base::OnceCallback<void(std::vector<ElementId>)> cb) {
  GetRecordReplayAgent()->GetMatchingElements(
      std::move(element_selector),
      base::BindOnce(
          [](base::UnguessableToken frame_token,
             base::OnceCallback<void(std::vector<ElementId>)> cb,
             const std::vector<DomNodeId>& dom_node_ids) {
            std::vector<ElementId> elements;
            elements.reserve(dom_node_ids.size());
            for (DomNodeId dom_node_id : dom_node_ids) {
              elements.emplace_back(frame_token, dom_node_id);
            }
            std::move(cb).Run(std::move(elements));
          },
          GetFrameToken(), std::move(cb)));
}

void ContentRecordReplayDriver::DoClick(DomNodeId dom_node_id,
                                        base::OnceCallback<void(bool)> cb) {
  GetRecordReplayAgent()->DoClick(dom_node_id, std::move(cb));
}

void ContentRecordReplayDriver::DoPaste(DomNodeId dom_node_id,
                                        FieldValue text,
                                        base::OnceCallback<void(bool)> cb) {
  GetRecordReplayAgent()->DoPaste(dom_node_id, std::move(text), std::move(cb));
}

void ContentRecordReplayDriver::DoSelect(DomNodeId dom_node_id,
                                         FieldValue value,
                                         base::OnceCallback<void(bool)> cb) {
  GetRecordReplayAgent()->DoSelect(dom_node_id, std::move(value),
                                   std::move(cb));
}

void ContentRecordReplayDriver::SetRecordReplayAgentForTesting(
    mojom::RecordReplayAgent* agent) {
  test_record_replay_agent_ = agent;
}

void ContentRecordReplayDriver::OnClick(DomNodeId dom_node_id,
                                        Selector element_selector) {
  client_->GetManager().OnClick(*this, ElementId{GetFrameToken(), dom_node_id},
                                std::move(element_selector), GetPassKey());
}

void ContentRecordReplayDriver::OnSelectChanged(DomNodeId dom_node_id,
                                                Selector element_selector,
                                                FieldValue value) {
  client_->GetManager().OnSelectChanged(
      *this, ElementId{GetFrameToken(), dom_node_id},
      std::move(element_selector), std::move(value), GetPassKey());
}

void ContentRecordReplayDriver::OnTextChange(DomNodeId dom_node_id,
                                             Selector element_selector,
                                             FieldValue text) {
  client_->GetManager().OnTextChange(
      *this, ElementId{GetFrameToken(), dom_node_id},
      std::move(element_selector), std::move(text), GetPassKey());
}

}  // namespace record_replay
