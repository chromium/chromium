// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_DIALOG_FACTORY_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_DIALOG_FACTORY_H_

#include "base/functional/callback.h"
#include "components/enterprise/data_controls/core/browser/data_controls_dialog.h"

namespace content {
class WebContents;
}  // namespace content

namespace data_controls {

// Factory interface to create `DataControlDialog`s, which should have
// platform-specific implementations.
class DataControlsDialogFactory {
 public:
  // Entry point to be used to show a `DataControlDialog`. It's possible no
  // extra dialog will be shown in certain cases, for example if one already
  // exists for `type` in the current context.
  void ShowDialogIfNeeded(content::WebContents* web_contents,
                          DataControlsDialog::Type type,
                          base::OnceCallback<void(bool bypassed)> callback =
                              base::OnceCallback<void(bool bypassed)>());

 private:
  virtual DataControlsDialog* CreateDialog(
      DataControlsDialog::Type type,
      content::WebContents* web_contents,
      base::OnceCallback<void(bool bypassed)> callback) = 0;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_DIALOG_FACTORY_H_
