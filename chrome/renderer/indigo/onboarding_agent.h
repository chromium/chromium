// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_INDIGO_ONBOARDING_AGENT_H_
#define CHROME_RENDERER_INDIGO_ONBOARDING_AGENT_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/indigo/indigo.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "v8/include/cppgc/persistent.h"
#include "v8/include/v8-forward.h"

namespace blink {
class AssociatedInterfaceRegistry;
}

namespace indigo {

// Responsible for observing the onboarding dialog's renderer and allowing it to
// report onboarding results to the browser.
//
// This class manages its own lifetime, deleting itself when the RenderFrame it
// observes is destroyed. It can only observe a single RenderFrame.
class OnboardingAgent : public content::RenderFrameObserver {
 public:
  static void MaybeCreate(content::RenderFrame* render_frame,
                          blink::AssociatedInterfaceRegistry* registry);

  explicit OnboardingAgent(content::RenderFrame* render_frame,
                           blink::AssociatedInterfaceRegistry* registry);
  OnboardingAgent(const OnboardingAgent&) = delete;
  OnboardingAgent& operator=(const OnboardingAgent&) = delete;
  ~OnboardingAgent() override;

  // Called by the JS `window.chromeOnboarding.acknowledgeChromeDisclaimer()`.
  void AcknowledgeChromeDisclaimer();

 private:
  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int world_id) override;

  mojo::AssociatedRemote<chrome::mojom::IndigoOnboardingDialogHost> host_;
  base::WeakPtrFactory<OnboardingAgent> weak_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_RENDERER_INDIGO_ONBOARDING_AGENT_H_
