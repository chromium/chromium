// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_NAVIGATION_CONSOLE_LOGGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_NAVIGATION_CONSOLE_LOGGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace subresource_filter {

// This class provides a static API to log console messages when an ongoing
// navigation successfully commits.
// - This class only supports root frame navigations.
class NavigationConsoleLogger
    : public content::WebContentsObserver,
      public content::NavigationHandleUserData<NavigationConsoleLogger> {
 public:
  // Creates a NavigationConsoleLogger object if it does not already exist for
  // |handle|. It will be scoped until the current root frame navigation commits
  // its next navigation. If |handle| has already committed, logs the message
  // immediately.
  static void LogMessageOnCommit(content::NavigationHandle* handle,
                                 blink::mojom::ConsoleMessageLevel level,
                                 const std::string& message);

  NavigationConsoleLogger(const NavigationConsoleLogger&) = delete;
  NavigationConsoleLogger& operator=(const NavigationConsoleLogger&) = delete;

  ~NavigationConsoleLogger() override;

 private:
  friend class content::NavigationHandleUserData<NavigationConsoleLogger>;
  explicit NavigationConsoleLogger(content::NavigationHandle& handle);

  // Creates a new NavigationConsoleLogger scoped to |handle| if one doesn't
  // exist. Returns the NavigationConsoleLogger associated with |handle|.
  //
  // Note: |handle| must be associated with a root frame navigation.
  static NavigationConsoleLogger* CreateIfNeededForNavigation(
      content::NavigationHandle* handle);

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  using Message = std::pair<blink::mojom::ConsoleMessageLevel, std::string>;
  std::vector<Message> commit_messages_;

  // |handle_| must outlive this class. This is guaranteed because the object
  // tears itself down with |handle_|'s navigation finishes.
  raw_ptr<const content::NavigationHandle> handle_;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_NAVIGATION_CONSOLE_LOGGER_H_
