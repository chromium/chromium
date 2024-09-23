// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_IOS_BROWSER_WEB_STATE_WRAPPER_H_
#define COMPONENTS_COMMERCE_IOS_BROWSER_WEB_STATE_WRAPPER_H_

#include <string>

#include "base/functional/callback.h"
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
  ~WebStateWrapper() override;

  const GURL& GetLastCommittedURL() override;

  const std::u16string& GetTitle() override;

  bool IsFirstLoadForNavigationFinished() override;

  void SetIsFirstLoadForNavigationFinished(bool finished);

  bool IsOffTheRecord() override;

  void RunJavascript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value)> callback) override;

  ukm::SourceId GetPageUkmSourceId() override;

  base::WeakPtr<WebWrapper> GetWeakPtr();

  void ClearWebStatePointer();

 private:
  raw_ptr<web::WebState> web_state_;

  // Whether the first load after a navigation has completed. This is useful
  // when dealing with single-page apps that may not fire subsequent load
  // events.
  bool is_first_load_for_nav_finished_{false};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_IOS_BROWSER_WEB_STATE_WRAPPER_H_
