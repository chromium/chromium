// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_NAVIGATION_CONSOLE_LOGGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_NAVIGATION_CONSOLE_LOGGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace subresource_filter {

// This class provides a static API to log console messages when an ongoing
// navigation successfully commits.
// - This class only supports main frame navigations.
// - This class should be replaced with a class scoped to the NavigationHandle
//   if it ever starts supporting user data.
class NavigationConsoleLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NavigationConsoleLogger> {
 public:
  // Creates a NavigationConsoleLogger object if it does not already exist for
  // |handle|'s WebContents. It will be scoped until the current main frame
  // navigation in |contents| commits its next navigation. If |handle| has
  // already committed, logs the message immediately.
  static void LogMessageOnCommit(content::NavigationHandle* handle,
                                 blink::mojom::ConsoleMessageLevel level,
                                 const std::string& message);

  ~NavigationConsoleLogger() override;

 private:
  friend class content::WebContentsUserData<NavigationConsoleLogger>;
  explicit NavigationConsoleLogger(content::NavigationHandle* handle);

  // Creates a new NavigationConsoleLogger scoped to |handle|'s WebContents if
  // one doesn't exist. Returns the NavigationConsoleLogger associated with
  // |handle|'s WebContents.
  //
  // Note: |handle| must be associated with a main frame navigation.
  static NavigationConsoleLogger* CreateIfNeededForNavigation(
      content::NavigationHandle* handle);

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  using Message = std::pair<blink::mojom::ConsoleMessageLevel, std::string>;
  std::vector<Message> commit_messages_;

  // |handle_| must outlive this class. This is guaranteed because the object
  // tears itself down with |handle_|'s navigation finishes.
  const content::NavigationHandle* handle_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(NavigationConsoleLogger);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_NAVIGATION_CONSOLE_LOGGER_H_
