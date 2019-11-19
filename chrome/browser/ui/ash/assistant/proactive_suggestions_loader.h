// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_PROACTIVE_SUGGESTIONS_LOADER_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_PROACTIVE_SUGGESTIONS_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace ash {
class ProactiveSuggestions;
}  // namespace ash

namespace network {
class SimpleURLLoader;
}  // namespace network

// A wrapper around a SimpleURLLoader which is responsible for downloading
// proactive content suggestions for a given |url|.
class ProactiveSuggestionsLoader {
 public:
  ProactiveSuggestionsLoader(Profile* profile, const GURL& url);
  ~ProactiveSuggestionsLoader();

  // Callback used when downloading of |proactive_suggestions| is complete.
  // Note that |proactive_suggestions| may be |nullptr|.
  using CompleteCallback = base::OnceCallback<void(
      scoped_refptr<ash::ProactiveSuggestions> proactive_suggestions)>;

  // Starts downloading of |proactive_suggestions| associated with |url_|,
  // running |complete_callback| when finished. Note that this method should be
  // called only once per ProactiveSuggestionsLoader instance.
  void Start(CompleteCallback complete_callback);

 private:
  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);

  Profile* const profile_;
  const GURL url_;
  CompleteCallback complete_callback_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsLoader);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_PROACTIVE_SUGGESTIONS_LOADER_H_
