// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
#define COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_HOST_H_

#include "components/web_modal/web_contents_modal_dialog_host.h"

#include "base/compiler_specific.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace web_modal {

class TestWebContentsModalDialogHost : public WebContentsModalDialogHost {
 public:
  explicit TestWebContentsModalDialogHost(gfx::NativeView host_view);

  TestWebContentsModalDialogHost(const TestWebContentsModalDialogHost&) =
      delete;
  TestWebContentsModalDialogHost& operator=(
      const TestWebContentsModalDialogHost&) = delete;

  ~TestWebContentsModalDialogHost() override;

  // WebContentsModalDialogHost:
  gfx::Size GetMaximumDialogSize() override;
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  void AddObserver(ModalDialogHostObserver* observer) override;
  void RemoveObserver(ModalDialogHostObserver* observer) override;

  void set_max_dialog_size(const gfx::Size& max_dialog_size) {
    max_dialog_size_ = max_dialog_size;
  }

 private:
  gfx::NativeView host_view_;
  gfx::Size max_dialog_size_;
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
