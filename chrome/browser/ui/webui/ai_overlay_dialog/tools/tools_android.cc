// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools_android.h"

#include <string>
#include <variant>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ttc {

// static
std::unique_ptr<AiOverlayTools> AiOverlayTools::Create(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser,
    PageContextMonitor* page_context_monitor) {
  return std::make_unique<AiOverlayToolsAndroid>(
      base::PassKey<AiOverlayTools>(), std::move(receiver), browser,
      page_context_monitor);
}

AiOverlayToolsAndroid::AiOverlayToolsAndroid(
    base::PassKey<AiOverlayTools>,
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser,
    PageContextMonitor* page_context_monitor)
    : AiOverlayTools(std::move(receiver), browser, page_context_monitor) {}

AiOverlayToolsAndroid::~AiOverlayToolsAndroid() = default;

void AiOverlayToolsAndroid::SwitchTab(const std::string& query,
                                      SwitchTabCallback callback) {
  RecordToolCallInvoked("SwitchTab");
  std::move(callback).Run(
      base::unexpected("SwitchTab not yet ported to Clank"));
}

void AiOverlayToolsAndroid::CloseCurrentTab(CloseCurrentTabCallback callback) {
  RecordToolCallInvoked("CloseCurrentTab");
  std::move(callback).Run(
      base::unexpected("CloseCurrentTab not yet ported to Clank"));
}

void AiOverlayToolsAndroid::Scroll(
    ai_overlay_dialog::mojom::ScrollGranularity granularity,
    double magnitude,
    ScrollCallback callback) {
  RecordToolCallInvoked("Scroll");
  content::WebContents* contents = GetActiveWebContents();
  if (!contents || !contents->GetRenderWidgetHostView()) {
    std::move(callback).Run(base::unexpected("No active tab or view"));
    return;
  }

  content::RenderWidgetHost* widget_host =
      contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (!widget_host) {
    std::move(callback).Run(base::unexpected("No render widget host"));
    return;
  }

  struct ScrollKey {
    ui::KeyboardCode key_code;
    ui::DomCode dom_code;
    ui::DomKey dom_key;
  };
  auto get_scroll_key =
      [](ai_overlay_dialog::mojom::ScrollGranularity granularity,
         double magnitude) -> ScrollKey {
    switch (granularity) {
      case ai_overlay_dialog::mojom::ScrollGranularity::kPage:
        return magnitude > 0 ? ScrollKey{ui::VKEY_NEXT, ui::DomCode::PAGE_DOWN,
                                         ui::DomKey::PAGE_DOWN}
                             : ScrollKey{ui::VKEY_PRIOR, ui::DomCode::PAGE_UP,
                                         ui::DomKey::PAGE_UP};
      case ai_overlay_dialog::mojom::ScrollGranularity::kDocument:
        return magnitude > 0
                   ? ScrollKey{ui::VKEY_END, ui::DomCode::END, ui::DomKey::END}
                   : ScrollKey{ui::VKEY_HOME, ui::DomCode::HOME,
                               ui::DomKey::HOME};
    }
  };
  const ScrollKey key = get_scroll_key(granularity, magnitude);

  // The events intentionally carry no Android KeyEvent (`os_event`). Events
  // that do are first offered to the app's key handlers, which could consume
  // them before they reach the page.
  for (blink::WebInputEvent::Type type :
       {blink::WebInputEvent::Type::kRawKeyDown,
        blink::WebInputEvent::Type::kKeyUp}) {
    input::NativeWebKeyboardEvent event(
        type, blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
    event.windows_key_code = key.key_code;
    event.dom_code = static_cast<int>(key.dom_code);
    event.dom_key = key.dom_key;
    widget_host->ForwardKeyboardEvent(event);
  }

  std::move(callback).Run(std::monostate());
}

void AiOverlayToolsAndroid::OpenPage(const std::string& query,
                                     OpenPageCallback callback) {
  RecordToolCallInvoked("OpenPage");
  std::move(callback).Run(base::unexpected("OpenPage not yet ported to Clank"));
}

void AiOverlayToolsAndroid::SetFullscreen(bool fullscreen,
                                          SetFullscreenCallback callback) {
  RecordToolCallInvoked("SetFullscreen");
  std::move(callback).Run(
      base::unexpected("SetFullscreen not yet ported to Clank"));
}

void AiOverlayToolsAndroid::OpenGeminiPanel(const std::string& prompt,
                                            OpenGeminiPanelCallback callback) {
  RecordToolCallInvoked("OpenGeminiPanel");
  std::move(callback).Run(base::unexpected("Glic service not available"));
}

void AiOverlayToolsAndroid::CloseGeminiPanel(
    CloseGeminiPanelCallback callback) {
  RecordToolCallInvoked("CloseGeminiPanel");
  std::move(callback).Run(base::unexpected("Glic service not available"));
}

}  // namespace ttc
