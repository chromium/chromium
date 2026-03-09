// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_INDIGO_INDIGO_AGENT_H_
#define CHROME_RENDERER_INDIGO_INDIGO_AGENT_H_

#include <string>

#include "chrome/common/indigo/indigo.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace blink {
class AssociatedInterfaceRegistry;
}

namespace indigo {

// Responsible for acting on the browser's behalf to inject the Indigo content
// script into the page and provide the functionality it needs.
//
// This class manages its own lifetime, deleting itself when the RenderFrame it
// observes is destroyed. It can only observe a single RenderFrame.
class IndigoAgent : public content::RenderFrameObserver,
                    public chrome::mojom::IndigoAgent {
 public:
  static void MaybeCreate(content::RenderFrame* render_frame,
                          blink::AssociatedInterfaceRegistry* registry);

  explicit IndigoAgent(content::RenderFrame* render_frame,
                       blink::AssociatedInterfaceRegistry* registry);
  IndigoAgent(const IndigoAgent&) = delete;
  IndigoAgent& operator=(const IndigoAgent&) = delete;
  ~IndigoAgent() override;

  // chrome::mojom::IndigoAgent:
  void InjectScript(const std::string& script_content,
                    const GURL& script_url,
                    const url::Origin& origin,
                    base::OnceClosure done) override;

 private:
  // content::RenderFrameObserver:
  void OnDestruct() override;

  void BindReceiver(mojo::PendingAssociatedReceiver<chrome::mojom::IndigoAgent>
                        pending_receiver);

  mojo::AssociatedReceiverSet<chrome::mojom::IndigoAgent> receivers_;
};

}  // namespace indigo

#endif  // CHROME_RENDERER_INDIGO_INDIGO_AGENT_H_
