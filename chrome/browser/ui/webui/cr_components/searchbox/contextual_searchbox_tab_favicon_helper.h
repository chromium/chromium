// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TAB_FAVICON_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TAB_FAVICON_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "url/gurl.h"

class Profile;

class ContextualSearchboxTabFaviconHelper {
 public:
  ContextualSearchboxTabFaviconHelper();
  ContextualSearchboxTabFaviconHelper(
      const ContextualSearchboxTabFaviconHelper&) = delete;
  ContextualSearchboxTabFaviconHelper& operator=(
      const ContextualSearchboxTabFaviconHelper&) = delete;
  ~ContextualSearchboxTabFaviconHelper();

  // Waits for the tab with `tab_id` to finish loading its favicon and
  // returns its base64 data URL via `callback`.
  void WaitForTabFaviconLoad(
      int32_t tab_id,
      Profile* profile,
      base::OnceCallback<void(const std::optional<GURL>&)> callback);

 private:
  class Waiter;
  std::vector<std::unique_ptr<Waiter>> active_waiters_;
  void RemoveWaiter(Waiter* waiter);

  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<ContextualSearchboxTabFaviconHelper> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TAB_FAVICON_HELPER_H_
