// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {
namespace shell {

class CastDevToolsManagerDelegate;

class RemoteDebuggingServer {
 public:
  explicit RemoteDebuggingServer(bool start_immediately);

  RemoteDebuggingServer(const RemoteDebuggingServer&) = delete;
  RemoteDebuggingServer& operator=(const RemoteDebuggingServer&) = delete;

  ~RemoteDebuggingServer();

  // Allows this WebContents to be debugged.
  void EnableWebContentsForDebugging(content::WebContents* web_contents);

  // Disables remote debugging for this web contents.
  void DisableWebContentsForDebugging(content::WebContents* web_contents);

 private:
  CastDevToolsManagerDelegate* GetDevtoolsDelegate();
  void StartIfNeeded();
  void StopIfNeeded();

  class WebContentsObserver;
  std::unordered_map<content::WebContents*,
                     std::unique_ptr<WebContentsObserver>>
      observers_;
  uint16_t port_;
  bool is_started_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
