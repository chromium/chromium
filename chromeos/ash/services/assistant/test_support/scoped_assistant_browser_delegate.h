// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_BROWSER_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_BROWSER_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"

namespace ash::assistant {

// A base testing implementation of the AssistantBrowserDelegate interface which
// tests can subclass to implement specific client mocking support. It also
// installs itself as the singleton instance.
class ScopedAssistantBrowserDelegate : AssistantBrowserDelegate {
 public:
  ScopedAssistantBrowserDelegate();
  ~ScopedAssistantBrowserDelegate() override;

  AssistantBrowserDelegate& Get();

  void SetOpenNewEntryPointClosure(base::OnceClosure closure);

  void OpenUrl(GURL url) override;
  base::expected<bool, AssistantBrowserDelegate::Error>
  IsNewEntryPointEligibleForPrimaryProfile() override;
  void OpenNewEntryPoint() override;
  std::optional<std::string> GetNewEntryPointName() override;

 private:
  base::OnceClosure open_new_entry_point_closure_;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_BROWSER_DELEGATE_H_
