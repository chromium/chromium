// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_WEB_CONTENTS_DISPLAY_OBSERVER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_WEB_CONTENTS_DISPLAY_OBSERVER_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/media_router/web_contents_display_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget_observer.h"

namespace media_router {

class WebContentsDisplayObserverView : public WebContentsDisplayObserver,
                                       public BrowserListObserver,
                                       public views::WidgetObserver,
                                       public content::WebContentsObserver {
 public:
  WebContentsDisplayObserverView(content::WebContents* web_contents,
                                 base::RepeatingClosure callback);

  ~WebContentsDisplayObserverView() override;

  // BrowserListObserver overrides:
  void OnBrowserSetLastActive(Browser* browser) override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // WebContentsDisplayObserver overrides:
  const display::Display& GetCurrentDisplay() const override;

  // content::WebContentsObserver overrides:
  void WebContentsDestroyed() override;

 private:
  // Calls |callback_| if the WebContents is no longer on |display_|.
  void CheckForDisplayChange();

  // Returns the display that is the closest to |wdiget_|.
  virtual display::Display GetDisplayNearestWidget() const;

  raw_ptr<content::WebContents> web_contents_;

  // The widget containing |web_contents_|.
  raw_ptr<views::Widget> widget_;

  // The display that |web_contents_| is on.
  display::Display display_;

  // Called when the display that |web_contents_| is on changes.
  base::RepeatingClosure callback_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_WEB_CONTENTS_DISPLAY_OBSERVER_VIEW_H_
