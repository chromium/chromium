// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PANEL_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PANEL_H_

#include "base/memory/raw_ptr.h"
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/platform/web_spell_check_panel_host_client.h"

#if !BUILDFLAG(HAS_SPELLCHECK_PANEL)
#error "Spellcheck panel should be enabled."
#endif

namespace service_manager {
class LocalInterfaceProvider;
}

class SpellCheckPanel : public content::RenderFrameObserver,
                        public blink::WebSpellCheckPanelHostClient,
                        public spellcheck::mojom::SpellCheckPanel {
 public:
  SpellCheckPanel(content::RenderFrame* render_frame,
                  service_manager::BinderRegistry* registry,
                  service_manager::LocalInterfaceProvider* embedder_provider);

  SpellCheckPanel(const SpellCheckPanel&) = delete;
  SpellCheckPanel& operator=(const SpellCheckPanel&) = delete;

  ~SpellCheckPanel() override;

 private:
  // content::RenderFrameObserver:
  void OnDestruct() override;

  // blink::WebSpellCheckPanelHostClient:
  bool IsShowingSpellingUI() override;
  void ShowSpellingUI(bool show) override;
  void UpdateSpellingUIWithMisspelledWord(
      const blink::WebString& word) override;

  // Binds browser receivers for the frame SpellCheckPanel interface.
  void SpellCheckPanelReceiver(
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanel> receiver);

  // spellcheck::mojom::SpellCheckPanel:
  void ToggleSpellPanel(bool visible) override;
  void AdvanceToNextMisspelling() override;

  mojo::Remote<spellcheck::mojom::SpellCheckPanelHost> GetSpellCheckPanelHost();

  // SpellCheckPanel receivers.
  mojo::ReceiverSet<spellcheck::mojom::SpellCheckPanel> receivers_;

  // True if the browser is showing the spelling panel.
  bool spelling_panel_visible_;

  raw_ptr<service_manager::LocalInterfaceProvider> embedder_provider_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PANEL_H_
