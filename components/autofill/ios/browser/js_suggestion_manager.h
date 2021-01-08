// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_JS_SUGGESTION_MANAGER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_JS_SUGGESTION_MANAGER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#import "ios/web/public/web_state_user_data.h"

namespace base {
class Value;
}  // namespace base

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

class JsSuggestionManager : public web::WebStateUserData<JsSuggestionManager> {
 public:
  ~JsSuggestionManager() override;

  static JsSuggestionManager* GetOrCreateForWebState(web::WebState* web_state);

  // Focuses the next focusable element in tab order inside the web frame with
  // frame id |frame_ID|. No action if there is no such element.
  void SelectNextElementInFrameWithID(const std::string& frame_ID);

  // Focuses the next focusable element in tab order after the element specified
  // by |form_name| and |field_name| in tab order inside the web frame with
  // frame id |frame_ID|. No action if there is no such element.
  void SelectNextElementInFrameWithID(const std::string& frame_ID,
                                      const std::string& form_name,
                                      const std::string& field_name);

  // Focuses the previous focusable element in tab order inside the web frame
  // with frame id |frame_ID|. No action if there is no such element.
  void SelectPreviousElementInFrameWithID(const std::string& frame_ID);

  // Focuses the previous focusable element in tab order from the element
  // specified by |form_name| and |field_name| in tab order inside the web frame
  // with frame id |frame_ID|. No action if there is no such element.
  void SelectPreviousElementInFrameWithID(const std::string& frame_ID,
                                          const std::string& form_name,
                                          const std::string& field_name);

  // Checks if the frame with frame id |frame_ID| contains a next and previous
  // element. |completionHandler| is called with 2 bools, the first indicating
  // if a previous element was found, and the second indicating if a next
  // element was found. |completionHcompletion_handlerandler| cannot be nil.
  void FetchPreviousAndNextElementsPresenceInFrameWithID(
      const std::string& frame_ID,
      base::OnceCallback<void(bool, bool)> completion_handler);

  // Checks if the frame with frame id |frame_ID| contains a next and previous
  // element starting from the field specified by |form_name| and |field_name|.
  // |completionHandler| is called with 2 BOOLs, the first indicating if a
  // previous element was found, and the second indicating if a next element was
  // found. |completion_handler| cannot be nil.
  void FetchPreviousAndNextElementsPresenceInFrameWithID(
      const std::string& frame_ID,
      const std::string& form_name,
      const std::string& field_name,
      base::OnceCallback<void(bool, bool)> completion_handler);

  // Closes the keyboard and defocuses the active input element in the frame
  // with frame id |frame_ID|.
  void CloseKeyboardForFrameWithID(const std::string& frame_ID);

 private:
  explicit JsSuggestionManager(web::WebState* web_state);

  void PreviousAndNextElementsPresenceResult(
      base::OnceCallback<void(bool, bool)> completion_handler,
      const base::Value* res);

  web::WebFrame* GetFrameWithFrameID(const std::string& frame_ID);

  web::WebState* web_state_;

  base::WeakPtrFactory<JsSuggestionManager> weak_ptr_factory_;

  friend class web::WebStateUserData<JsSuggestionManager>;

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_JS_SUGGESTION_MANAGER_H_
