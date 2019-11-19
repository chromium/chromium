// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_SCRIPT_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_SCRIPT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class GURL;

namespace translate {

class TranslateScriptTest;
class TranslateURLFetcher;

class TranslateScript {
 public:
  typedef base::Callback<void(bool, const std::string&)> RequestCallback;

  TranslateScript();
  virtual ~TranslateScript();

  // Returns the fetched translate script.
  const std::string& data() { return data_; }

  // Used by unit-tests to override some defaults:
  // Delay after which the translate script is fetched again from the
  // translation server.
  void set_expiration_delay(int delay_ms) {
    expiration_delay_ = base::TimeDelta::FromMilliseconds(delay_ms);
  }

  // Clears the translate script, so it will be fetched next time we translate.
  void Clear() { data_.clear(); }

  // Fetches the JS translate script (the script that is injected in the page
  // to translate it). |is_incognito| is used during the fetch to determine
  // which variations headers to add.
  void Request(const RequestCallback& callback, bool is_incognito);

  // Returns the URL to be used to load the translate script.
  static GURL GetTranslateScriptURL();

 private:
  friend class TranslateScriptTest;
  FRIEND_TEST_ALL_PREFIXES(TranslateScriptTest, CheckScriptParameters);
  FRIEND_TEST_ALL_PREFIXES(TranslateScriptTest, CheckScriptURL);

  static const char kScriptURL[];
  static const char kRequestHeader[];

  // Used in kTranslateScriptURL to specify using always ssl to load resources.
  static const char kAlwaysUseSslQueryName[];
  static const char kAlwaysUseSslQueryValue[];

  // Used in kTranslateScriptURL to specify a callback function name.
  static const char kCallbackQueryName[];
  static const char kCallbackQueryValue[];

  // Used in kTranslateScriptURL to specify a CSS loader callback function name.
  static const char kCssLoaderCallbackQueryName[];
  static const char kCssLoaderCallbackQueryValue[];

  // Used in kTranslateScriptURL to specify a JavaScript loader callback
  // function name.
  static const char kJavascriptLoaderCallbackQueryName[];
  static const char kJavascriptLoaderCallbackQueryValue[];

  // The callback when the script is fetched or a server error occured.
  void OnScriptFetchComplete(bool success, const std::string& data);

  // URL fetcher to fetch the translate script.
  std::unique_ptr<TranslateURLFetcher> fetcher_;

  // The JS injected in the page to do the translation.
  std::string data_;

  // The starting time of fetching the translate script.
  // Number of milliseconds since the epoch.
  double script_fetch_start_time_;

  // Delay after which the translate script is fetched again from the translate
  // server.
  base::TimeDelta expiration_delay_;

  // The callbacks called when the server sends a response.
  typedef std::vector<RequestCallback> RequestCallbackList;
  RequestCallbackList callback_list_;

  base::WeakPtrFactory<TranslateScript> weak_method_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TranslateScript);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_SCRIPT_H_
