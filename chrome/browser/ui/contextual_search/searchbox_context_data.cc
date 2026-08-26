// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

SearchboxContextData::Context::Context() = default;
SearchboxContextData::Context::~Context() = default;

DEFINE_USER_DATA(SearchboxContextData);

// static
SearchboxContextData* SearchboxContextData::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

SearchboxContextData::SearchboxContextData(ui::UnownedUserDataHost& host)
    : scoped_unowned_user_data_(host, *this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}
SearchboxContextData::~SearchboxContextData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SearchboxContextData::SetPendingContext(std::unique_ptr<Context> context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_context_ = std::move(context);
}

std::unique_ptr<SearchboxContextData::Context>
SearchboxContextData::TakePendingContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::move(pending_context_);
}
