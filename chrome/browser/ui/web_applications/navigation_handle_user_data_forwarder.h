// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_HANDLE_USER_DATA_FORWARDER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_HANDLE_USER_DATA_FORWARDER_H_

#include "base/supports_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace web_app {

// Helper class that takes a NavigationHandleUserData and attaches it to the
// first navigation that is started in the given WebContents. The user data
// needs to implement a `AttachToNavigationHandle` method to do the actual
// attaching.
// This is a plain `base::SupportsUserData::Data` rather than a
// `WebContentsUserData` to make it easier to re-use the same user data key
// as what is used by the underlying `NavigationHandleUserData`.
template <typename UserDataType>
class NavigationHandleUserDataForwarder : public content::WebContentsObserver,
                                          public base::SupportsUserData::Data {
 public:
  NavigationHandleUserDataForwarder(content::WebContents& contents,
                                    std::unique_ptr<UserDataType> data,
                                    GURL target_url)
      : content::WebContentsObserver(&contents),
        data_(std::move(data)),
        target_url_(std::move(target_url)) {}
  ~NavigationHandleUserDataForwarder() override = default;

  // Deletes the current instance of `NavigationHandleUserDataForwarder`
  // in the associated `WebContents`.
  void SelfDestruct() {
    CHECK_EQ(web_contents()->GetUserData(UserDataKey()), this);
    web_contents()->RemoveUserData(UserDataKey());
  }

  // content::WebContentsObserver overrides:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() == target_url_) {
      UserDataType::AttachToNavigationHandle(*navigation_handle,
                                             std::move(data_));
    }
    SelfDestruct();
  }

  static const void* UserDataKey() { return UserDataType::UserDataKey(); }

 private:
  std::unique_ptr<UserDataType> data_;
  GURL target_url_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_HANDLE_USER_DATA_FORWARDER_H_
