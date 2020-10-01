// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_CONFIG_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_CONFIG_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

namespace video_tutorials {

// Default URL string for GetVideoTutorials RPC.
extern const char kDefaultGetTutorialsPath[];

// Default base URL string.
extern const char kDefaultBaseURL[];

// Finch parameter key for base server URL to retrieve the tutorials.
extern const char kBaseURLKey[];

// Finch parameter key for the default preferred locale.
extern const char kPreferredLocaleConfigKey[];

// Default preferred locale setting before users pick.
extern const char kDefaultPreferredLocale[];

// Finch parameter key for the fetch frequency to retrieve the tutorials.
extern const char kFetchFrequencyKey[];

class Config {
 public:
  // Get video tutorials metadata server URL.
  static GURL GetTutorialsServerURL();

  // Get the default locale before users choice.
  static std::string GetDefaultPreferredLocale();

  // Get the default fetch frequency.
  static base::TimeDelta GetFetchFrequency();

  // Gets the experiment tag to be passed to server.
  static std::string GetExperimentTag();
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_CONFIG_H_
