// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/blocked_content/popup_blocker.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

struct NavigateParams;

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

  class Observer {
   public:
    virtual void BlockedPopupAdded(int32_t id, const GURL& url) {}

   protected:
    virtual ~Observer() = default;
  };

  ~PopupBlockerTabHelper() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the number of blocked popups.
  size_t GetBlockedPopupsCount() const;

  PopupIdMap GetBlockedPopupRequests();

  // Creates the blocked popup with |popup_id| in given |dispostion|.
  // Note that if |disposition| is WindowOpenDisposition::CURRENT_TAB,
  // blocked popup will be opened as it was specified by renderer.
  void ShowBlockedPopup(int32_t popup_id, WindowOpenDisposition disposition);

  // Adds a new blocked popup to the UI.
  void AddBlockedPopup(NavigateParams* params,
                       const blink::mojom::WindowFeatures& window_features,
                       PopupBlockType block_type);

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Logs a histogram measuring popup blocker actions.
  static void LogAction(Action action);

 private:
  struct BlockedRequest;
  friend class content::WebContentsUserData<PopupBlockerTabHelper>;

  explicit PopupBlockerTabHelper(content::WebContents* web_contents);

  // Called when the blocked popup notification is shown or hidden.
  void PopupNotificationVisibilityChanged(bool visible);

  // Note, this container should be sorted based on the position in the popup
  // list, so it is keyed by an id which is continually increased.
  std::map<int32_t, std::unique_ptr<BlockedRequest>> blocked_popups_;

  base::ObserverList<Observer>::Unchecked observers_;

  int32_t next_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PopupBlockerTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_
