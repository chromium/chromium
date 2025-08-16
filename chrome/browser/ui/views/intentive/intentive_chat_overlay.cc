#include "chrome/browser/ui/views/intentive/intentive_chat_overlay.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace {
constexpr int kW = 420;
constexpr int kH = 600;
constexpr int kPadding = 16;
}  // namespace

// static
views::Widget* IntentiveChatOverlay::ShowOrToggle(views::View* parent_view,
                                                  content::BrowserContext* context) {
  // For true toggling, store a per-window Widget* somewhere (e.g., BrowserView).
  auto* overlay = new IntentiveChatOverlay(context);

  auto* widget = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,  // Ownership
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);     // Type
  params.name = "IntentiveChatOverlay";
  params.delegate = nullptr;  // we'll SetContentsView(...) instead
  params.parent = parent_view->GetWidget()->GetNativeView(); // overlay above contents
  widget->Init(std::move(params));

  widget->SetContentsView(overlay);  // widget now owns |overlay|

  // Position bottom-right within the parent content area.
  const gfx::Rect pb = parent_view->GetBoundsInScreen();
  widget->SetBounds({pb.right() - kW - kPadding,
                     pb.bottom() - kH - kPadding,
                     kW, kH});

  widget->Show();
  return widget;
}

IntentiveChatOverlay::IntentiveChatOverlay(content::BrowserContext* context)
    : browser_context_(context) {
  SetUseDefaultFillLayout(true);
  web_view_ = AddChildView(std::make_unique<views::WebView>(browser_context_));
  web_view_->LoadInitialURL(GURL("https://chatgpt.com"));
  web_view_->SetPreferredSize(gfx::Size(kW, kH));
}

IntentiveChatOverlay::~IntentiveChatOverlay() = default;
