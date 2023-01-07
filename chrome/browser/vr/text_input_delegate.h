// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEXT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_TEXT_INPUT_DELEGATE_H_

#include "base/functional/callback.h"
#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

struct TextInputInfo;

class VR_BASE_EXPORT TextInputDelegate {
 public:
  TextInputDelegate();

  TextInputDelegate(const TextInputDelegate&) = delete;
  TextInputDelegate& operator=(const TextInputDelegate&) = delete;

  virtual ~TextInputDelegate();

  // RequestFocusCallback gets called when an element requests focus.
  typedef base::RepeatingCallback<void(int)> RequestFocusCallback;
  // RequestUnfocusCallback gets called when an element requests unfocus.
  typedef base::RepeatingCallback<void(int)> RequestUnfocusCallback;
  // UpdateInputCallback gets called when the text input info changes for the
  // element being edited.
  typedef base::RepeatingCallback<void(const TextInputInfo&)>
      UpdateInputCallback;

  void SetRequestFocusCallback(const RequestFocusCallback& callback);
  void SetRequestUnfocusCallback(const RequestUnfocusCallback& callback);
  void SetUpdateInputCallback(const UpdateInputCallback& callback);

  virtual void RequestFocus(int element_id);
  virtual void RequestUnfocus(int element_id);
  virtual void UpdateInput(const TextInputInfo& info);

 private:
  RequestFocusCallback request_focus_callback_;
  RequestUnfocusCallback request_unfocus_callback_;
  UpdateInputCallback update_input_callback_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEXT_INPUT_DELEGATE_H_
