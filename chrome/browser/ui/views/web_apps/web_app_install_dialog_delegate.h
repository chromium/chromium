// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_DELEGATE_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/web_apps/web_app_modal_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

class PrefService;

namespace feature_engagement {
class Tracker;
}

namespace webapps {
class MlInstallOperationTracker;
}  // namespace webapps

namespace views {
class Widget;
}  // namespace views

namespace gfx {
class Rect;
}  // namespace gfx

namespace web_app {

enum InstallDialogType { kSimple, kDetailed, kDiy, kMaxValue = kDiy };

std::ostream& operator<<(std::ostream& os, InstallDialogType type);

// Defines the maximum allowed width and height shrinkage (in pixels) from the
// preferred size of a dialog before it is considered too small/occluded and
// automatically closed to prevent UI spoofing.
struct MaxAllowedShrinkage {
  int max_width_shrinkage;
  int max_height_shrinkage;
};

inline constexpr MaxAllowedShrinkage kSimpleMaxShrinkage = {40, 20};
inline constexpr MaxAllowedShrinkage kDetailedMaxShrinkage = {100, 150};
inline constexpr MaxAllowedShrinkage kDiyMaxShrinkage = {50, 50};
inline constexpr MaxAllowedShrinkage kLaunchMaxShrinkage = {50, 50};

MaxAllowedShrinkage GetMaxAllowedShrinkage(InstallDialogType type);

// For some browser windows that are smaller in size, the install dialog's
// current size is smaller than the preferred size, leading to important
// security information being occluded. This function performs the comparison
// between the sizes and prevents that from happening.
// This serves as a stop-gap fix for crbug.com/384962294.
// TODO(crbug.com/346974105): Remove once tab modal dialogs can be sized
// irrespective of the size of the browser window triggering it.
bool IsWidgetCurrentSizeSmallerThanPreferredSize(views::Widget* widget,
                                                 MaxAllowedShrinkage shrinkage);

class WebAppInstallDialogDelegate : public WebAppModalDialogDelegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDiyAppsDialogOkButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDiyAppsDialogInputTextId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPwaInstallDialogInstallButton);
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kInstalledPWAEventId);

  WebAppInstallDialogDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> install_info,
      std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
      AppInstallationAcceptanceCallback callback,
      PwaInProductHelpState iph_state,
      PrefService* prefs,
      feature_engagement::Tracker* tracker,
      InstallDialogType dialog_type);

  ~WebAppInstallDialogDelegate() override;

  virtual void OnAccept();
  void OnCancel();
  void OnClose();

  virtual bool OnOkButtonClicked();

  // This is called when the dialog has been either accepted, cancelled, closed
  // or destroyed without an user-action.
  void OnDestroyed();

  // Takes care of enabling or disabling the dialog model's OK button for DIY
  // apps based on changes in the text field, and also keeps track of the text
  // field's contents.
  void OnTextFieldChangedMaybeUpdateButton(
      const std::u16string& text_field_contents);

  base::WeakPtr<WebAppInstallDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // views::WidgetObserver overrides:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  // WebAppModalDialogDelegate overrides:
  void CloseDialogAsIgnored() override;

  InstallDialogType dialog_type() { return dialog_type_; }

 protected:
  std::unique_ptr<WebAppInstallInfo> install_info_;

 private:
  void MeasureIphOnDialogClose();
  void MeasureAcceptUserActionsForInstallDialog();
  void MeasureCancelUserActionsForInstallDialog();

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker_;
  AppInstallationAcceptanceCallback callback_;
  PwaInProductHelpState iph_state_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<feature_engagement::Tracker> tracker_;
  InstallDialogType dialog_type_;
  std::u16string text_field_contents_;
  bool received_user_response_ = false;

  // Ensures the corresponding page action is highlighted, if any.
  // If the new page actions framework is enabled, then a
  // `ScopedPageActionActivity` is used.
  const std::optional<std::variant<views::Button::ScopedAnchorHighlight,
                                   page_actions::ScopedPageActionActivity>>
      page_action_highlight_;

  base::WeakPtrFactory<WebAppInstallDialogDelegate> weak_ptr_factory_{this};
};
}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_DELEGATE_H_
