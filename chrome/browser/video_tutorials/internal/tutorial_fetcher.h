// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_FETCHER_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace video_tutorials {

class TutorialFetcher {
 public:
  // Called after the fetch task is done, |status| and serialized response
  // |data| will be returned. Invoked with |nullptr| if status is not success.
  using FinishedCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<std::string> response_body)>;

  // Method to create a TutorialFetcher.
  static std::unique_ptr<TutorialFetcher> Create(
      const GURL& url,
      const std::string& country_code,
      const std::string& accept_languages,
      const std::string& api_key,
      const std::string& experiment_tag,
      const std::string& client_version,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // For testing only.
  static void SetOverrideURLForTesting(const GURL& url);

  // Start the fetch to download tutorials.
  virtual void StartFetchForTutorials(FinishedCallback callback) = 0;

  // Called when accept languages are changed.
  virtual void OnAcceptLanguagesChanged(
      const std::string& accept_languages) = 0;

  virtual ~TutorialFetcher();

  TutorialFetcher(const TutorialFetcher& other) = delete;
  TutorialFetcher& operator=(const TutorialFetcher& other) = delete;

 protected:
  TutorialFetcher();
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_FETCHER_H_
