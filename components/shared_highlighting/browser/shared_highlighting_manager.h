// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_BROWSER_SHARED_HIGHLIGHTING_MANAGER_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_BROWSER_SHARED_HIGHLIGHTING_MANAGER_H_

#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"

namespace shared_highlighting {

class SharedHighlightingManager
    : public content::PageUserData<SharedHighlightingManager> {
 public:
  SharedHighlightingManager(const SharedHighlightingManager&) = delete;
  SharedHighlightingManager& operator=(const SharedHighlightingManager&) =
      delete;
  ~SharedHighlightingManager() override;

 private:
  explicit SharedHighlightingManager(content::Page& page);

  // Removes all the highlights from the page.
  void RemoveHighlight();

  // Returns the url that will be shared when resharing a highlight.
  std::string GetUrlToReshareHighlights();

  // Returns a concatenate string of the different text matches to the
  // highlights in the page.
  std::string GetTextMatchesFromHighlights();

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_BROWSER_SHARED_HIGHLIGHTING_MANAGER_H_
