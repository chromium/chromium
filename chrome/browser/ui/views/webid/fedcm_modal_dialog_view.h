// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webid/identity_dialog_controller.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

// A dialog allowing the user to complete a flow (e.g. signing in to an identity
// provider) prompted by FedCM.
// TODO(crbug.com/40263254): Rename modal dialog to pop-up window.
class FedCmModalDialogView : public content::WebContentsObserver {
 public:
  class Observer {
   public:
    // Tells observers that the pop-up window is destroyed.
    virtual void OnPopupWindowDestroyed() = 0;
  };

  // This enum describes the outcome of attempting to open the pop-up window and
  // is used for histograms. Do not remove or modify existing values, but you
  // may add new values at the end. This enum should be kept in sync with
  // FedCmShowPopupWindowResult in tools/metrics/histograms/enums.xml.
  enum class ShowPopupWindowResult {
    kSuccess,
    kFailedByInvalidUrl,
    kFailedForOtherReasons,

    kMaxValue = kFailedForOtherReasons
  };

  // This enum describes the reason for closing the pop-up window and is used
  // for histograms. Do not remove or modify existing values, but you may add
  // new values at the end. This enum should be kept in sync with
  // FedCmClosePopupWindowReason in tools/metrics/histograms/enums.xml.
  enum class ClosePopupWindowReason {
    kIdpInitiatedClose,
    kPopupWindowDestroyed,

    kMaxValue = kPopupWindowDestroyed
  };

  // This enum describes the reason for closing the pop-up window and is used
  // for histograms. Do not remove or modify existing values, but you may add
  // new values at the end. This enum should be kept in sync with
  // FedCmPopupInteraction in tools/metrics/histograms/enums.xml.
  enum class PopupInteraction {
    kLosesFocusAndIdpInitiatedClose,
    kLosesFocusAndPopupWindowDestroyed,
    kNeverLosesFocusAndIdpInitiatedClose,
    kNeverLosesFocusAndPopupWindowDestroyed,

    kMaxValue = kNeverLosesFocusAndPopupWindowDestroyed
  };

  explicit FedCmModalDialogView(content::WebContents* web_contents,
                                FedCmModalDialogView::Observer* observer);
  FedCmModalDialogView(const FedCmModalDialogView&) = delete;
  FedCmModalDialogView& operator=(const FedCmModalDialogView&) = delete;
  ~FedCmModalDialogView() override;

  // Shows a modal dialog of |url|. The |url| is commonly but not limited to a
  // URL which allows the user to sign in with an identity provider. Virtual for
  // testing purposes.
  virtual content::WebContents* ShowPopupWindow(const GURL& url);
  virtual void ClosePopupWindow();
  virtual void ResizeAndFocusPopupWindow();
  virtual void SetCustomYPosition(int y);
  virtual void SetActiveModeSheetType(
      AccountSelectionView::SheetType sheet_type);

  // content::WebContentsObserver
  void WebContentsDestroyed() override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;

 protected:
  Observer* GetObserverForTesting();

 private:
  raw_ptr<content::WebContents> source_window_{nullptr};
  raw_ptr<content::WebContents> popup_window_{nullptr};
  raw_ptr<Observer> observer_{nullptr};

  // If set, this will be the y-coordinate position of the pop-up window.
  // Otherwise, the pop-up window is centred vertically and horizontally. Used
  // to position the pop-up window directly over the active mode modal dialog.
  std::optional<int> custom_y_position_;

  // Whether one of Blink.FedCm.Button.LoadingStatePopupInteraction or
  // Blink.FedCm.Button.UseOtherAccountPopupInteraction has been recorded. This
  // bool prevents double counting because user closing the pop-up causes both
  // `ClosePopupWindow` and `WebContentsDestroyed` to be called.
  bool popup_interaction_metric_recorded_{false};

  // The sheet type of the active mode dialog which opened this pop-up.
  // `std::nullopt` for non-active mode cases.
  std::optional<AccountSelectionView::SheetType> active_mode_sheet_type_;

  // Number of times the user lost focus of the pop-up. i.e. number of times
  // `OnWebContentsLostFocus` is called. This is an int because when the user
  // closes the pop-up, the web contents loses focus before it gets destroyed so
  // there is one lost focus event that is not from the user losing focus while
  // the pop-up is open.
  int num_lost_focus_{0};

  base::WeakPtrFactory<FedCmModalDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_
