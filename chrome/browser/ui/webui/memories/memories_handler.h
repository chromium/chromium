// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/memories/memories.mojom.h"
#include "components/history_clusters/core/memories.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if !defined(CHROME_BRANDED)
#include "base/task/cancelable_task_tracker.h"
#endif

class Profile;

namespace content {
class WebContents;
}  // namespace content

#if !defined(CHROME_BRANDED)
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
  void QueryMemories(const std::string& query,
                     QueryMemoriesCallback callback) override;

 private:
  void OnMemoriesQueryResults(
      const std::string& query,
      QueryMemoriesCallback callback,
      std::vector<memories::mojom::MemoryPtr> memory_mojoms);

#if !defined(CHROME_BRANDED)
  using MemoriesQueryResultsCallback =
      base::OnceCallback<void(std::vector<memories::mojom::MemoryPtr>)>;
  void OnHistoryQueryResults(MemoriesQueryResultsCallback callback,
                             history::QueryResults results);
  base::CancelableTaskTracker history_task_tracker_;
#endif

  Profile* profile_;
  content::WebContents* web_contents_;

  mojo::Remote<memories::mojom::Page> page_;
  mojo::Receiver<memories::mojom::PageHandler> page_handler_;

  base::WeakPtrFactory<MemoriesHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_
