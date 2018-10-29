// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_API_H_
#define CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_API_H_

#include <vector>

#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

class RebooterAPI;

// This class is used to register components that are to be executed by the main
// controller either before the scanner or after the cleaner.
class ComponentAPI {
 public:
  virtual ~ComponentAPI() {}

  // Called before the scanning starts. Components may need to consult the
  // command line to identify if this is a post-reboot scan.
  virtual void PreScan() = 0;

  // Called after scanning. |found_pups| contains the ids of the found pups.
  virtual void PostScan(const std::vector<UwSId>& found_pups) = 0;

  // Called before cleanup starts.
  virtual void PreCleanup() = 0;

  // Called after the final cleanup. |result_code| can be used to identify if
  // a post-reboot removal will be needed, or other status that might be useful
  // to the component. |rebooter| is specified in case the component needs to
  // specify command line arguments for the post-reboot run so that the
  // |PostValidation| call below can take some decisions based on it. |rebooter|
  // can be null in tests.
  virtual void PostCleanup(ResultCode result_code, RebooterAPI* rebooter) = 0;

  // Called after the post reboot run scanner is done validating the cleanup.
  virtual void PostValidation(ResultCode result_code) = 0;

  // Called when the final dialog window is closed. As above, |result_code| can
  // be used to tell the result of the run (i.e., whether a cleanup was needed
  // or not). Since this happens after users had a chance to look at the logs to
  // decide whether they want to opt-out or not, any logging done here won't be
  // uploaded back to Google.
  virtual void OnClose(ResultCode result_code) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_API_H_
