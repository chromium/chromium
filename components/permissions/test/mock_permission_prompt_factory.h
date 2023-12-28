// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_FACTORY_H_
#define COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace permissions {
class MockPermissionPrompt;
enum class RequestType;

// Provides a skeleton class for both unit and browser testing when trying to
// test the bubble manager logic. Should not be used for anything that requires
// actual UI.
// See example usage in
// chrome/browser/permissions/permission_request_manager_unittest.cc
class MockPermissionPromptFactory {
 public:
  explicit MockPermissionPromptFactory(PermissionRequestManager* manager);

  MockPermissionPromptFactory(const MockPermissionPromptFactory&) = delete;
  MockPermissionPromptFactory& operator=(const MockPermissionPromptFactory&) =
      delete;

  ~MockPermissionPromptFactory();

  // Create method called by the PBM to show a bubble.
  std::unique_ptr<PermissionPrompt> Create(
      content::WebContents* web_contents,
      PermissionPrompt::Delegate* delegate);

  void ResetCounts();

  void DocumentOnLoadCompletedInPrimaryMainFrame();

  void set_response_type(PermissionRequestManager::AutoResponseType type) {
    response_type_ = type;
  }

  PermissionRequestManager::AutoResponseType response_type() {
    return response_type_;
  }

  // If the current view is visible.
  bool is_visible();
  // Number of times |Show| was called on any bubble.
  int show_count() { return show_count_; }
  // Number of requests seen by the last |Show|.
  int request_count() { return requests_count_; }
  // Number of requests seen.
  int TotalRequestCount();
  // Whether the specified permission was shown in a prompt.
  bool RequestTypeSeen(RequestType type);
  // Whether a prompt with the given origin was shown.
  bool RequestOriginSeen(const GURL& origin);

  void WaitForPermissionBubble();

 private:
  friend class MockPermissionPrompt;

  // This shouldn't be called. Is here to fail tests that try to create a bubble
  // after the factory has been destroyed.
  static std::unique_ptr<PermissionPrompt> DoNotCreate(
      content::WebContents* web_contents,
      PermissionPrompt::Delegate* delegate);

  void HideView(MockPermissionPrompt* view);

  int show_count_;
  int requests_count_;
  std::vector<RequestType> request_types_seen_;
  std::vector<GURL> request_origins_seen_;

  std::vector<raw_ptr<MockPermissionPrompt, VectorExperimental>> prompts_;
  PermissionRequestManager::AutoResponseType response_type_;

  base::RepeatingClosure show_bubble_quit_closure_;

  // The bubble manager that will be associated with this factory.
  raw_ptr<PermissionRequestManager> manager_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_FACTORY_H_
