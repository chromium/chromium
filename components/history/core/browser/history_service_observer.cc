// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_service_observer.h"

namespace history {

void HistoryServiceObserver::OnURLsModified(HistoryService* history_service,
                                            const URLRows& changed_urls,
                                            UrlsModifiedReason reason) {
  OnURLsModified(history_service, changed_urls);
}

}  // namespace history
