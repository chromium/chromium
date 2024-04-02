// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_COMMON_FIELD_DATA_MANAGER_FACTORY_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_COMMON_FIELD_DATA_MANAGER_FACTORY_IOS_H_

#import "components/autofill/core/common/field_data_manager.h"
#import "ios/web/public/js_messaging/web_frame_user_data.h"

namespace web {
class WebFrame;
}  // namespace web

namespace autofill {

// Interface to create or provide a FieldDataManager to classes that need one.
// Always returns the same instance for a given WebFrame, for the lifetime of
// that frame.
class FieldDataManagerFactoryIOS {
 public:
  // Creates or returns a FieldDataManager corresponding to the given frame.
  // Both pointers must be valid.
  static FieldDataManager* FromWebFrame(web::WebFrame* frame);

  // Creates or returns a FieldDataManager corresponding to the given frame.
  // `frame` must be valid at the time this is called; however, callers may hold
  // the scoped_refptr to prolong the lifetime of the FieldDataManager beyond
  // that of the frame.
  static const scoped_refptr<FieldDataManager> GetRetainable(
      web::WebFrame* frame);

 private:
  // This factory is stateless and does not need to be instantiated.
  FieldDataManagerFactoryIOS() = default;
};

// Holds a refptr to an instance of `FieldDataManager`, scoped to the lifetime
// of the corresponding WebFrame. Should not be accessed directly; use the
// factory methods above instead.
class FieldDataManagerHolderIOS
    : public web::WebFrameUserData<FieldDataManagerHolderIOS> {
 public:
  FieldDataManagerHolderIOS(const FieldDataManagerHolderIOS&) = delete;
  FieldDataManagerHolderIOS& operator=(const FieldDataManagerHolderIOS&) =
      delete;
  ~FieldDataManagerHolderIOS() override;

 private:
  friend class FieldDataManagerFactoryIOS;
  friend class web::WebFrameUserData<FieldDataManagerHolderIOS>;

  FieldDataManager* get() { return manager_.get(); }
  const scoped_refptr<FieldDataManager> GetRetainable() { return manager_; }

  FieldDataManagerHolderIOS(web::WebFrame* frame);
  const scoped_refptr<FieldDataManager> manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_COMMON_FIELD_DATA_MANAGER_FACTORY_IOS_H_
