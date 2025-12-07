// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_WEB_CONTENT_HANDLER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_WEB_CONTENT_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

class GURL;
namespace supervised_user {

class SupervisedUserSettingsService;

// This base class contains all the Web Approval Intersitial functionality that
// requires access to the current web content.
// It contains implementation of the common methods that can be shared accross
// platforms and can live in components.
class WebContentHandler {
 public:
  using ApprovalRequestInitiatedCallback = base::OnceCallback<void(bool)>;

  virtual ~WebContentHandler();

  // Initiates the OS specific local approval flow for a given `url`.
  // Not all platforms with supervised users support this operation,
  // and they must throw an error when implementing this method.
  virtual void RequestLocalApproval(
      const GURL& url,
      const std::u16string& child_display_name,
      const UrlFormatter& url_formatter,
      const FilteringBehaviorReason& filtering_behavior_reason,
      ApprovalRequestInitiatedCallback callback) = 0;

  // TODO(b/273692421): Add unit (or browser test) coverage for the moved
  // methods that currently have no test coverage.

  // Returns true if the given frame is the primary main frame for the active
  // page.
  virtual bool IsMainFrame() const = 0;

  // Removes all the infobars which are attached to web contents
  // and for which ShouldExpire() returns true, if the navigation frame id
  // is the main frame.
  virtual void CleanUpInfoBarOnMainFrame() = 0;

  // Goes back to main frame if we are on a subframe.
  // The action applies when localWebApprovalsEnabled is disabled.
  virtual void GoBack() = 0;

#if BUILDFLAG(IS_ANDROID)
  // Opens a resource with additional information on what is currently
  // displayed (typically, some help center article).
  virtual void LearnMore(base::OnceClosure open_help_page) = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  // Returns the interstitial navigation id.
  virtual int64_t GetInterstitialNavigationId() const = 0;

  // Closes the local approval widget if it is on-screen.
  virtual void MaybeCloseLocalApproval() = 0;

  static const char* GetLocalApprovalDurationMillisecondsHistogram();
  static const char* GetLocalApprovalResultHistogram();

 protected:
  WebContentHandler();

  // Records the outcome of the local web approval flow.
  void RecordLocalWebApprovalResultMetric(LocalApprovalResult approval_result);

  // Processes the outcome of the local approval request.
  // Should be called by platform specific completion callback.
  // TODO(b/278079069): Refactor and convert the class to an interface.
  void OnLocalApprovalRequestCompleted(
      supervised_user::SupervisedUserSettingsService& settings_service,
      const GURL& url,
      base::TimeTicks start_time,
      LocalApprovalResult approval_result,
      std::optional<supervised_user::LocalWebApprovalErrorType>
          local_approval_error_type);
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_WEB_CONTENT_HANDLER_H_
