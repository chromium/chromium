// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_UNIQUE_ID_DATA_TAB_HELPER_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_UNIQUE_ID_DATA_TAB_HELPER_H_

#include "base/memory/ref_counted_memory.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Class binding a unique renderer IDs data to a WebState.
class UniqueIDDataTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<UniqueIDDataTabHelper> {
 public:
  UniqueIDDataTabHelper(const UniqueIDDataTabHelper&) = delete;
  UniqueIDDataTabHelper& operator=(const UniqueIDDataTabHelper&) = delete;

  ~UniqueIDDataTabHelper() override;

  // Returns the next available renderer id for WebState.
  uint32_t GetNextAvailableRendererID() const;

  void SetNextAvailableRendererID(uint32_t new_id);

  const scoped_refptr<autofill::FieldDataManager> GetFieldDataManager();

 private:
  friend class web::WebStateUserData<UniqueIDDataTabHelper>;

  explicit UniqueIDDataTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  uint32_t next_available_renderer_id_ = 1;

  // Maps UniqueFieldId of an input element to the pair of:
  // 1) The most recent text that user typed or PasswordManager autofilled in
  // input elements. Used for storing username/password before JavaScript
  // changes them.
  // 2) Field properties mask, i.e. whether the field was autofilled, modified
  // by user, etc. (see FieldPropertiesMask).
  scoped_refptr<autofill::FieldDataManager> field_data_manager_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_UNIQUE_ID_DATA_TAB_HELPER_H_
