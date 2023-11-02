// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_IOS_BROWSER_WEB_STATE_WRAPPER_H_
#define COMPONENTS_COMMERCE_IOS_BROWSER_WEB_STATE_WRAPPER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/commerce/core/web_wrapper.h"
#include "ios/web/public/web_state.h"

class GURL;

namespace base {
class Value;
}  // namespace base

namespace commerce {

// A WebWrapper backed by web::WebState.
class WebStateWrapper : public WebWrapper {
 public:
  explicit WebStateWrapper(web::WebState* web_state);
  WebStateWrapper(const WebStateWrapper&) = delete;
  WebStateWrapper operator=(const WebStateWrapper&) = delete;
  ~WebStateWrapper() override = default;

  const GURL& GetLastCommittedURL() override;

  bool IsOffTheRecord() override;

  void RunJavascript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value)> callback) override;

  void ClearWebStatePointer();

 private:
  base::raw_ptr<web::WebState> web_state_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_IOS_BROWSER_WEB_STATE_WRAPPER_H_
