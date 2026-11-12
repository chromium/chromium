// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TTC_TTC_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_TTC_TTC_VIEW_DELEGATE_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"

namespace ttc {

// How TTC's UI talks to the session. Outlives every TtcView it's passed to.
class TtcViewDelegate {
 public:
  virtual ~TtcViewDelegate() = default;

  // Ends the session asynchronously.
  virtual void EndSessionAsync() = 0;

  // Runs `callback` with each audio level update of the user's microphone.
  virtual base::CallbackListSubscription RegisterAudioLevelCallback(
      base::RepeatingCallback<void(float)> callback) = 0;

  // Mutes the user's microphone, or unmutes it if it's muted.
  virtual void ToggleMicMuted() = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_VIEWS_TTC_TTC_VIEW_DELEGATE_H_
