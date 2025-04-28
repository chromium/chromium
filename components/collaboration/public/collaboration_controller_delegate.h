// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_CONTROLLER_DELEGATE_H_
#define COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_CONTROLLER_DELEGATE_H_

#include "base/functional/callback.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace collaboration {

// The class responsible for controlling actions on platform specific UI
// elements. This delegate is required by the CollaborationController.
class CollaborationControllerDelegate {
 public:
  struct ErrorInfo {
    enum class Type {
      kUnknown = 0,
      // Show the generic error dialog.
      kGenericError = 1,
      // Show the link invalid error dialog.
      kInvalidUrl = 2,
      // Show the error when Entreprise has disabled sign in.
      kSigninDisabledByPolicy = 3,
      // Show the error when Entreprise disabled sync.
      kSyncDisabledByPolicy = 4,
      // Show the group full error dialog.
      kGroupFull = 5,
      // Show the group closed error dialog.
      kGroupClosedByOrganizationPolicy = 6,
    };

    explicit ErrorInfo(Type type) : type_(type) { GetStringForErrorType(); }

    bool operator==(const ErrorInfo& other) const {
      return type_ == other.type_;
    }

    std::string error_header;
    std::string error_body;
    Type type() const { return type_; }

    std::string GetLogString() const {
      switch (type_) {
        case Type::kUnknown:
          return "Unknown";
        case Type::kGenericError:
          return "Generic Error";
        case Type::kInvalidUrl:
          return "Invalid Url";
        case Type::kSyncDisabledByPolicy:
          return "Sync Disabled By Policy";
        case Type::kSigninDisabledByPolicy:
          return "Signin Disabled By Policy";
        case Type::kGroupFull:
          return "Group Is Full";
        case Type::kGroupClosedByOrganizationPolicy:
          return "Group Is Closed By Organization Policy";
      }
    }

   private:
    void GetStringForErrorType() {
      switch (type_) {
        case Type::kInvalidUrl:
          error_header =
              l10n_util::GetStringUTF8(IDS_COLLABORATION_LINK_FAILED_HEADER);
          error_body =
              l10n_util::GetStringUTF8(IDS_COLLABORATION_LINK_FAILED_BODY);
          break;
        case Type::kSyncDisabledByPolicy:
          error_header = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_ENTREPRISE_SYNC_DISABLED_HEADER);
          error_body = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_ENTREPRISE_SYNC_DISABLED_BODY);
          break;
        case Type::kSigninDisabledByPolicy:
          error_header = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_ENTREPRISE_SIGNIN_DISABLED_HEADER);
          error_body = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_ENTREPRISE_SIGNIN_DISABLED_BODY);
          break;
        case Type::kGroupFull:
          error_header = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_GROUP_IS_FULL_ERROR_DIALOG_HEADER);
          error_body = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_GROUP_IS_FULL_ERROR_DIALOG_BODY);
          break;
        case Type::kGroupClosedByOrganizationPolicy:
          error_header = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_ENTREPRISE_GROUP_CLOSED_HEADER);
          error_body = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_ENTREPRISE_GROUP_CLOSED_BODY);
          break;
        case Type::kGenericError:
        case Type::kUnknown:
          error_header = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_SOMETHING_WENT_WRONG_HEADER);
          error_body = l10n_util::GetStringUTF8(
              IDS_COLLABORATION_SOMETHING_WENT_WRONG_BODY);
      };
    }

    Type type_;
  };

  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.components.collaboration)
  enum class Outcome {
    kSuccess = 0,
    kFailure = 1,
    kCancel = 2,
    kGroupLeftOrDeleted = 3,
  };

  CollaborationControllerDelegate() = default;
  virtual ~CollaborationControllerDelegate() = default;

  // Disallow copy/assign.
  CollaborationControllerDelegate(const CollaborationControllerDelegate&) =
      delete;
  CollaborationControllerDelegate& operator=(
      const CollaborationControllerDelegate&) = delete;

  // Callback for informing the service whether a the UI was displayed
  // successfully.
  using ResultCallback = base::OnceCallback<void(Outcome)>;
  using ResultWithGroupTokenCallback =
      base::OnceCallback<void(CollaborationControllerDelegate::Outcome,
                              std::optional<data_sharing::GroupToken>)>;

  // Request to initialize UI.
  virtual void PrepareFlowUI(base::OnceCallback<void()> exit_callback,
                             ResultCallback result) = 0;

  // Request to show the error page/dialog.
  virtual void ShowError(const ErrorInfo& error, ResultCallback result) = 0;

  // Request to cancel and close the current UI screen.
  virtual void Cancel(ResultCallback result) = 0;

  // Request to show the authentication screen for the current `flow_type`.
  virtual void ShowAuthenticationUi(FlowType flow_type,
                                    ResultCallback result) = 0;

  // Notification for when sign-in or sync status has been updated to ensure
  // that the update propagated to all relevant components.
  virtual void NotifySignInAndSyncStatusChange() = 0;

  // Request to show the invitation dialog with preview data.
  virtual void ShowJoinDialog(
      const data_sharing::GroupToken& token,
      const data_sharing::SharedDataPreview& preview_data,
      ResultCallback result) = 0;

  // Request to show the share dialog.
  virtual void ShowShareDialog(const tab_groups::EitherGroupID& either_id,
                               ResultWithGroupTokenCallback result) = 0;

  // Request to show the share sheet after the share dialog successfully creates
  // the shared tab group.
  virtual void OnUrlReadyToShare(const data_sharing::GroupId& group_id,
                                 const GURL& url,
                                 ResultCallback result) = 0;

  // Request to show the manage dialog.
  virtual void ShowManageDialog(const tab_groups::EitherGroupID& either_id,
                                ResultCallback result) = 0;

  // Request to show the leave dialog.
  virtual void ShowLeaveDialog(const tab_groups::EitherGroupID& either_id,
                               ResultCallback result) = 0;

  // Request to show the delete dialog.
  virtual void ShowDeleteDialog(const tab_groups::EitherGroupID& either_id,
                                ResultCallback result) = 0;

  // Open the local tab group associated with `group_id` in UI.
  virtual void PromoteTabGroup(const data_sharing::GroupId& group_id,
                               ResultCallback result) = 0;

  // Focus the UI screen associated with the current delegate instance.
  virtual void PromoteCurrentScreen() = 0;

  // Called when the flow is finished so the delegate instance can clean up
  // itself.
  virtual void OnFlowFinished() = 0;

#if BUILDFLAG(IS_ANDROID)
  // Returns the Java object of the CollaborationControllerDelegate.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_CONTROLLER_DELEGATE_H_
