// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_WEB_CONTENT_HANDLER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_WEB_CONTENT_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"

class GURL;
namespace supervised_user {

class SupervisedUserSettingsService;

// This class contains all the Web Approval Intersitial functionality that
// requires access to the current web content.
class WebContentHandler {
 public:
  using ApprovalRequestInitiatedCallback = base::OnceCallback<void(bool)>;

  // The result of local web approval flow.
  // Used for metrics. Those values are logged to UMA. Entries should not be
  // renumbered and numeric values should never be reused.
  // Matches the enum "FamilyLinkUserLocalWebApprovalResult" in
  // src/tools/metrics/histograms/enums.xml.
  // LINT.IfChange
  enum class LocalApprovalResult {
    kApproved = 0,
    kDeclined = 1,
    kCanceled = 2,
    kError = 3,
    kMaxValue = kError
  };
  // LINT.ThenChange(
  //     //tools/metrics/histograms/enums.xml
  // )

  virtual ~WebContentHandler();

  // Initiates the OS specific local approval flow for a given `url`.
  virtual void RequestLocalApproval(
      const GURL& url,
      const std::u16string& child_display_name,
      ApprovalRequestInitiatedCallback callback) = 0;
  // Returns true if the given frame is the primary main frame for the active
  // page.
  // TODO(b/273692421): Once all content-based methods are moved from the
  // interstitial, frame_id will be moved from into WebContentHandler.
  // WebContentHandler.
  virtual bool IsMainFrame(int frame_id) = 0;
  // Shows the feedback page to the user.
  // TODO(b/276428131): Remove when local we approvals if fully launched.
  virtual void ShowFeedback(GURL url, std::u16string reason) = 0;

  // Removes all the infobars which are attached to web_contents_
  // and for which ShouldExpire() returns true, if the navigation frame_id
  // is the main frame.
  // TODO(b/273692421): frame_id will be moved from the interstitial into
  // WebContentHandler.
  // TODO(b/273692421): Add unit (or browser test) coverage for the moved
  // methods that currently have no test coverage.
  virtual void CleanUpInfoBarOnMainFrame(int frame_id) = 0;

  static const char* GetLocalApprovalDurationMillisecondsHistogram();
  static const char* GetLocalApprovalResultHistogram();

 protected:
  // Processes the outcome of the local approval request.
  // Shared between the platforms. Should be called by platform specific
  // completion callback.
  void OnLocalApprovalRequestCompleted(
      supervised_user::SupervisedUserSettingsService& settings_service,
      const GURL& url,
      base::TimeTicks start_time,
      LocalApprovalResult approval_result);
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_WEB_CONTENT_HANDLER_H_
