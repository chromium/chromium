// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CAMERA_CANCEL_CAMERA_UPLOAD_DIALOG_H_
#define CHROMEOS_ASH_EXPERIENCES_CAMERA_CANCEL_CAMERA_UPLOAD_DIALOG_H_

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {
FORWARD_DECLARE_TEST(CameraSaveHandlerTest, CancelUpload);
}

namespace ui {
class DialogModelDelegate;
}

class CancelCameraUploadDialog {
 public:
  using ClickedCallback =
      base::RepeatingCallback<void(bool cancel, bool skip_dialog_next_time)>;
  explicit CancelCameraUploadDialog(ClickedCallback callback);
  CancelCameraUploadDialog(const CancelCameraUploadDialog&) = delete;
  CancelCameraUploadDialog& operator=(const CancelCameraUploadDialog&) = delete;
  ~CancelCameraUploadDialog();

 private:
  FRIEND_TEST_ALL_PREFIXES(ash::CameraSaveHandlerTest, CancelUpload);

  void OnCancelClicked(ui::DialogModelDelegate*);
  void OnCloseClicked(ui::DialogModelDelegate*);

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSkipDialogCheckboxId);
  ClickedCallback clicked_callback_;
  base::WeakPtr<views::Widget> widget_ = nullptr;
  base::WeakPtrFactory<CancelCameraUploadDialog> weak_ptr_factory_{this};
};

#endif  // CHROMEOS_ASH_EXPERIENCES_CAMERA_CANCEL_CAMERA_UPLOAD_DIALOG_H_
