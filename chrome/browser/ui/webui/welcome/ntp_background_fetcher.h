// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "content/public/browser/web_ui_data_source.h"

namespace network {
class SimpleURLLoader;
}

namespace welcome {

class NtpBackgroundFetcher {
 public:
  NtpBackgroundFetcher(size_t index,
                       content::WebUIDataSource::GotDataCallback callback);

  NtpBackgroundFetcher(const NtpBackgroundFetcher&) = delete;
  NtpBackgroundFetcher& operator=(const NtpBackgroundFetcher&) = delete;

  ~NtpBackgroundFetcher();

 private:
  void OnFetchCompleted(std::unique_ptr<std::string> response_body);

  size_t index_;
  content::WebUIDataSource::GotDataCallback callback_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
};

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_FETCHER_H_
