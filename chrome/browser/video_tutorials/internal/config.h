// Copyright 2020 The Chromium Authors
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

// Finch parameter key for base server URL to retrieve the tutorials.
extern const char kBaseURLKey[];

// Finch parameter key for the default preferred locale.
extern const char kPreferredLocaleConfigKey[];

// Default preferred locale setting before user picks the language.
// This will be used as the language for the video tutorial promo cards.
extern const char kDefaultPreferredLocale[];

// Finch parameter key for the fetch frequency to retrieve the tutorials.
extern const char kFetchFrequencyKey[];

class Config {
 public:
  // Get video tutorials metadata server URL.
  static GURL GetTutorialsServerURL(const std::string& default_server_url);

  // Get the default locale before users choice.
  static std::string GetDefaultPreferredLocale();

  // Get the default fetch frequency.
  static base::TimeDelta GetFetchFrequency();

  // Gets the experiment tag to be passed to server.
  static std::string GetExperimentTag();
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_CONFIG_H_
