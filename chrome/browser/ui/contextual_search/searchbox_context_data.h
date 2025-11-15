// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_SEARCHBOX_CONTEXT_DATA_H_
#define CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_SEARCHBOX_CONTEXT_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "components/omnibox/browser/searchbox.mojom.h"

// Temporarily stores search context (files, mode, query, etc.) while moving
// from one piece of UI to another (e.g. context menu to composebox dialog).
//
// This class is not thread-safe and should only be used on a single thread.
class SearchboxContextData {
 public:
  struct Context {
    Context();
    ~Context();

    std::string text;
    std::vector<searchbox::mojom::SearchContextAttachmentStubPtr> file_infos;
    searchbox::mojom::ToolMode mode = searchbox::mojom::ToolMode::kDefault;
  };

  SearchboxContextData();
  ~SearchboxContextData();

  // Takes ownership of pending context from caller.
  void SetPendingContext(std::unique_ptr<Context> context);

  // Transfers ownership of the pending context to caller.
  std::unique_ptr<Context> TakePendingContext();

 private:
  std::unique_ptr<Context> pending_context_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_SEARCHBOX_CONTEXT_DATA_H_
