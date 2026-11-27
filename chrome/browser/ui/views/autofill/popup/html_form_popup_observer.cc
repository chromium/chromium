// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/html_form_popup_observer.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

namespace autofill {

HtmlFormPopupObserver::HtmlFormPopupObserver(
    content::WebContents& web_contents,
    base::RepeatingClosure on_html_form_popup_shown)
    : web_contents_(web_contents.GetWeakPtr()),
      on_html_form_popup_shown_(std::move(on_html_form_popup_shown)) {
  CHECK(on_html_form_popup_shown_);
  if (auto* client = aura::client::GetTransientWindowClient()) {
    client->AddObserver(this);
  }
}

HtmlFormPopupObserver::~HtmlFormPopupObserver() {
  if (auto* client = aura::client::GetTransientWindowClient()) {
    client->RemoveObserver(this);
  }
}

void HtmlFormPopupObserver::OnTransientChildWindowAdded(
    aura::Window* parent,
    aura::Window* transient_child) {
  // `RenderWidgetHostViewAura::InitAsPopup()` creates renderer-owned HTML form
  // popup windows (such as <select>, date, or color pickers) with
  // `WINDOW_TYPE_MENU` and attaches them as transient children of the parent
  // view. Filtering on `WINDOW_TYPE_MENU` avoids false positives from other
  // transient children (such as tooltips or `WINDOW_TYPE_POPUP` widgets).
  if (!parent || !transient_child ||
      transient_child->GetType() != aura::client::WINDOW_TYPE_MENU) {
    return;
  }
  content::WebContents* outermost =
      web_contents_ ? web_contents_->GetOutermostWebContents() : nullptr;
  aura::Window* outermost_native_view =
      outermost ? outermost->GetNativeView() : nullptr;
  if (!outermost_native_view || !outermost_native_view->Contains(parent)) {
    return;
  }
  // Post a task rather than notifying synchronously: this callback fires inside
  // `TransientWindowManager::AddTransientChild()` (before restacking and
  // per-window observer notifications) and midway through
  // `RenderWidgetHostViewAura::InitAsPopup()`, while callers typically close a
  // widget and trigger re-entrant accessibility and focus updates.
  if (!notification_pending_) {
    notification_pending_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HtmlFormPopupObserver::NotifyPopupShown,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void HtmlFormPopupObserver::OnTransientChildWindowRemoved(
    aura::Window* parent,
    aura::Window* transient_child) {}

void HtmlFormPopupObserver::NotifyPopupShown() {
  notification_pending_ = false;
  on_html_form_popup_shown_.Run();
}

}  // namespace autofill
