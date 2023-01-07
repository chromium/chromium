// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/browser/shared_highlighting_manager.h"

namespace shared_highlighting {

SharedHighlightingManager::SharedHighlightingManager(content::Page& page)
    : content::PageUserData<SharedHighlightingManager>(page) {}

SharedHighlightingManager::~SharedHighlightingManager() = default;

void SharedHighlightingManager::RemoveHighlight() {
  // TODO(cheickcisse): Implement logic that will remove all the highlights from
  // the page.
}

std::string SharedHighlightingManager::GetUrlToReshareHighlights() {
  // TODO(cheickcisse): return url that will be shared when user reshare a
  // highligh.
  return "";
}

std::string SharedHighlightingManager::GetTextMatchesFromHighlights() {
  // TODO(cheickcisse): return concatenate string of text matches separated by
  // ",".
  return "";
}

}  // namespace shared_highlighting
