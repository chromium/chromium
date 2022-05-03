// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/tab_helper.h"

#include "base/time/default_tick_clock.h"
#include "components/autofill_assistant/browser/desktop/starter_delegate_desktop.h"
#include "components/autofill_assistant/browser/starter.h"

namespace autofill_assistant {

void CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<CommonDependencies> common_dependencies,
    std::unique_ptr<PlatformDependencies> platform_dependencies) {
  StarterDelegateDesktop::CreateForWebContents(
      web_contents, std::move(common_dependencies),
      std::move(platform_dependencies));
  auto starter_delegate =
      StarterDelegateDesktop::FromWebContents(web_contents)->GetWeakPtr();

  Starter::CreateForWebContents(
      web_contents, starter_delegate->GetWeakPtr(), ukm::UkmRecorder::Get(),
      RuntimeManagerImpl::GetForWebContents(web_contents)->GetWeakPtr(),
      base::DefaultTickClock::GetInstance());
}

}  // namespace autofill_assistant
