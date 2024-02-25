// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include <string>

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace javascript_dialogs {

class AppModalDialogManagerDelegate {
 public:
  virtual ~AppModalDialogManagerDelegate() = default;

  // Returns a displayable title associated with the origin for the JavaScript
  // Dialog.
  virtual std::u16string GetTitle(content::WebContents* web_contents,
                                  const url::Origin& origin) = 0;
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_APP_MODAL_DIALOG_MANAGER_DELEGATE_H_
