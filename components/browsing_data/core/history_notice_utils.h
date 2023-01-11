// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_HISTORY_NOTICE_UTILS_H_
#define COMPONENTS_BROWSING_DATA_CORE_HISTORY_NOTICE_UTILS_H_

#include "base/functional/callback_forward.h"

namespace history {
class WebHistoryService;
}

namespace syncer {
class SyncService;
}

namespace version_info {
enum class Channel;
}

namespace browsing_data {

// Whether the Clear Browsing Data UI should show a notice about the existence
// of other forms of browsing history stored in user's account. The response
// is returned in a |callback|.
void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    base::OnceCallback<void(bool)> callback);

// Whether the Clear Browsing Data UI should popup a dialog with information
// about the existence of other forms of browsing history stored in user's
// account when the user deletes their browsing history for the first time.
// The response is returned in a |callback|. The |channel| parameter
// must be provided for successful communication with the Sync server, but
// the result does not depend on it.
void ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    version_info::Channel channel,
    base::OnceCallback<void(bool)> callback);

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_HISTORY_NOTICE_UTILS_H_
