// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/mock_permission_prompt_factory.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

MockPermissionPromptFactory::MockPermissionPromptFactory(
    PermissionRequestManager* manager)
    : show_count_(0),
      requests_count_(0),
      response_type_(PermissionRequestManager::NONE),
      manager_(manager) {
  manager->set_view_factory_for_testing(base::BindRepeating(
      &MockPermissionPromptFactory::Create, base::Unretained(this)));
}

MockPermissionPromptFactory::~MockPermissionPromptFactory() {
  manager_->set_view_factory_for_testing(
      base::BindRepeating(&MockPermissionPromptFactory::DoNotCreate));
  for (permissions::MockPermissionPrompt* prompt : prompts_) {
    prompt->factory_ = nullptr;
  }
  prompts_.clear();
}

std::unique_ptr<PermissionPrompt> MockPermissionPromptFactory::Create(
    content::WebContents* web_contents,
    PermissionPrompt::Delegate* delegate) {
  MockPermissionPrompt* prompt = new MockPermissionPrompt(this, delegate);

  prompts_.push_back(prompt);
  show_count_++;
  requests_count_ = delegate->Requests().size();
  for (const PermissionRequest* request : delegate->Requests()) {
    request_types_seen_.push_back(request->request_type());
    request_origins_seen_.push_back(request->requesting_origin());
  }

  if (!show_bubble_quit_closure_.is_null())
    show_bubble_quit_closure_.Run();

  manager_->set_auto_response_for_test(response_type_);
  return base::WrapUnique(prompt);
}

void MockPermissionPromptFactory::ResetCounts() {
  show_count_ = 0;
  requests_count_ = 0;
  request_types_seen_.clear();
  request_origins_seen_.clear();
}

void MockPermissionPromptFactory::DocumentOnLoadCompletedInPrimaryMainFrame() {
  manager_->DocumentOnLoadCompletedInPrimaryMainFrame();
}

bool MockPermissionPromptFactory::is_visible() {
  return !prompts_.empty();
}

int MockPermissionPromptFactory::TotalRequestCount() {
  return request_types_seen_.size();
}

bool MockPermissionPromptFactory::RequestTypeSeen(RequestType type) {
  return base::Contains(request_types_seen_, type);
}

bool MockPermissionPromptFactory::RequestOriginSeen(const GURL& origin) {
  return base::Contains(request_origins_seen_, origin);
}

void MockPermissionPromptFactory::WaitForPermissionBubble() {
  if (is_visible())
    return;
  DCHECK(show_bubble_quit_closure_.is_null());
  base::RunLoop loop;
  show_bubble_quit_closure_ = loop.QuitClosure();
  loop.Run();
  show_bubble_quit_closure_ = base::RepeatingClosure();
}

// static
std::unique_ptr<PermissionPrompt> MockPermissionPromptFactory::DoNotCreate(
    content::WebContents* web_contents,
    PermissionPrompt::Delegate* delegate) {
  NOTREACHED_IN_MIGRATION();
  return base::WrapUnique(new MockPermissionPrompt(nullptr, nullptr));
}

void MockPermissionPromptFactory::HideView(MockPermissionPrompt* prompt) {
  auto it = base::ranges::find(prompts_, prompt);
  if (it != prompts_.end())
    prompts_.erase(it);
}

}  // namespace permissions
