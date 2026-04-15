// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_RECORD_REPLAY_DRIVER_H_
#define COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_RECORD_REPLAY_DRIVER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/record_replay/core/browser/record_replay_driver.h"
#include "components/record_replay/core/common/aliases.h"
#include "components/record_replay/core/common/record_replay.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class RenderFrameHost;
}

namespace record_replay {

class RecordReplayClient;

// The browser-side endpoint for Mojo communication with the renderer
// implementation of the interface (`RecordReplayAgent`).
//
// Owned by `RecordReplayDriverFactory` (1 per frame). Tied to a
// `RenderFrameHost`. Created when a `RenderFrameHost` is created and
// destroyed when it is deleted. It runs exclusively on the UI thread.
class ContentRecordReplayDriver : public RecordReplayDriver,
                                  public mojom::RecordReplayDriver {
 public:
  ContentRecordReplayDriver(content::RenderFrameHost* render_frame_host,
                            RecordReplayClient& client);
  ContentRecordReplayDriver(const ContentRecordReplayDriver&) = delete;
  ContentRecordReplayDriver& operator=(const ContentRecordReplayDriver&) =
      delete;
  ~ContentRecordReplayDriver() override;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::RecordReplayDriver>
          pending_receiver);

  // RecordReplayDriver:
  bool IsActive() const override;
  base::UnguessableToken GetFrameToken() const override;
  void StartRecording() override;
  void StopRecording() override;
  void GetElementSelector(DomNodeId dom_node_id,
                          base::OnceCallback<void(Selector)> cb) override;
  void GetMatchingElements(
      Selector element_selector,
      base::OnceCallback<void(std::vector<std::unique_ptr<ElementId>>)> cb)
      override;
  void DoClick(DomNodeId dom_node_id,
               base::OnceCallback<void(bool)> cb) override;
  void DoPaste(DomNodeId dom_node_id,
               FieldValue text,
               base::OnceCallback<void(bool)> cb) override;
  void DoSelect(DomNodeId dom_node_id,
                FieldValue value,
                base::OnceCallback<void(bool)> cb) override;
  void SetRecordReplayAgentForTesting(mojom::RecordReplayAgent* agent) override;

  // mojom::RecordReplayDriver:
  void OnClick(DomNodeId dom_node_id, Selector element_selector) override;
  void OnSelectChanged(DomNodeId dom_node_id,
                       Selector element_selector,
                       FieldValue text) override;
  void OnTextChange(DomNodeId dom_node_id,
                    Selector element_selector,
                    FieldValue text) override;

 private:
  mojom::RecordReplayAgent* GetRecordReplayAgent();

  const raw_ref<RecordReplayClient> client_;
  const raw_ref<content::RenderFrameHost> rfh_;
  raw_ptr<mojom::RecordReplayAgent> test_record_replay_agent_ = nullptr;
  mojo::AssociatedReceiver<mojom::RecordReplayDriver> receiver_{this};
  mojo::AssociatedRemote<mojom::RecordReplayAgent> agent_;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_RECORD_REPLAY_DRIVER_H_
