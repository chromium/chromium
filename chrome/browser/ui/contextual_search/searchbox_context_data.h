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
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

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
    std::vector<searchbox::mojom::SearchContextAttachmentPtr> file_infos;
    omnibox::ToolMode mode = omnibox::TOOL_MODE_UNSPECIFIED;
  };

  DECLARE_USER_DATA(SearchboxContextData);

  // `host` is the UnownedUserDataHost of the browser window this data
  // belongs to; tests may pass their own host.
  explicit SearchboxContextData(ui::UnownedUserDataHost& host);
  ~SearchboxContextData();

  // Returns the data for `browser`, or null if it does not have one.
  static SearchboxContextData* From(BrowserWindowInterface* browser);

  // Takes ownership of pending context from caller.
  void SetPendingContext(std::unique_ptr<Context> context);

  // Transfers ownership of the pending context to caller.
  std::unique_ptr<Context> TakePendingContext();

 private:
  ui::ScopedUnownedUserData<SearchboxContextData> scoped_unowned_user_data_;

  std::unique_ptr<Context> pending_context_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_SEARCHBOX_CONTEXT_DATA_H_
