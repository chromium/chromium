// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_SUGGESTION_CONTROLLER_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_SUGGESTION_CONTROLLER_JAVA_SCRIPT_FEATURE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace autofill {

class SuggestionControllerJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static SuggestionControllerJavaScriptFeature* GetInstance();

  // Focuses the next focusable element in tab order inside |frame|. No action
  // if there is no such element.
  void SelectNextElementInFrame(web::WebFrame* frame);

  // Focuses the next focusable element in tab order after the element specified
  // by |form_name| and |field_name| in tab order inside |frame|. No action if
  // there is no such element.
  void SelectNextElementInFrame(web::WebFrame* frame,
                                const std::string& form_name,
                                const std::string& field_name);

  // Focuses the previous focusable element in tab order inside |frame|. No
  // action if there is no such element.
  void SelectPreviousElementInFrame(web::WebFrame* frame);

  // Focuses the previous focusable element in tab order from the element
  // specified by |form_name| and |field_name| in tab order inside |frame|. No
  // action if there is no such element.
  void SelectPreviousElementInFrame(web::WebFrame* frame,
                                    const std::string& form_name,
                                    const std::string& field_name);

  // Checks if |frame| contains a next and previous element.
  // |completionHandler| is called with 2 bools, the first indicating if a
  // previous element was found, and the second indicating if a next element was
  // found. |completion_handler| cannot be nil.
  void FetchPreviousAndNextElementsPresenceInFrame(
      web::WebFrame* frame,
      base::OnceCallback<void(bool, bool)> completion_handler);

  // Checks if |frame| contains a next and previous element starting from the
  // field specified by |form_name| and |field_name|.
  // |completionHandler| is called with 2 BOOLs, the first indicating if a
  // previous element was found, and the second indicating if a next element was
  // found. |completion_handler| cannot be nil.
  void FetchPreviousAndNextElementsPresenceInFrame(
      web::WebFrame* frame,
      const std::string& form_name,
      const std::string& field_name,
      base::OnceCallback<void(bool, bool)> completion_handler);

 private:
  friend class base::NoDestructor<SuggestionControllerJavaScriptFeature>;

  SuggestionControllerJavaScriptFeature();
  ~SuggestionControllerJavaScriptFeature() override;

  SuggestionControllerJavaScriptFeature(
      const SuggestionControllerJavaScriptFeature&) = delete;
  SuggestionControllerJavaScriptFeature& operator=(
      const SuggestionControllerJavaScriptFeature&) = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_SUGGESTION_CONTROLLER_JAVA_SCRIPT_FEATURE_H_
