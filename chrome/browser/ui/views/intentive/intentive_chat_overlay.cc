#include "chrome/browser/ui/views/intentive/intentive_chat_overlay.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace { constexpr int kW=420, kH=600; }

IntentiveChatOverlay::IntentiveChatOverlay() { SetUseDefaultFillLayout(true); }

views::Widget* IntentiveChatOverlay::ShowOrToggle(views::View* parent,
                                                  content::BrowserContext* ctx) {
  auto* overlay = new IntentiveChatOverlay();
  overlay->SetOwnedByWidget(true);
  overlay->web_view_ = overlay->AddChildView(std::make_unique<views::WebView>(ctx));
  overlay->web_view_->LoadInitialURL(GURL("https://chatgpt.com"));

  auto* widget = new views::Widget();
  views::Widget::InitParams p(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  p.delegate = overlay; p.name = "IntentiveChatOverlay";
  p.parent = parent->GetWidget()->GetNativeView();
  widget->Init(std::move(p));

  auto b = parent->GetBoundsInScreen();
  widget->SetBounds({b.right()-kW-16, b.bottom()-kH-16, kW, kH});
  widget->Show();
  return widget;
}
