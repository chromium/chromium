// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RECORD_REPLAY_RECORD_REPLAY_AGENT_H_
#define CHROME_RENDERER_RECORD_REPLAY_RECORD_REPLAY_AGENT_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/common/record_replay/record_replay.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_record_replay_client.h"

namespace blink {
class WebDocument;
class WebFormControlElement;
class WebInputElement;
class WebNode;
}  // namespace blink

namespace content {
class RenderFrame;
}

namespace record_replay {

class RecordReplayAgent : public content::RenderFrameObserver,
                          public blink::WebRecordReplayClient,
                          public mojom::RecordReplayAgent {
 public:
  RecordReplayAgent(content::RenderFrame* render_frame,
                    blink::AssociatedInterfaceRegistry* registry);
  RecordReplayAgent(const RecordReplayAgent&) = delete;
  RecordReplayAgent& operator=(const RecordReplayAgent&) = delete;
  ~RecordReplayAgent() override;

  // mojom::RecordReplayAgent:
  void StartRecording() override;
  void StopRecording() override;
  void GetElementSelector(
      int64_t dom_node_id,
      base::OnceCallback<void(const std::string&)> cb) override;
  void GetMatchingElements(
      const std::string& element_selector,
      base::OnceCallback<void(const std::vector<int64_t>&)> cb) override;
  void DoClick(int64_t dom_node_id, base::OnceCallback<void(bool)> cb) override;
  void DoPaste(int64_t dom_node_id,
               const std::string& text,
               base::OnceCallback<void(bool)> cb) override;
  void DoSelect(int64_t dom_node_id,
                const std::string& value,
                base::OnceCallback<void(bool)> cb) override;

 private:
  friend class RecordReplayAgentTestApi;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::RecordReplayAgent>
          pending_receiver);

  mojo::AssociatedRemote<mojom::RecordReplayDriver>& GetDriver();
  blink::WebDocument GetDocument();

  // content::RenderFrameObserver:
  void WillDetach(blink::DetachReason detach_reason) override;
  void OnDestruct() override;

  // blink::WebRecordReplayClient:
  void DidReceiveLeftMouseDownOrGestureTapInNode(
      const blink::WebNode& node) override;
  void SelectControlSelectionChanged(
      const blink::WebFormControlElement& element) override;
  void TextFieldDidEndEditing(const blink::WebInputElement& element) override;

  mojo::AssociatedReceiver<mojom::RecordReplayAgent> receiver_{this};
  mojo::AssociatedRemote<mojom::RecordReplayDriver> driver_;

  // Indicates whether the agent is currently recording actions.
  bool record_ = false;
};

}  // namespace record_replay

#endif  // CHROME_RENDERER_RECORD_REPLAY_RECORD_REPLAY_AGENT_H_
