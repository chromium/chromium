// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/ui/webui/memories/memories.mojom.h"
#include "components/history_clusters/core/memories.mojom.h"
#include "components/history_clusters/core/memories_remote_model_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if !defined(OFFICIAL_BUILD)
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#endif

class Profile;

namespace content {
class WebContents;
}  // namespace content

#if !defined(OFFICIAL_BUILD)
namespace history {
class QueryResults;
}  // namespace history
#endif

// Handles bidirectional communication between memories page and the browser.
class MemoriesHandler : public memories::mojom::PageHandler {
 public:
  MemoriesHandler(
      mojo::PendingReceiver<memories::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents);
  ~MemoriesHandler() override;

  MemoriesHandler(const MemoriesHandler&) = delete;
  MemoriesHandler& operator=(const MemoriesHandler&) = delete;

  // memories::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<memories::mojom::Page> pending_page) override;

  using MemoriesResultCallback =
      base::OnceCallback<void(memories::mojom::MemoriesResultPtr)>;
  void GetSampleMemories(const std::string& query,
                         MemoriesResultCallback callback) override;

  void GetMemories(MemoriesResultCallback callback) override;

 private:
#if !defined(OFFICIAL_BUILD)
  void OnHistoryQueryResults(const std::string& query,
                             MemoriesResultCallback callback,
                             history::QueryResults results);
#endif

  Profile* profile_;
  content::WebContents* web_contents_;

  mojo::Remote<memories::mojom::Page> page_;
  mojo::Receiver<memories::mojom::PageHandler> page_handler_;

#if !defined(OFFICIAL_BUILD)
  base::CancelableTaskTracker history_task_tracker_;
  base::WeakPtrFactory<MemoriesHandler> weak_ptr_factory_{this};
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_
