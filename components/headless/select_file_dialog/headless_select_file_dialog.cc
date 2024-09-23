// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/select_file_dialog/headless_select_file_dialog.h"

#include "base/memory/ptr_util.h"
#include "ui/gfx/native_widget_types.h"

namespace headless {

// static
HeadlessSelectFileDialogFactory* HeadlessSelectFileDialogFactory::instance_ =
    nullptr;

// HeadlessSelectFileDialog implements a stub select file dialog
// that cancels itself as soon as it gets open, optionally calling
// back the owner.

class HeadlessSelectFileDialog : public ui::SelectFileDialog {
 public:
  HeadlessSelectFileDialog(Listener* listener,
                           std::unique_ptr<ui::SelectFilePolicy> policy,
                           SelectFileDialogCallback callback)
      : ui::SelectFileDialog(listener, std::move(policy)),
        callback_(std::move(callback)) {}

 protected:
  ~HeadlessSelectFileDialog() override = default;

  // ui::BaseShellDialog
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }

  void ListenerDestroyed() override { listener_ = nullptr; }

  // ui::SelectFileDialog
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    if (callback_) {
      std::move(callback_).Run(type);
    }

    if (listener_) {
      listener_->FileSelectionCanceled();
    }
  }

 private:
  // ui::SelectFileDialog:
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

  SelectFileDialogCallback callback_;
};

// HeadlessSelectFileDialogFactory creates cancelable SelectFileDialog's

// static
void HeadlessSelectFileDialogFactory::SetUp() {
  ui::SelectFileDialog::SetFactory(
      // Private constructor.
      base::WrapUnique(new HeadlessSelectFileDialogFactory()));
}

// static
void HeadlessSelectFileDialogFactory::SetSelectFileDialogOnceCallbackForTests(
    SelectFileDialogCallback callback) {
  DCHECK(instance_);
  instance_->callback_ = std::move(callback);
}

ui::SelectFileDialog* HeadlessSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new HeadlessSelectFileDialog(listener, std::move(policy),
                                      std::move(callback_));
}

HeadlessSelectFileDialogFactory::HeadlessSelectFileDialogFactory() {
  DCHECK(!instance_);
  instance_ = this;
}

HeadlessSelectFileDialogFactory::~HeadlessSelectFileDialogFactory() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

}  // namespace headless
