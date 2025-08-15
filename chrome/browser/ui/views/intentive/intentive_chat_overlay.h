#pragma once
#include "ui/views/widget/widget_delegate.h"
namespace content { class BrowserContext; }
namespace views { class WebView; class View; class Widget; }

class IntentiveChatOverlay : public views::WidgetDelegateView {
 public:
  static views::Widget* ShowOrToggle(views::View* parent_view,
                                     content::BrowserContext* context);
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return false; }
  std::u16string GetWindowTitle() const override { return u"Command"; }
 private:
  IntentiveChatOverlay();
  raw_ptr<views::WebView> web_view_ = nullptr;
};
