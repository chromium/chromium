// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_panel.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"

SpellCheckPanel::SpellCheckPanel(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry,
    service_manager::LocalInterfaceProvider* embedder_provider)
    : content::RenderFrameObserver(render_frame),
      spelling_panel_visible_(false),
      embedder_provider_(embedder_provider) {
  DCHECK(render_frame);
  DCHECK(embedder_provider);
  registry->AddInterface(base::BindRepeating(
      &SpellCheckPanel::SpellCheckPanelReceiver, base::Unretained(this)));
  render_frame->GetWebFrame()->SetSpellCheckPanelHostClient(this);
}

SpellCheckPanel::~SpellCheckPanel() = default;

void SpellCheckPanel::OnDestruct() {
  delete this;
}

bool SpellCheckPanel::IsShowingSpellingUI() {
  return spelling_panel_visible_;
}

void SpellCheckPanel::ShowSpellingUI(bool show) {
  UMA_HISTOGRAM_BOOLEAN("SpellCheck.api.showUI", show);
  GetSpellCheckPanelHost()->ShowSpellingPanel(show);
}

void SpellCheckPanel::UpdateSpellingUIWithMisspelledWord(
    const blink::WebString& word) {
  GetSpellCheckPanelHost()->UpdateSpellingPanelWithMisspelledWord(word.Utf16());
}

void SpellCheckPanel::SpellCheckPanelReceiver(
    mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanel> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SpellCheckPanel::AdvanceToNextMisspelling() {
  auto* render_frame = content::RenderFrameObserver::render_frame();
  DCHECK(render_frame->GetWebFrame());

  render_frame->GetWebFrame()->ExecuteCommand(
      blink::WebString::FromUTF8("AdvanceToNextMisspelling"));
}

void SpellCheckPanel::ToggleSpellPanel(bool visible) {
  auto* render_frame = content::RenderFrameObserver::render_frame();
  DCHECK(render_frame->GetWebFrame());

  // Tell our frame whether the spelling panel is visible or not so
  // that it won't need to make mojo calls later.
  spelling_panel_visible_ = visible;

  render_frame->GetWebFrame()->ExecuteCommand(
      blink::WebString::FromUTF8("ToggleSpellPanel"));
}

mojo::Remote<spellcheck::mojom::SpellCheckPanelHost>
SpellCheckPanel::GetSpellCheckPanelHost() {
  mojo::Remote<spellcheck::mojom::SpellCheckPanelHost> spell_check_panel_host;
  embedder_provider_->GetInterface(
      spell_check_panel_host.BindNewPipeAndPassReceiver());
  return spell_check_panel_host;
}
