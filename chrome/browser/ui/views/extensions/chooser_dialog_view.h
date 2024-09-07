// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"

class DeviceChooserContentView;

namespace permissions {
class ChooserController;
}

// Displays a chooser view as a modal dialog constrained
// to the window/tab displaying the given web contents.
class ChooserDialogView : public views::DialogDelegateView,
                          public views::TableViewObserver {
  METADATA_HEADER(ChooserDialogView, views::DialogDelegateView)

 public:
  explicit ChooserDialogView(
      std::unique_ptr<permissions::ChooserController> chooser_controller);
  ChooserDialogView(const ChooserDialogView&) = delete;
  ChooserDialogView& operator=(const ChooserDialogView&) = delete;
  ~ChooserDialogView() override;

  // views::DialogDelegate:
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  DeviceChooserContentView* device_chooser_content_view_for_test() const {
    return device_chooser_content_view_;
  }

 private:
  raw_ptr<DeviceChooserContentView> device_chooser_content_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_
