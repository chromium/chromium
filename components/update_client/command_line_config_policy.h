// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_
#define COMPONENTS_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_

class GURL;

namespace update_client {

// This class provides additional settings from command line switches to the
// main configurator.
class CommandLineConfigPolicy {
 public:
  // If true, background downloads are enabled.
  virtual bool BackgroundDownloadsEnabled() const;

  // If true, differential updates are enabled.
  virtual bool DeltaUpdatesEnabled() const;

  // If true, speed up the initial update checking.
  virtual bool FastUpdate() const;

  // If true, pings are enabled. Pings are the requests sent to the update
  // server that report the success or failure of installs or update attempts.
  virtual bool PingsEnabled() const;

  // If true, add "testrequest" attribute to update check requests.
  virtual bool TestRequest() const;

  // The override URL for updates. Can be empty.
  virtual GURL UrlSourceOverride() const;

  // If non-zero, time interval in seconds until the first component
  // update check.
  virtual double InitialDelay() const;

  virtual ~CommandLineConfigPolicy() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_COMMAND_LINE_CONFIG_POLICY_H_
