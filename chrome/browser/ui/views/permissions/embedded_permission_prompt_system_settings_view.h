// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_SYSTEM_SETTINGS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_SYSTEM_SETTINGS_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

// A view used to display information to the user that they need to go to OS
// system settings and grant permission to Chrome, in order to use that
// permission on the site the user is visiting.
class EmbeddedPermissionPromptSystemSettingsView
    : public EmbeddedPermissionPromptBaseView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOpenSettingsId);

  EmbeddedPermissionPromptSystemSettingsView(
      content::WebContents* web_contents,
      base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate);
  EmbeddedPermissionPromptSystemSettingsView(
      const EmbeddedPermissionPromptSystemSettingsView&) = delete;
  EmbeddedPermissionPromptSystemSettingsView& operator=(
      const EmbeddedPermissionPromptSystemSettingsView&) = delete;
  ~EmbeddedPermissionPromptSystemSettingsView() override;

  std::u16string GetAccessibleWindowTitle() const override;
  std::u16string GetWindowTitle() const override;
  void RunButtonCallback(int type) override;

  // EmbeddedPermissionPromptBaseView
  void OnWidgetTreeActivated(views::Widget* root_widget,
                             views::Widget* active_widget) override;

 protected:
  std::vector<RequestLineConfiguration> GetRequestLinesConfiguration()
      const override;
  std::vector<ButtonConfiguration> GetButtonsConfiguration() const override;

 private:
  void OnPermissionChecksDone(const std::vector<bool>& results);
  void NotifyDelegatePermissionNoLongerDenied();

  bool prompt_resolved_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      host_widget_observation_{this};
  base::WeakPtrFactory<EmbeddedPermissionPromptSystemSettingsView>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_SYSTEM_SETTINGS_VIEW_H_
