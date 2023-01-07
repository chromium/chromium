// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_EVENT_LOGGER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_EVENT_LOGGER_H_

#include "components/offline_pages/core/offline_event_logger.h"

namespace offline_pages {

class OfflinePageModelEventLogger : public OfflineEventLogger {
 public:
  // Records that a page has been saved for |name_space| with |url|
  // and |offline_id|.
  void RecordPageSaved(const std::string& name_space,
                       const std::string& url,
                       int64_t offline_id);

  // Records that a page with |offline_id| has been deleted.
  void RecordPageDeleted(int64_t offline_id);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_EVENT_LOGGER_H_
