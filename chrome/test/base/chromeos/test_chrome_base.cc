// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_chrome_base.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/test/base/chromeos/fake_ash_test_chrome_browser_main_extra_parts.h"
#include "content/public/browser/browser_main_parts.h"
#include "headless/public/headless_shell.h"
#include "ui/gfx/switches.h"

namespace test {

TestChromeBase::TestChromeBase(content::ContentMainParams params)
    : params_(std::move(params)) {
  auto created_main_parts_closure =
      base::BindOnce(&TestChromeBase::CreatedBrowserMainPartsImpl,
                     weak_ptr_factory_.GetWeakPtr());
  params_.created_main_parts_closure = std::move(created_main_parts_closure);
}

TestChromeBase::~TestChromeBase() = default;

int TestChromeBase::Start() {
  // Can only Start()'ed once.
  DCHECK(params_.created_main_parts_closure);

  int rv = 0;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless)) {
    rv = headless::HeadlessShellMain(std::move(params_));
  } else {
    rv = content::ContentMain(std::move(params_));
  }
  return rv;
}

void TestChromeBase::CreatedBrowserMainPartsImpl(
    content::BrowserMainParts* browser_main_parts) {
  browser_main_parts_ =
      static_cast<ChromeBrowserMainParts*>(browser_main_parts);
  CreateFakeAshTestChromeBrowserMainExtraParts();
}

void TestChromeBase::CreateFakeAshTestChromeBrowserMainExtraParts() {
  browser_main_parts_->AddParts(
      std::make_unique<test::FakeAshTestChromeBrowserMainExtraParts>());
}

}  // namespace test
