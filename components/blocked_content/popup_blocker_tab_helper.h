// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_
#define COMPONENTS_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/url_list_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace blocked_content {
class PopupNavigationDelegate;

// Per-tab class to manage blocked popups.
class PopupBlockerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PopupBlockerTabHelper> {
 public:
  // Mapping from popup IDs to blocked popup requests.
  typedef std::map<int32_t, GURL> PopupIdMap;

  // This enum is backed by a histogram. Make sure enums.xml is updated if this
  // is updated.
  enum class Action : int {
    // A popup was initiated and was sent to the popup blocker for
    // consideration.
    kInitiated = 0,

    // A popup was blocked by the popup blocker.
    kBlocked = 1,

    // A previously blocked popup was clicked through. For popups blocked
    // without a user gesture.
    kClickedThroughNoGesture = 2,

    // A previously blocked popup was clicked through. For popups blocked
    // due to the abusive popup blocker.
    kClickedThroughAbusive = 3,

    // Add new elements before this value.
    kMaxValue = kClickedThroughAbusive
  };

  PopupBlockerTabHelper(const PopupBlockerTabHelper&) = delete;
  PopupBlockerTabHelper& operator=(const PopupBlockerTabHelper&) = delete;

  ~PopupBlockerTabHelper() override;

  // Returns the number of blocked popups.
  size_t GetBlockedPopupsCount() const;

  PopupIdMap GetBlockedPopupRequests();

  // Creates the blocked popup with |popup_id| in given |dispostion|.
  // Note that if |disposition| is WindowOpenDisposition::CURRENT_TAB,
  // blocked popup will be opened as it was specified by renderer.
  void ShowBlockedPopup(int32_t popup_id, WindowOpenDisposition disposition);

  // All blocked popups will be opened with the disposition defaulted to
  // WindowOpenDisposition::CURRENT_TAB. Used only on Android.
  void ShowAllBlockedPopups();

  // Adds a new blocked popup to the UI.
  void AddBlockedPopup(std::unique_ptr<PopupNavigationDelegate> delegate,
                       const blink::mojom::WindowFeatures& window_features,
                       PopupBlockType block_type);

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Logs a histogram measuring popup blocker actions.
  static void LogAction(Action action);

  blocked_content::UrlListManager* manager() { return &manager_; }

 private:
  struct BlockedRequest;
  friend class content::WebContentsUserData<PopupBlockerTabHelper>;

  explicit PopupBlockerTabHelper(content::WebContents* web_contents);

  // Called when the blocked popup notification is hidden.
  void HidePopupNotification();

  blocked_content::UrlListManager manager_;

  // Note, this container should be sorted based on the position in the popup
  // list, so it is keyed by an id which is continually increased.
  std::map<int32_t, std::unique_ptr<BlockedRequest>> blocked_popups_;

  int32_t next_id_ = 0;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_
