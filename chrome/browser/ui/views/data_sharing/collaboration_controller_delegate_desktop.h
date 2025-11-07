// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATA_SHARING_COLLABORATION_CONTROLLER_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_DATA_SHARING_COLLABORATION_CONTROLLER_DELEGATE_DESKTOP_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/tab_groups/tab_group_id.h"

class Browser;
class BrowserWindowInterface;

namespace views {
class Widget;
}

namespace collaboration {
class CollaborationService;
struct ServiceStatus;
}  // namespace collaboration

class CollaborationControllerDelegateDesktop
    : public collaboration::CollaborationControllerDelegate,
      public BrowserListObserver {
 public:
  explicit CollaborationControllerDelegateDesktop(
      Browser* browser,
      std::optional<data_sharing::FlowType> flow = std::nullopt);
  ~CollaborationControllerDelegateDesktop() override;

  void PrepareFlowUI(base::OnceCallback<void()> exit_callback,
                     ResultCallback result) override;
  void ShowError(const ErrorInfo& error, ResultCallback result) override;
  void Cancel(ResultCallback result) override;
  void ShowAuthenticationUi(collaboration::FlowType flow_type,
                            ResultCallback result) override;
  void NotifySignInAndSyncStatusChange() override;
  void ShowJoinDialog(const data_sharing::GroupToken& token,
                      const data_sharing::SharedDataPreview& preview_data,
                      ResultCallback result) override;
  void ShowShareDialog(const tab_groups::EitherGroupID& either_id,
                       ResultWithGroupTokenCallback result) override;
  void OnUrlReadyToShare(const data_sharing::GroupId& group_id,
                         const GURL& url,
                         ResultCallback result) override;
  void ShowManageDialog(const tab_groups::EitherGroupID& either_id,
                        ResultCallback result) override;
  void ShowLeaveDialog(const tab_groups::EitherGroupID& either_id,
                       ResultCallback result) override;
  void ShowDeleteDialog(const tab_groups::EitherGroupID& either_id,
                        ResultCallback result) override;
  void PromoteTabGroup(const data_sharing::GroupId& group_id,
                       ResultCallback result) override;
  void PromoteCurrentScreen() override;
  void OnFlowFinished() override;

  views::Widget* prompt_dialog_widget_for_testing() {
    return prompt_dialog_widget_;
  }

  views::Widget* error_dialog_widget_for_testing() {
    return error_dialog_widget_;
  }

 protected:
  virtual collaboration::ServiceStatus GetServiceStatus();

 private:
  // Called when browser closed via RegisterBrowserDidClose callback.
  void OnBrowserDidClose(BrowserWindowInterface* browser_window_interface);

  void OnManageDialogClosing(
      ResultCallback result,
      std::optional<data_sharing::mojom::GroupAction> action,
      std::optional<data_sharing::mojom::GroupActionProgress> progress);

  void ShowErrorDialog(const ErrorInfo& error);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void MaybeShowSignInUiForHistorySyncOptin();
#endif
  void MaybeShowSignInAndSyncUi();
  void MaybeShowSignInOrSyncPromptDialog();
  void OnPromptDialogOk();
  void OnPromptDialogCancel();
  void OnErrorDialogOkForUpdate();
  void OnErrorDialogOk();
  void MaybeCloseDialogs();
  void ExitFlow();

  // The browser this delegate shows UI on.
  raw_ptr<Browser> browser_;

  // The flow of this delegate. Only needed to set to distinguish kLeave,
  // kDelete and kRemoveLastTab flows.
  std::optional<data_sharing::FlowType> flow_;

  // Collaboration service to query sign in and sync status.
  raw_ptr<collaboration::CollaborationService> collaboration_service_;

  // Widget of the prompt dialog.
  raw_ptr<views::Widget> prompt_dialog_widget_;

  // Widget of the error dialog.
  raw_ptr<views::Widget> error_dialog_widget_;

  // Exit callback to quit the flow.
  base::OnceCallback<void()> exit_callback_;

  // Callback passed from `ShowAuthenticationUi()`.
  ResultCallback authentication_ui_callback_;

  // Callback passed from `ShowError()`.
  ResultCallback error_ui_callback_;

  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::kCollaborationShareTabGroup;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observer_{this};

  // Subscription for browser closed callback.
  base::CallbackListSubscription browser_close_subscription_;

  base::WeakPtrFactory<CollaborationControllerDelegateDesktop>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DATA_SHARING_COLLABORATION_CONTROLLER_DELEGATE_DESKTOP_H_
