// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_NEW_FRAME_CATCHER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_NEW_FRAME_CATCHER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace autofill::test {

// Catcher that captures the latest new frame reported by `manager`.
class NewFrameCatcher : public web::WebFramesManager::Observer {
 public:
  explicit NewFrameCatcher(web::WebFramesManager* manager);
  ~NewFrameCatcher() override;

  // Returns the latest new frame that was observed. Returns nullptr if nothing
  // was seen.
  web::WebFrame* latest_new_frame() { return latest_new_frame_; }

 private:
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;

  raw_ptr<web::WebFrame> latest_new_frame_ = nullptr;
  base::ScopedObservation<web::WebFramesManager,
                          web::WebFramesManager::Observer>
      scoped_observer_{this};
};

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_NEW_FRAME_CATCHER_H_
