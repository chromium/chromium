// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_HTML_FORM_POPUP_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_HTML_FORM_POPUP_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/transient_window_client_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// Observes Aura transient window additions and invokes a callback when a
// renderer-owned HTML form popup (`WINDOW_TYPE_MENU`, e.g. <select> or date
// picker) is attached inside the same outermost WebContents tree.
//
// Observation starts on construction and ends on destruction (RAII).
class HtmlFormPopupObserver
    : public aura::client::TransientWindowClientObserver {
 public:
  HtmlFormPopupObserver(content::WebContents& web_contents,
                        base::RepeatingClosure on_html_form_popup_shown);
  HtmlFormPopupObserver(const HtmlFormPopupObserver&) = delete;
  HtmlFormPopupObserver& operator=(const HtmlFormPopupObserver&) = delete;
  ~HtmlFormPopupObserver() override;

  // aura::client::TransientWindowClientObserver:
  void OnTransientChildWindowAdded(aura::Window* parent,
                                   aura::Window* transient_child) override;
  void OnTransientChildWindowRemoved(aura::Window* parent,
                                     aura::Window* transient_child) override;

 private:
  void NotifyPopupShown();

  base::WeakPtr<content::WebContents> web_contents_;
  base::RepeatingClosure on_html_form_popup_shown_;
  bool notification_pending_ = false;
  base::WeakPtrFactory<HtmlFormPopupObserver> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_HTML_FORM_POPUP_OBSERVER_H_
